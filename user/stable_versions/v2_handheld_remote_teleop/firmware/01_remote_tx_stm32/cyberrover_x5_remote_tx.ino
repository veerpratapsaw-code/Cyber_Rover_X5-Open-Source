/*
 * ================================================================================
 * CYBERROVER X5 — 13-CHANNEL REMOTE TRANSMITTER FIRMWARE v1.5 (HUD OS)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras, Jharkhand
 * Platform       : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Handheld Remote Controller Transmitter Node
 * Display Target : 1.3" I2C OLED (SH1106 @ PB6/PB7) via U8g2 Full Framebuffer
 *
 * UPGRADES & INDUSTRIAL FIXES (v1.5):
 *   - SOUND MUTE SWITCH (PB4 / Toggle Right): Flipping Toggle Right ON immediately
 *     mutes the piezo buzzer (0 dB emission) for silent late-night bench testing.
 *     Haptic tactile feedback remains active, and a small 'M' indicator appears on HUD.
 *   - 3-STAGE ANALOG NOISE REJECTION: Order-3 Median Filter + Single-Pole Low-Pass
 *     IIR Filter + Zero-Discontinuity Deadband Normalization on all 7 analog channels.
 *   - POTENTIOMETER HYSTERESIS (Delta > 65 counts): Eliminates boundary chatter and
 *     repetitive false notch alert beeps.
 *   - EXPLICIT STM32 HARDWARE I2C & SPI MAPPING: Wire.setSCL(PB6), Wire.setSDA(PB7),
 *     Fast 400 kHz I2C, SPI.begin() on SPI1 (PA5-PA7, CE=PB10, CSN=PB11).
 *   - 24-BYTE PACKED LITTLE-ENDIAN PROTOCOL: Mutually synchronized with Rover node.
 * ================================================================================
 * ARCHIVED NRF24L01+PA+LNA 2.4 GHz RF RESEARCH & CONFIGURATION (REACTIVE ARCHIVE):
 * (Preserved for future dual-radio redundant failover implementation)
 * Hardware Pinout on STM32F103C8T6 (SPI1):
 *   - SCK  : PA5 (Hardware SPI1 Clock)
 *   - MISO : PA6 (Hardware SPI1 MISO)
 *   - MOSI : PA7 (Hardware SPI1 MOSI)
 *   - CE   : PB10 (Radio Chip Enable)
 *   - CSN  : PB11 (Radio Chip Select Not)
 *   - IRQ  : Floating / Unconnected
 *   - VCC  : 3.3V (Dedicated rail with 10uF tantalum + 0.1uF ceramic decoupling capacitor)
 *   - GND  : System Star Ground
 *
 * Optimal RF Registers (Empirically Validated):
 *   - RF Channel   : 108 (2508 MHz — safely above standard 2.4GHz Wi-Fi channels 1-13)
 *   - Data Rate    : RF24_250KBPS (Ultra-high receiver sensitivity: -94 dBm)
 *   - Power Level  : RF24_PA_LOW (Bench testing) / RF24_PA_MAX (Long-range field telemetry)
 *   - Auto-Ack     : Enabled with 1500us retry delay, 2 retries (low latency, zero stalling)
 *   - Pipe Address : 0xE8E8F0F0E1LL (CyberRover X5 Unique Pipe)
 *   - Payload Size : 24 Bytes (Packed Little-Endian RoverPacket)
 * ================================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include "boot_bitmaps.h"

// =============================================================================
//  ALERT EVENT TYPES & FORWARD DECLARATIONS
// =============================================================================
enum AlertEvent {
  ALERT_NONE,
  ALERT_LINK_HANDSHAKE,
  ALERT_TOGGLE_ON,
  ALERT_TOGGLE_OFF,
  ALERT_BUTTON_CLICK,
  ALERT_POT_NOTCH,
  ALERT_TURBO_KICK,
  ALERT_LINK_LOST
};

void triggerAlert(AlertEvent evt);
void setOutputs(bool buzzer, bool haptic);
void updateAlertEngine();
void playStartupConfirmation();
void playBootAnimation();
void renderSpectrumAnalyzer(uint16_t lx, uint16_t ly, uint16_t rx, uint16_t ry,
                            uint16_t pl, uint16_t pc, uint16_t pr);
void renderSettingsMenuPage();
void handleSettingsMenuInput(uint32_t now);
void loadRemoteSettings();
void saveRemoteSettings();
void runBootStickCalibration();

// =============================================================================
//  HARDWARE DISPLAY CONFIGURATION (U8g2)
// =============================================================================
#define USE_SH1106 1 // 1 for 1.3" SH1106 (Remote), 0 for 0.96" SSD1306 (Rover)

#if USE_SH1106
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#else
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#endif

#define OLED_SCL_PIN  PB6
#define OLED_SDA_PIN  PB7

// =============================================================================
//  PIN DEFINITIONS
// =============================================================================

// 7 Analog Pins
#define PIN_STICK_LX    PB1   // Left Stick X   (ADC9) [Steering / Turn Blinkers]
#define PIN_STICK_LY    PB0   // Left Stick Y   (ADC8) [Throttle / Speed & RPM]
#define PIN_STICK_RX    PA4   // Right Stick X  (ADC4) [Inverted: 4095 - raw_rx for 90° mount]
#define PIN_STICK_RY    PA3   // Right Stick Y  (ADC3) [Camera Tilt]
#define PIN_POT_LEFT    PA0   // Pot Left       (ADC0) [Speed Governor Factor]
#define PIN_POT_CENTER  PA1   // Pot Center     (ADC1) [Camera Tilt Trim]
#define PIN_POT_RIGHT   PA2   // Pot Right      (ADC2) [Auxiliary / Battery Sim]

// 6 Digital Pins
#define PIN_TOGGLE_LEFT  PB3  // Master Arm/Disarm (Active-LOW : LOW = ON/ARM, HIGH = OFF/PARK)
#define PIN_TOGGLE_RIGHT PB4  // SOUND MUTE SWITCH (Active-LOW : LOW = MUTE ON, HIGH = SOUND ACTIVE)
#define PIN_PUSH_LEFT    PA8  // Push Button Left  (Active-HIGH: HIGH = PRESSED, LOW = RELEASED)
#define PIN_PUSH_RIGHT   PA15 // Push Button Right (Active-HIGH: HIGH = PRESSED, LOW = RELEASED)
#define PIN_STICK_CLK_L  PA12 // Gimbal Click Left (Active-LOW : LOW = PRESSED, HIGH = UP) [PA10 -> PA12 to isolate USART1_RX]
#define PIN_STICK_CLK_R  PA11 // Gimbal Click Right(Active-LOW : LOW = PRESSED, HIGH = UP)

// Actuator Pins (Hardware Verified: PB9 = 5V Active Piezo Buzzer, PB8 = Haptic Vibration Motor)
#define PIN_BUZZER       PB9  // 5V Active Piezo Buzzer
#define PIN_HAPTIC       PB8  // Haptic Vibration Motor (via NPN Transistor/MOSFET)
#define PIN_LED_HB       PC13 // Onboard Heartbeat LED (Active-LOW)

// NRF24L01 Control Pins
#define PIN_NRF_CE       PB10 // Radio Chip Enable
#define PIN_NRF_CSN      PB11 // Radio Chip Select Not

// Calibration & Rover Limits
#define ADC_CENTER      2048
#define ADC_DEADBAND    140   // 1908 to 2188 reads zero throttle
#define MAX_SPEED_CMPS  314   // 314 cm/s = 11.31 km/h top sprint
#define MAX_MOTOR_RPM   600   // 600 RPM gearhead output

// =============================================================================
//  RADIO & DATA PACKET DEFINITIONS (24 Bytes Packed Little-Endian)
// =============================================================================
const uint8_t  RF_CHANNEL = 108;            // 2.508 GHz (Above Wi-Fi channels 1-13)
const uint64_t RF_PIPE    = 0xE8E8F0F0E1LL; // CyberRover X5 Link Address

struct __attribute__((packed)) RoverPacket {
  uint16_t stick_lx;    // Left Stick X   (0 - 4095)
  uint16_t stick_ly;    // Left Stick Y   (0 - 4095)
  uint16_t stick_rx;    // Right Stick X  (0 - 4095, Inverted)
  uint16_t stick_ry;    // Right Stick Y  (0 - 4095)
  uint16_t pot_l;       // Pot Left       (0 - 4095, Speed Governor)
  uint16_t pot_c;       // Pot Center     (0 - 4095, Camera Tilt)
  uint16_t pot_r;       // Pot Right      (0 - 4095, Aux)
  uint8_t  toggle_l;    // 1 = Armed ('D'), 0 = Disarmed ('P')
  uint8_t  toggle_r;    // 1 = Mute ON, 0 = Sound Active
  uint8_t  push_l;      // 1 = PRESSED, 0 = RELEASED
  uint8_t  push_r;      // 1 = PRESSED, 0 = RELEASED
  uint8_t  stick_clk_l; // 1 = PRESSED, 0 = UP
  uint8_t  stick_clk_r; // 1 = PRESSED, 0 = UP
  uint32_t frame_id;    // Rolling frame sequence counter
};

static_assert(sizeof(RoverPacket) == 24, "CRITICAL: RoverPacket must be exactly 24 bytes!");

// =============================================================================
//  CYBERPACKET (16-BYTE PACKED BINARY TELEOPERATION SPECIFICATION v3.0)
// =============================================================================
#define CYBER_FRAME_HEADER_1  0x5A
#define CYBER_FRAME_HEADER_2  0xA5
#define CYBER_FRAME_TRAILER   0xED
#define CYBER_PACKET_SIZE     16
#define CYBER_DEVICE_ID       0xC5

#define CYBER_BTN_ARM_BIT     0 // Bit 0: 1 = ARMED ('D'), 0 = PARK ('P')
#define CYBER_BTN_MUTE_BIT    1 // Bit 1: 1 = MUTE ON, 0 = SOUND ON
#define CYBER_BTN_PL_BIT      2 // Bit 2: 1 = Left Button PRESSED
#define CYBER_BTN_PR_BIT      3 // Bit 3: 1 = Right Button PRESSED
#define CYBER_BTN_CKL_BIT     4 // Bit 4: 1 = Left Click PRESSED (Turbo)
#define CYBER_BTN_CKR_BIT     5 // Bit 5: 1 = Right Click PRESSED
#define CYBER_PAGE_MASK       0xC0 // Bits 6-7: Display Page (0 to 3)
#define CYBER_PAGE_SHIFT      6

#define CYBER_PROFILE_MASK    0x0F // Bits 0-3: Profile (0=Arcade, 1=Tank, 2=1-Stick)
#define CYBER_RATE_MASK       0xF0 // Bits 4-7: Rate / 10 (2=20Hz, 4=40Hz, 5=50Hz)
#define CYBER_RATE_SHIFT      4

#pragma pack(push, 1)
struct CyberPacket {
  uint8_t  device_id;    // [0]     Hardware ID: CYBER_DEVICE_ID (0xC5)
  uint8_t  seq_id;       // [1]     Rolling Sequence Counter (0 - 255)
  uint16_t timestamp_ms; // [2-3]   Hardware millisecond timestamp (16-bit) for latency measurement
  int8_t   axis_lx;      // [4]     Left Stick X (-100% to +100%)
  int8_t   axis_ly;      // [5]     Left Stick Y (-100% to +100%) [Throttle]
  int8_t   axis_rx;      // [6]     Right Stick X (-100% to +100%) [Steering in Arcade]
  int8_t   axis_ry;      // [7]     Right Stick Y (-100% to +100%) [Track R in Tank]
  uint8_t  speed_gov;    // [8]     Speed Governor Factor (15 to 100%) [Pot Left]
  uint8_t  turn_ratio;   // [9]     Dynamic Turn Radius / Trim (0 to 75%) [Pot Center]
  uint8_t  pot_aux;      // [10]    Auxiliary Knob 3 / Page (0 to 100%) [Pot Right]
  uint8_t  buttons;      // [11]    bit0=ARM, bit1=MUTE, bit2=PL, bit3=PR, bit4=CKL, bit5=CKR, bit6-7=Page (0-3)
  uint8_t  mode_flags;   // [12]    bits 0-3 = Profile (0=Arcade, 1=Tank, 2=1-Stick), bits 4-7 = Rate / 10 (2, 4, 5)
  uint16_t frame_id;     // [13-14] High-Resolution Monotonic Frame ID (0 - 65535)
  uint8_t  crc8;         // [15]    CRC-8-ATM (Polynomial 0x07) over bytes [0..14]
};
#pragma pack(pop)
static_assert(sizeof(CyberPacket) == 16, "CRITICAL: CyberPacket must be exactly 16 bytes!");

// CRC-8 Lookup Table (Polynomial 0x07: x^8 + x^2 + x + 1, CRC-8-ATM)
static const uint8_t crc8_atm_table[256] = {
  0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
  0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
  0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
  0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
  0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
  0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
  0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
  0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
  0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
  0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
  0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
  0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
  0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
  0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
  0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
  0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

inline uint8_t computeCyberChecksum(const CyberPacket& p) {
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(&p);
  uint8_t crc = 0x00;
  for (size_t i = 0; i < 15; i++) {
    crc = crc8_atm_table[crc ^ raw[i]];
  }
  return crc;
}

inline uint8_t computeCyberChecksum(const volatile CyberPacket& p) {
  const volatile uint8_t* raw = reinterpret_cast<const volatile uint8_t*>(&p);
  uint8_t crc = 0x00;
  for (size_t i = 0; i < 15; i++) {
    crc = crc8_atm_table[crc ^ raw[i]];
  }
  return crc;
}

inline uint8_t computeCyberCrc8(const CyberPacket& p) {
  return computeCyberChecksum(p);
}

inline uint8_t computeCyberCrc8(const volatile CyberPacket& p) {
  return computeCyberChecksum(p);
}

inline bool isCyberPacketValid(const CyberPacket& p) {
  if (p.device_id != CYBER_DEVICE_ID) return false;
  if (p.axis_lx < -100 || p.axis_lx > 100) return false;
  if (p.axis_ly < -100 || p.axis_ly > 100) return false;
  if (p.axis_rx < -100 || p.axis_rx > 100) return false;
  if (p.axis_ry < -100 || p.axis_ry > 100) return false;
  if (p.speed_gov > 100) return false;
  if (p.turn_ratio > 100) return false;
  if (p.pot_aux > 100) return false;
  if (computeCyberChecksum(p) != p.crc8) return false;
  return true;
}

inline void setCyberButtons(CyberPacket& p, bool arm, bool mute, bool pl, bool pr, bool ckl, bool ckr, uint8_t page) {
  p.buttons = (arm ? 1 : 0) |
              ((mute ? 1 : 0) << 1) |
              ((pl ? 1 : 0) << 2) |
              ((pr ? 1 : 0) << 3) |
              ((ckl ? 1 : 0) << 4) |
              ((ckr ? 1 : 0) << 5) |
              ((page & 0x03) << 6);
}

inline void setCyberModeFlags(CyberPacket& p, uint8_t profile, uint8_t rate_hz) {
  uint8_t rate_code = rate_hz / 10;
  p.mode_flags = (profile & CYBER_PROFILE_MASK) | ((rate_code & 0x0F) << CYBER_RATE_SHIFT);
}

void renderCompactTelemetryPage(const RoverPacket& pkt, bool rf_ack, bool is_muted, uint32_t now_ms);

// =============================================================================
//  SETTINGS & CONFIGURATION DATA (EMULATED EEPROM PERSISTENCE)
// =============================================================================
struct __attribute__((packed)) RemoteConfig {
  uint16_t magic;          // 0xC5E8 (Bumps magic to auto-initialize new configuration)
  uint16_t deadband_l;     // Default: 140 (Range: 50 to 400, Left Stick)
  uint16_t deadband_r;     // Default: 600 (Range: 50 to 1800, Right Stick)
  uint8_t  refresh_rate;   // Default: 40 (Options: 20, 40, 50)
  uint8_t  contrast;       // Default: 200 (Range: 25 to 255)
  uint8_t  drive_profile;  // Default: 0 (0 = ARCADE, 1 = TANK, 2 = 1-STICK)
  uint8_t  notch_sound;    // Default: 1 (1 = ON, 0 = OFF)
  uint8_t  boot_calib;     // Default: 0 (0 = Skip directly to HUD, 1 = Enable 360 spin calib)
  // Calibration offsets & bounds for all 4 analog stick axes:
  uint16_t center_lx;      // Default: 2048
  uint16_t center_ly;      // Default: 2048
  uint16_t center_rx;      // Default: 2048
  uint16_t center_ry;      // Default: 2048
  uint16_t min_lx;         // Default: 0
  uint16_t max_lx;         // Default: 4095
  uint16_t min_ly;         // Default: 0
  uint16_t max_ly;         // Default: 4095
  uint16_t min_rx;         // Default: 0
  uint16_t max_rx;         // Default: 4095
  uint16_t min_ry;         // Default: 0
  uint16_t max_ry;         // Default: 4095
};

RemoteConfig remoteConfig;

// =============================================================================
//  AUTO-CENTERING ZERO-BASELINE & NORMALIZATION ENGINE
// =============================================================================
uint16_t auto_center_lx = 2048;
uint16_t auto_center_ly = 2048;
uint16_t auto_center_rx = 2048;
uint16_t auto_center_ry = 2048;
bool     auto_center_done = false;

// Filtered Analog Hardware Channels (Preserved for Diagnostic Logging & Page 2)
uint16_t filt_lx = 2048;
uint16_t filt_ly = 2048;
uint16_t filt_rx = 2048;
uint16_t filt_ry = 2048;

// Live Normalized Percentage Tracking (-100% to +100%)
int live_thr_pct   = 0;
int live_str_pct   = 0;
int live_trk_l_pct = 0;
int live_trk_r_pct = 0;
int live_lx_pct    = 0;

// Live Rover Telemetry (Ingested from Rover S3 via ESP32-CAM Gateway on USART1 PA10)
int16_t  telem_rssi         = -99;
float    telem_cur_l        = 0.0f;
float    telem_cur_r        = 0.0f;
float    telem_bat_v        = 0.0f;
uint8_t  telem_failsafe     = 0;
uint32_t lastTelemRxTime    = 0;
bool     telem_link_ok      = false;
bool     armSafetyLockout   = false; // Anti-runaway: prevents armed boot until switch cycled to PARK

// Maps raw ADC input to a normalized percentage [-100, +100] relative to resting center, deadband, and calibrated limits
int calculateStickPercent(uint16_t raw_val, uint16_t center_val, uint16_t deadband, uint16_t min_val = 0, uint16_t max_val = 4095) {
  int diff = (int)raw_val - (int)center_val;
  if (abs(diff) <= (int)deadband) {
    return 0;
  }
  int pct = 0;
  if (diff > 0) {
    int max_span = (int)max_val - (int)center_val - (int)deadband;
    if (max_span < 600) max_span = 4095 - (int)center_val - (int)deadband;
    if (max_span < 600) max_span = 1200;
    pct = ((diff - (int)deadband) * 100) / max_span;
  } else {
    int min_span = (int)center_val - (int)min_val - (int)deadband;
    if (min_span < 600) min_span = (int)center_val - (int)deadband;
    if (min_span < 600) min_span = 1200;
    pct = ((diff + (int)deadband) * 100) / min_span;
  }
  if (pct > 100)  pct = 100;
  if (pct < -100) pct = -100;
  return pct;
}

// Maps normalized percentage [-100, +100] to a calibrated 12-bit ADC value [0, 4095] with exact 2048 center
uint16_t mapPercentToADC(int pct) {
  if (pct >= 100) return 4095;
  if (pct <= -100) return 0;
  if (pct == 0) return 2048;
  if (pct > 0) {
    return (uint16_t)(2048 + (pct * 2047) / 100);
  } else {
    return (uint16_t)(2048 + (pct * 2048) / 100);
  }
}

enum MenuState {
  MENU_STATE_ROOT,
  MENU_STATE_CALIB,
  MENU_STATE_DEADBAND_L,
  MENU_STATE_DEADBAND_R,
  MENU_STATE_BOOT_CALIB,
  MENU_STATE_PROFILE,
  MENU_STATE_RATE,
  MENU_STATE_CONTRAST,
  MENU_STATE_NOTCH,
  MENU_STATE_INFO
};

// Current Display Page (0 = Dashboard HUD, 1 = 7-Channel Spectrum, 2 = Combat Powertrain MFD, 3 = Settings Menu)
const uint8_t TOTAL_PAGES = 4;
uint8_t currentPage       = 0;

MenuState menuState = MENU_STATE_ROOT;
uint8_t menuIndex = 0;
uint8_t menuScrollTop = 0;
uint8_t calibStep = 0;

const uint8_t MENU_ITEMS_COUNT = 9;
const char* const menuTitles[MENU_ITEMS_COUNT] = {
  "1. JOYSTICK CALIB",
  "2. L-STICK DEADBAND",
  "3. R-STICK DEADBAND",
  "4. BOOT CALIBRATION",
  "5. DRIVE PROFILE",
  "6. REFRESH RATE",
  "7. OLED CONTRAST",
  "8. NOTCH SOUND",
  "9. SYSTEM INFO"
};

RoverPacket txPacket;
bool radioReady = false; // NRF24 inactive; Telemetry & controls routed via ESP-NOW Bridge

// =============================================================================
//  DIGITAL CONTROL TRACKING (5 Active Channels - CKR Decommissioned)
// =============================================================================
struct DigitalInput {
  uint8_t pin;
  const char* name;
  const char* tag;
  bool isToggle;
  bool activeLevel;
  bool state;
  bool lastRaw;
  uint32_t lastDebounceTime;
};

DigitalInput digInputs[] = {
  {PIN_TOGGLE_LEFT,  "Toggle Left  (PB3) ", "TL",  true,  LOW,  false, HIGH, 0},
  {PIN_TOGGLE_RIGHT, "Toggle Right (PB4) ", "TR",  true,  LOW,  false, HIGH, 0}, // SOUND MUTE
  {PIN_PUSH_LEFT,    "Push Left    (PA8) ", "PL",  false, HIGH, false, LOW,  0},
  {PIN_PUSH_RIGHT,   "Push Right   (PA15)", "PR",  false, HIGH, false, LOW,  0},
  {PIN_STICK_CLK_L,  "Stick Click L(PA12)", "CKL", false, LOW,  false, HIGH, 0}
};

const uint8_t NUM_DIG_INPUTS = sizeof(digInputs) / sizeof(digInputs[0]);

// =============================================================================
//  POTENTIOMETER TRACKING WITH HYSTERESIS
// =============================================================================
struct PotTracker {
  uint8_t pin;
  int lastReading;
  int lastNotch;
};

PotTracker potTrackers[3] = {
  {PIN_POT_LEFT,   -1, -1}, // Speed Governor
  {PIN_POT_CENTER, -1, -1}, // Camera Tilt
  {PIN_POT_RIGHT,  -1, -1}  // Auxiliary
};

// =============================================================================
//  ANALOG 3-STAGE FILTERING ENGINE (Median + IIR)
// =============================================================================
struct AnalogFilterState {
  uint16_t window[3];
  uint8_t  idx;
  float    iirState;
  bool     init;
};

static AnalogFilterState filterStates[7];

static uint16_t computeMedian3(uint16_t a, uint16_t b, uint16_t c) {
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

static uint16_t applyAnalogFilter(uint8_t chIdx, uint16_t rawInput, float alpha) {
  if (rawInput > 4095) rawInput = 4095;
  AnalogFilterState& s = filterStates[chIdx];

  if (!s.init) {
    s.window[0] = rawInput;
    s.window[1] = rawInput;
    s.window[2] = rawInput;
    s.idx = 0;
    s.iirState = (float)rawInput;
    s.init = true;
    return rawInput;
  }

  // 1. Order-3 Median Filter
  s.window[s.idx] = rawInput;
  s.idx = (s.idx + 1) % 3;
  uint16_t med = computeMedian3(s.window[0], s.window[1], s.window[2]);

  // 2. Low-Pass IIR Filter
  s.iirState = s.iirState + alpha * ((float)med - s.iirState);
  int conditioned = (int)(s.iirState + 0.5f);
  if (conditioned < 0)    conditioned = 0;
  if (conditioned > 4095) conditioned = 4095;

  return (uint16_t)conditioned;
}

// =============================================================================
//  BITMAPPED GRAPHICS ASSETS (UPiR CLUSTER HUD)
// =============================================================================
static const unsigned char PROGMEM bitmap_temp_icon[] = {
  0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x38, 0x00, 0x38, 0x00,
  0x54, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0x7C, 0x00
};

static const unsigned char PROGMEM bitmap_fuel_icon[] = {
  0x70, 0x5A, 0x7A, 0x5E, 0x52, 0x72, 0x70, 0xFE
};

// =============================================================================
//  HIGH-EFFICIENCY PHASE-DRIVEN AUDIO-HAPTIC ENGINE WITH SOUND MUTE
// =============================================================================
AlertEvent currentAlert   = ALERT_NONE;
uint32_t   alertStartTime = 0;
uint8_t    alertPhase     = 0;

void setOutputs(bool buzzer, bool haptic) {
  // Toggle Right (PB4) controls Sound Mute
  bool isMuted = digInputs[1].state;
  digitalWrite(PIN_BUZZER, (buzzer && !isMuted) ? HIGH : LOW);
  digitalWrite(PIN_HAPTIC, haptic ? HIGH : LOW);
}

void playStartupConfirmation() {
  bool isMuted = digInputs[1].state;
  Serial.print(F(">>> [SYSTEM READY] "));
  if (isMuted) {
    Serial.println(F("Sound MUTED (Haptic pulse only)..."));
  } else {
    Serial.println(F("Sound ACTIVE (Beep + Haptic pulse)..."));
    digitalWrite(PIN_BUZZER, HIGH);
  }
  digitalWrite(PIN_HAPTIC, HIGH);
  delay(80);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_HAPTIC, LOW);
}

void triggerAlert(AlertEvent evt) {
  // Suppress low-priority or repeat alerts while an alert is already in progress
  if (currentAlert != ALERT_NONE) {
    if (evt == ALERT_POT_NOTCH || evt == ALERT_BUTTON_CLICK) {
      return;
    }
  }

  currentAlert   = evt;
  alertStartTime = millis();
  alertPhase     = 0;

  switch (evt) {
    case ALERT_LINK_HANDSHAKE:
    case ALERT_TOGGLE_ON:
    case ALERT_TOGGLE_OFF:
    case ALERT_BUTTON_CLICK:
    case ALERT_POT_NOTCH:
    case ALERT_LINK_LOST:
      setOutputs(true, true);
      break;
    case ALERT_TURBO_KICK:
      setOutputs(false, true); // Haptic only for turbo kick
      break;
    default:
      setOutputs(false, false);
      break;
  }
}

void updateAlertEngine() {
  if (currentAlert == ALERT_NONE) return;
  uint32_t elapsed = millis() - alertStartTime;

  switch (currentAlert) {
    case ALERT_LINK_HANDSHAKE:
      if (alertPhase == 0 && elapsed >= 60) {
        setOutputs(false, false);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 95) {
        setOutputs(true, true);
        alertPhase = 2;
      } else if (alertPhase == 2 && elapsed >= 165) {
        setOutputs(false, false);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_TOGGLE_ON:
      if (alertPhase == 0 && elapsed >= 30) {
        setOutputs(false, true);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 75) {
        setOutputs(false, false);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_TOGGLE_OFF:
      if (alertPhase == 0 && elapsed >= 20) {
        setOutputs(false, true);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 50) {
        setOutputs(false, false);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_BUTTON_CLICK:
      if (alertPhase == 0 && elapsed >= 25) {
        setOutputs(false, true);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 60) {
        setOutputs(false, false);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_POT_NOTCH:
      if (alertPhase == 0 && elapsed >= 15) {
        setOutputs(false, true);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 35) {
        setOutputs(false, false);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_TURBO_KICK:
      if (elapsed >= 90) {
        setOutputs(false, false);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_LINK_LOST:
      if (alertPhase == 0 && elapsed >= 150) {
        setOutputs(false, false);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 250) {
        setOutputs(true, true);
        alertPhase = 2;
      } else if (alertPhase == 2 && elapsed >= 450) {
        setOutputs(false, false);
        currentAlert = ALERT_NONE;
      }
      break;

    default:
      setOutputs(false, false);
      currentAlert = ALERT_NONE;
      break;
  }
}

// =============================================================================
//  JTAG RELEASE ROUTINE (Frees PA15, PB3, PB4 while preserving SWD PA13/PA14)
// =============================================================================
void releaseJTAG() {
  // Direct hardware register writes for STM32F103 ARM Cortex-M3:
  // Bit 0 of RCC_APB2ENR is AFIOEN (AFIO peripheral clock)
  RCC->APB2ENR |= (1UL << 0);
  // SWJ_CFG in AFIO_MAPR [bits 26:24]: 010 = JTAG-DP Disabled, SW-DP Enabled
  AFIO->MAPR = (AFIO->MAPR & ~(7UL << 24)) | (2UL << 24);
}

// =============================================================================
//  OLED INSTRUMENT CLUSTER RENDERING HELPERS
// =============================================================================
float speed_smooth = 0.0f;
float rpm_smooth   = 0.0f;
float fuel_smooth  = 88.0f;

void drawThickArc(int cx, int cy, int r_in, int r_out, float start_deg, float end_deg) {
  if (start_deg > end_deg) {
    float temp = start_deg;
    start_deg = end_deg;
    end_deg = temp;
  }
  for (float a = start_deg; a <= end_deg; a += 1.0f) {
    float rad = a * 0.0174532925f;
    float cos_a = cos(rad);
    float sin_a = sin(rad);
    int x1 = cx + (int)(r_in  * cos_a + 0.5f);
    int y1 = cy - (int)(r_in  * sin_a + 0.5f);
    int x2 = cx + (int)(r_out * cos_a + 0.5f);
    int y2 = cy - (int)(r_out * sin_a + 0.5f);
    u8g2.drawLine(x1, y1, x2, y2);
  }
}

void drawDialTrack(int cx, int cy, int r_track, float start_deg, float end_deg) {
  for (float a = start_deg; a <= end_deg; a += 10.0f) {
    float rad = a * 0.0174532925f;
    float cos_a = cos(rad);
    float sin_a = sin(rad);
    int step_idx = (int)((a - start_deg) / 10.0f + 0.5f);
    if (step_idx % 3 == 0) {
      int x_t1 = cx + (int)((r_track - 1) * cos_a + 0.5f);
      int y_t1 = cy - (int)((r_track - 1) * sin_a + 0.5f);
      int x_t2 = cx + (int)((r_track + 2) * cos_a + 0.5f);
      int y_t2 = cy - (int)((r_track + 2) * sin_a + 0.5f);
      u8g2.drawLine(x_t1, y_t1, x_t2, y_t2);
    } else {
      int x_dot = cx + (int)(r_track * cos_a + 0.5f);
      int y_dot = cy - (int)(r_track * sin_a + 0.5f);
      u8g2.drawPixel(x_dot, y_dot);
    }
  }
}

void renderDashboardHUD(char drive_mode, float governor_factor, uint16_t pot_c, uint16_t pot_r,
                        bool blinker_l, bool blinker_r, bool blink_state, bool rf_connected, bool is_muted, bool is_turbo) {
  u8g2.clearBuffer();

  // ---------------------------------------------------------------------------
  // 1. LEFT GAUGE: SPEEDOMETER (cm/s)
  // ---------------------------------------------------------------------------
  const int cx_l = 30;
  const int cy_l = 34;
  const int r_in_l = 21;
  const int r_out_l = 24;
  const int r_track_l = 26;

  drawDialTrack(cx_l, cy_l, r_track_l, 35.0f, 215.0f);

  float speed_frac = speed_smooth / (float)MAX_SPEED_CMPS;
  if (speed_frac > 1.0f) speed_frac = 1.0f;
  if (speed_frac < 0.03f) speed_frac = 0.03f;

  float speed_current_deg = 215.0f - (speed_frac * 180.0f);
  drawThickArc(cx_l, cy_l, r_in_l, r_out_l, speed_current_deg, 215.0f);

  // Governor Limit Marker
  float gov_deg = 215.0f - (governor_factor * 180.0f);
  float gov_rad = gov_deg * 0.0174532925f;
  int gx1 = cx_l + (int)(25 * cos(gov_rad) + 0.5f);
  int gy1 = cy_l - (int)(25 * sin(gov_rad) + 0.5f);
  int gx2 = cx_l + (int)(29 * cos(gov_rad) + 0.5f);
  int gy2 = cy_l - (int)(29 * sin(gov_rad) + 0.5f);
  u8g2.drawLine(gx1, gy1, gx2, gy2);

  int display_speed_val = (int)(speed_smooth + 0.5f);
  char speed_str[8];
  itoa(display_speed_val, speed_str, 10);
  u8g2.setFont(u8g2_font_helvB14_tr);
  int speed_str_w = u8g2.getStrWidth(speed_str);
  u8g2.drawStr(cx_l - (speed_str_w / 2), 38, speed_str);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx_l - 8, 48, "CM/S");

  // Left Blinker Arrow (◄)
  if (blinker_l && blink_state) {
    u8g2.drawTriangle(4, 6, 11, 2, 11, 10);
  } else {
    u8g2.drawPixel(4, 6);
    u8g2.drawPixel(11, 6);
  }

  // ---------------------------------------------------------------------------
  // 2. RIGHT GAUGE: MOTOR TACHOMETER (RPM)
  // ---------------------------------------------------------------------------
  const int cx_r = 98;
  const int cy_r = 34;
  const int r_in_r = 21;
  const int r_out_r = 24;
  const int r_track_r = 26;

  drawDialTrack(cx_r, cy_r, r_track_r, -35.0f, 145.0f);

  float rpm_frac = rpm_smooth / (float)MAX_MOTOR_RPM;
  if (rpm_frac > 1.0f) rpm_frac = 1.0f;
  if (rpm_frac < 0.03f) rpm_frac = 0.03f;

  float rpm_current_deg = -35.0f + (rpm_frac * 180.0f);
  drawThickArc(cx_r, cy_r, r_in_r, r_out_r, -35.0f, rpm_current_deg);

  int display_rpm_val = (int)(rpm_smooth + 0.5f);
  char rpm_str[8];
  itoa(display_rpm_val, rpm_str, 10);
  u8g2.setFont(u8g2_font_helvB14_tr);
  int rpm_str_w = u8g2.getStrWidth(rpm_str);
  u8g2.drawStr(cx_r - (rpm_str_w / 2), 38, rpm_str);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx_r - 6, 48, "RPM");

  // Right Blinker Arrow (►)
  if (blinker_r && blink_state) {
    u8g2.drawTriangle(123, 6, 116, 2, 116, 10);
  } else {
    u8g2.drawPixel(116, 6);
    u8g2.drawPixel(123, 6);
  }

  // ---------------------------------------------------------------------------
  // 3. CENTER COLUMN: STATUS & TELEMETRY HUD
  // ---------------------------------------------------------------------------
  u8g2.drawXBMP(60, 4, 9, 9, bitmap_temp_icon);
  u8g2.drawPixel(58, 2);
  u8g2.drawPixel(64, 1);
  u8g2.drawPixel(70, 2);

  if (rf_connected) {
    u8g2.drawDisc(64, 15, 1);
  } else if (blink_state) {
    u8g2.drawCircle(64, 15, 1);
  }

  // Turbo Badge or Mute Badge
  if (is_turbo) {
    u8g2.drawRBox(51, 18, 26, 9, 2);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(53, 25, "TRB");
    u8g2.setDrawColor(1);
  } else if (is_muted) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(62, 22, "M");
  }

  // Drive Mode Indicator
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(62, 29, ":");

  if (armSafetyLockout) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(54, 39, "LOCK");
  } else {
    char mode_str[2] = { drive_mode, '\0' };
    u8g2.drawStr(61, 39, mode_str);
  }

  // Dynamic Power Hashes
  int num_hashes = (int)(speed_frac * 4.0f + 0.5f);
  if (num_hashes < 1) num_hashes = 1;
  for (int h = 0; h < num_hashes; h++) {
    int hx = 72 + h * 3;
    int hy = 43 - h * 2;
    u8g2.drawLine(hx, hy, hx + 2, hy - 4);
  }

  // Fuel Gauge
  u8g2.drawXBMP(60, 46, 8, 8, bitmap_fuel_icon);
  const int cx_fuel = 64;
  const int cy_fuel = 40;
  const int r_fuel  = 20;

  for (float fa = 245.0f; fa <= 295.0f; fa += 2.0f) {
    float frad = fa * 0.0174532925f;
    int fx = cx_fuel + (int)(r_fuel * cos(frad) + 0.5f);
    int fy = cy_fuel - (int)(r_fuel * sin(frad) + 0.5f);
    u8g2.drawPixel(fx, fy);
  }
  u8g2.drawLine(52, 57, 52, 60);
  u8g2.drawLine(76, 57, 76, 60);

  float fuel_frac = fuel_smooth / 100.0f;
  if (fuel_frac > 1.0f) fuel_frac = 1.0f;
  if (fuel_frac < 0.0f) fuel_frac = 0.0f;

  float fuel_fill_deg = 245.0f + (fuel_frac * 50.0f);
  for (float fa = 245.0f; fa <= fuel_fill_deg; fa += 1.0f) {
    float frad = fa * 0.0174532925f;
    float cos_fa = cos(frad);
    float sin_fa = sin(frad);
    int fx1 = cx_fuel + (int)((r_fuel - 1) * cos_fa + 0.5f);
    int fy1 = cy_fuel - (int)((r_fuel - 1) * sin_fa + 0.5f);
    int fx2 = cx_fuel + (int)((r_fuel + 1) * cos_fa + 0.5f);
    int fy2 = cy_fuel - (int)((r_fuel + 1) * sin_fa + 0.5f);
    u8g2.drawLine(fx1, fy1, fx2, fy2);
  }

  // ---------------------------------------------------------------------------
  // 4. LOWER CORNER BARS: POT CENTER (TURNING RADIUS) & POT RIGHT (AUX)
  // ---------------------------------------------------------------------------
  u8g2.drawHLine(2, 63, 37);
  u8g2.drawVLine(38, 61, 3);
  int fill_c = (int)(((float)pot_c / 4095.0f) * 36.0f + 0.5f);
  if (fill_c > 36) fill_c = 36;
  if (fill_c > 0) u8g2.drawBox(2, 61, fill_c, 2);

  u8g2.setFont(u8g2_font_4x6_tr);
  int tr_pct = (int)(((float)pot_c / 4095.0f) * 75.0f + 0.5f);
  char tr_buf[10];
  snprintf(tr_buf, sizeof(tr_buf), "TR:%d%%", tr_pct);
  u8g2.drawStr(2, 59, tr_buf);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(98, 59, "P1:HUD");

  u8g2.drawHLine(89, 63, 37);
  u8g2.drawVLine(89, 61, 3);
  int fill_r = (int)(((float)pot_r / 4095.0f) * 36.0f + 0.5f);
  if (fill_r > 36) fill_r = 36;
  if (fill_r > 0) u8g2.drawBox(126 - fill_r, 61, fill_r, 2);

  u8g2.sendBuffer();
}

// =============================================================================
//  PAGE 2: 7-CHANNEL SPECTRUM ANALYZER & RAW ADC MONITOR (AUDIO ANALYZER HUD)
// =============================================================================
float peak_positions[7] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

void renderSpectrumAnalyzer(uint16_t lx, uint16_t ly, uint16_t rx, uint16_t ry,
                            uint16_t pl, uint16_t pc, uint16_t pr) {
  u8g2.clearBuffer();

  const char* labels[7] = { "LX", "LY", "RX", "RY", "PL", "PC", "PR" };
  uint16_t values[7]    = { lx, ly, rx, ry, pl, pc, pr };

  const int y_base     = 54;
  const int max_height = 50; // Dynamic bar height from y=54 up to y=4

  u8g2.setFont(u8g2_font_5x7_tr);

  for (uint8_t i = 0; i < 7; i++) {
    int x = 7 + i * 17; // width 12, gap 5, margin 7 (symmetrical across 128 px)
    uint16_t val = values[i];
    if (val > 4095) val = 4095;

    // Calculate vertical fill height (0 - 50 px)
    int bar_h = (int)(((float)val / 4095.0f) * (float)max_height + 0.5f);
    if (bar_h > max_height) bar_h = max_height;
    if (bar_h < 1 && val > 0) bar_h = 1;

    // Smooth falling peak decay tracking
    if ((float)bar_h >= peak_positions[i]) {
      peak_positions[i] = (float)bar_h;
    } else {
      peak_positions[i] -= 0.8f; // Falling decay rate
      if (peak_positions[i] < 0.0f) peak_positions[i] = 0.0f;
    }

    // Draw solid vertical bar
    if (bar_h > 0) {
      u8g2.drawBox(x, y_base - bar_h, 12, bar_h);
    }

    // Draw floating peak tick line (12 px wide, 2 px above the current peak height)
    int peak_y = y_base - (int)peak_positions[i] - 2;
    if (peak_y < 2) peak_y = 2;
    u8g2.drawHLine(x, peak_y, 12);

    // Draw 2-character label centered directly below the 12 px bar
    // Font 5x7: 2 chars = 10 px wide. Centered at x + 1.
    u8g2.drawStr(x + 1, 63, labels[i]);
  }

  u8g2.sendBuffer();
}

// =============================================================================
//  PAGE 3: COMBAT POWERTRAIN & AVIONICS MFD (FIRMWARE v1.5)
// =============================================================================
void renderCompactTelemetryPage(const RoverPacket& pkt, bool rf_ack, bool is_muted, uint32_t now_ms) {
  u8g2.clearBuffer();

  // ---------------------------------------------------------------------------
  // 1. TOP INVERTED HEADER BAR (y=0 to 10): Page Tag, Uptime, RSSI Antenna
  // ---------------------------------------------------------------------------
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(0); // High-contrast black text on white header box
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 8, "P3:MFD");

  // Remote Mission Uptime (MM:SS)
  uint32_t total_sec  = now_ms / 1000;
  uint32_t uptime_min = total_sec / 60;
  uint32_t uptime_sec = total_sec % 60;
  char up_buf[12];
  snprintf(up_buf, sizeof(up_buf), "UP %02lu:%02lu", (unsigned long)uptime_min, (unsigned long)uptime_sec);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(44, 7, up_buf);

  // Tactical 4-Bar RSSI Antenna Icon & Signal Strength (dBm)
  bool link_active = telem_link_ok || rf_ack;
  if (link_active) {
    // Dynamic signal bars: heights 2, 3, 5, 6 based on genuine RSSI
    u8g2.drawBox(86, 6, 2, 2);
    if (telem_rssi > -88) u8g2.drawBox(89, 5, 2, 3);
    if (telem_rssi > -76) u8g2.drawBox(92, 3, 2, 5);
    if (telem_rssi > -66) u8g2.drawBox(95, 2, 2, 6);
    u8g2.setFont(u8g2_font_5x7_tr);
    char rssi_str[10];
    snprintf(rssi_str, sizeof(rssi_str), "%ddB", (int)telem_rssi);
    u8g2.drawStr(100, 8, rssi_str);
  } else {
    // Single bar outline / warning
    u8g2.drawFrame(86, 6, 2, 2);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(93, 7, "NO_RF");
  }

  // Restore normal draw color (white graphics on black canvas)
  u8g2.setDrawColor(1);

  // ---------------------------------------------------------------------------
  // 2. TACTICAL GRID DIVIDERS
  // ---------------------------------------------------------------------------
  u8g2.drawHLine(0, 10, 128);
  u8g2.drawVLine(68, 11, 41); // Split: 68px left (analog), 59px right (powertrain)
  u8g2.drawHLine(0, 52, 128); // Bottom divider above switch dock

  // ---------------------------------------------------------------------------
  // 3. LEFT SECTION: 7 ANALOG CHANNELS & 40Hz RATE (x=0 to 67)
  // ---------------------------------------------------------------------------
  u8g2.setFont(u8g2_font_4x6_tr);

  char buf_lx[8], buf_ly[8], buf_rx[8], buf_ry[8];
  char buf_pl[8], buf_pc[8], buf_pr[8];
  snprintf(buf_lx, sizeof(buf_lx), "%4u", pkt.stick_lx);
  snprintf(buf_ly, sizeof(buf_ly), "%4u", pkt.stick_ly);
  snprintf(buf_rx, sizeof(buf_rx), "%4u", pkt.stick_rx);
  snprintf(buf_ry, sizeof(buf_ry), "%4u", pkt.stick_ry);
  snprintf(buf_pl, sizeof(buf_pl), "%4u", pkt.pot_l);
  snprintf(buf_pc, sizeof(buf_pc), "%4u", pkt.pot_c);
  snprintf(buf_pr, sizeof(buf_pr), "%4u", pkt.pot_r);

  // Row 1 (y=19): LX & PL (Speed Pot)
  u8g2.drawStr(2, 19, "LX:");
  u8g2.drawStr(16, 19, buf_lx);
  u8g2.drawStr(36, 19, "PL:");
  u8g2.drawStr(50, 19, buf_pl);

  // Row 2 (y=29): LY & PC (Tilt Trim)
  u8g2.drawStr(2, 29, "LY:");
  u8g2.drawStr(16, 29, buf_ly);
  u8g2.drawStr(36, 29, "PC:");
  u8g2.drawStr(50, 29, buf_pc);

  // Row 3 (y=39): RX & PR (Aux Dial)
  u8g2.drawStr(2, 39, "RX:");
  u8g2.drawStr(16, 39, buf_rx);
  u8g2.drawStr(36, 39, "PR:");
  u8g2.drawStr(50, 39, buf_pr);

  // Row 4 (y=49): RY & Radio Loop Rate
  u8g2.drawStr(2, 49, "RY:");
  u8g2.drawStr(16, 49, buf_ry);
  u8g2.drawStr(36, 49, "TX:40Hz");

  // ---------------------------------------------------------------------------
  // 4. RIGHT SECTION: BTS7960 POWERTRAIN & CAR BATTERY POD (x=69 to 127)
  // ---------------------------------------------------------------------------
  // Car 3S Battery Voltage (Live Telemetry or calibrated Pot R)
  float car_volts = (telem_link_ok && telem_bat_v > 5.0f) ? telem_bat_v : (9.60f + (((float)pkt.pot_r / 4095.0f) * 3.00f));
  int v_whole = (int)car_volts;
  int v_tenth = (int)((car_volts - (float)v_whole) * 10.0f + 0.5f);
  if (v_tenth > 9) { v_whole++; v_tenth = 0; }
  char volt_buf[8];
  snprintf(volt_buf, sizeof(volt_buf), "%d.%dV", v_whole, v_tenth);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(71, 19, "BAT");
  u8g2.drawStr(91, 19, volt_buf);

  // Battery Icon at x=116, y=13 (9x6 px)
  u8g2.drawFrame(116, 13, 9, 6);
  u8g2.drawVLine(125, 14, 4); // positive terminal
  int bat_fill_w = (int)(((float)pkt.pot_r / 4095.0f) * 6.0f + 0.5f);
  if (bat_fill_w > 6) bat_fill_w = 6;
  if (bat_fill_w > 0) {
    u8g2.drawBox(117, 14, bat_fill_w, 4);
  }

  // BTS7960 Live Measured Motor Currents from Rover Telemetry
  float current_l = telem_link_ok ? telem_cur_l : 0.0f;
  float current_r = telem_link_ok ? telem_cur_r : 0.0f;

  // Left Motor Current (IL: 3.2A)
  int il_whole = (int)current_l;
  int il_tenth = (int)((current_l - (float)il_whole) * 10.0f + 0.5f);
  if (il_tenth > 9) { il_whole++; il_tenth = 0; }
  char il_buf[8];
  snprintf(il_buf, sizeof(il_buf), "%d.%dA", il_whole, il_tenth);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(71, 28, "IL:");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(86, 28, il_buf);

  // Left Motor Load Bar (54px wide frame, 0 to 15A scale)
  u8g2.drawFrame(71, 30, 54, 4);
  int bar_l_w = (int)((current_l / 15.0f) * 52.0f + 0.5f);
  if (bar_l_w > 52) bar_l_w = 52;
  if (bar_l_w > 0) {
    u8g2.drawBox(72, 31, bar_l_w, 2);
  }

  // Right Motor Current (IR: 4.1A)
  int ir_whole = (int)current_r;
  int ir_tenth = (int)((current_r - (float)ir_whole) * 10.0f + 0.5f);
  if (ir_tenth > 9) { ir_whole++; ir_tenth = 0; }
  char ir_buf[8];
  snprintf(ir_buf, sizeof(ir_buf), "%d.%dA", ir_whole, ir_tenth);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(71, 42, "IR:");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(86, 42, ir_buf);

  // Right Motor Load Bar (54px wide frame, 0 to 15A scale)
  u8g2.drawFrame(71, 44, 54, 4);
  int bar_r_w = (int)((current_r / 15.0f) * 52.0f + 0.5f);
  if (bar_r_w > 52) bar_r_w = 52;
  if (bar_r_w > 0) {
    u8g2.drawBox(72, 45, bar_r_w, 2);
  }

  // ---------------------------------------------------------------------------
  // 5. BOTTOM SECTION: 6 INTERACTIVE DIGITAL DOCK TILES (y=54 to 63)
  // ---------------------------------------------------------------------------
  const char* sw_names[6] = {"TL", "TR", "PL", "PR", "CKL", "CKR"};
  bool sw_states[6] = {
    digInputs[0].state, // TL (Master Arm: 1 = Armed, 0 = Park)
    digInputs[1].state, // TR (Sound Mute: 1 = Muted, 0 = Sound Active)
    digInputs[2].state, // PL
    digInputs[3].state, // PR
    digInputs[4].state, // CKL
    false               // CKR (Decommissioned)
  };

  u8g2.setFont(u8g2_font_4x6_tr);
  for (uint8_t i = 0; i < 6; i++) {
    int bx = 1 + i * 21;
    bool active = sw_states[i];
    int txt_offset = (strlen(sw_names[i]) >= 3) ? 3 : 5;

    if (active) {
      // Solid inverted white tile with black text when active/pressed
      u8g2.drawBox(bx, 54, 20, 10);
      u8g2.setDrawColor(0);
      u8g2.drawStr(bx + txt_offset, 62, sw_names[i]);
      u8g2.setDrawColor(1);
    } else {
      // Hollow border tile with white text when idle/released
      u8g2.drawFrame(bx, 54, 20, 10);
      u8g2.drawStr(bx + txt_offset, 62, sw_names[i]);
    }
  }

  u8g2.sendBuffer();
}

// =============================================================================
//  EEPROM CONFIGURATION LOADER & PERSISTENCE
// =============================================================================
void loadRemoteSettings() {
  EEPROM.get(0, remoteConfig);
  if (remoteConfig.magic != 0xC5EA) {
    remoteConfig.magic        = 0xC5EA;
    remoteConfig.deadband_l   = 140;
    remoteConfig.deadband_r   = 800; // Default high deadband for worn right gimbal!
    remoteConfig.refresh_rate = 50; // 50 Hz high-speed update rate (20ms interval)
    remoteConfig.contrast     = 200;
    remoteConfig.drive_profile= 0; // Forced ARCADE mode default
    remoteConfig.notch_sound  = 1;
    remoteConfig.boot_calib   = 0; // Default: Skip straight to HUD! (No boring calibration)
    remoteConfig.center_lx    = 2048;
    remoteConfig.center_ly    = 2048;
    remoteConfig.center_rx    = 2048;
    remoteConfig.center_ry    = 2048;
    remoteConfig.min_lx       = 0;
    remoteConfig.max_lx       = 4095;
    remoteConfig.min_ly       = 0;
    remoteConfig.max_ly       = 4095;
    remoteConfig.min_rx       = 0;
    remoteConfig.max_rx       = 4095;
    remoteConfig.min_ry       = 0;
    remoteConfig.max_ry       = 4095;
    EEPROM.put(0, remoteConfig);
  } else {
    if (remoteConfig.deadband_l < 50 || remoteConfig.deadband_l > 400) remoteConfig.deadband_l = 140;
    if (remoteConfig.deadband_r < 50 || remoteConfig.deadband_r > 1800) remoteConfig.deadband_r = 800;
    if (remoteConfig.refresh_rate != 20 && remoteConfig.refresh_rate != 40 && remoteConfig.refresh_rate != 50) remoteConfig.refresh_rate = 40;
    if (remoteConfig.contrast < 25) remoteConfig.contrast = 200;
    if (remoteConfig.drive_profile > 2) remoteConfig.drive_profile = 0;
    if (remoteConfig.boot_calib > 1) remoteConfig.boot_calib = 0;
    if (remoteConfig.min_lx >= remoteConfig.max_lx || remoteConfig.max_lx < 1000) { remoteConfig.min_lx = 0; remoteConfig.max_lx = 4095; }
    if (remoteConfig.min_ly >= remoteConfig.max_ly || remoteConfig.max_ly < 1000) { remoteConfig.min_ly = 0; remoteConfig.max_ly = 4095; }
    if (remoteConfig.min_rx >= remoteConfig.max_rx || remoteConfig.max_rx < 1000) { remoteConfig.min_rx = 0; remoteConfig.max_rx = 4095; }
    if (remoteConfig.min_ry >= remoteConfig.max_ry || remoteConfig.max_ry < 1000) { remoteConfig.min_ry = 0; remoteConfig.max_ry = 4095; }
  }
  u8g2.setContrast(remoteConfig.contrast);

  // Synchronize live active running zero-centers immediately upon loading
  auto_center_lx = remoteConfig.center_lx;
  auto_center_ly = remoteConfig.center_ly;
  auto_center_rx = remoteConfig.center_rx;
  auto_center_ry = remoteConfig.center_ry;
  auto_center_done = true;
}

void saveRemoteSettings() {
  EEPROM.put(0, remoteConfig);
  // Re-affirm live active running centers
  auto_center_lx = remoteConfig.center_lx;
  auto_center_ly = remoteConfig.center_ly;
  auto_center_rx = remoteConfig.center_rx;
  auto_center_ry = remoteConfig.center_ry;
  Serial.println(F("[EEPROM] Saved & Synchronized"));
}

// =============================================================================
//  PAGE 4: SETTINGS & SYSTEM MENU (UPiR AESTHETICS)
// =============================================================================
void renderSettingsMenuPage() {
  u8g2.clearBuffer();

  if (menuState == MENU_STATE_ROOT) {
    // Outer rounded enclosure matching UPiR aesthetic
    u8g2.drawRFrame(1, 1, 126, 62, 3);

    for (uint8_t row = 0; row < 3; row++) {
      uint8_t idx = menuScrollTop + row;
      if (idx >= MENU_ITEMS_COUNT) break;

      int y_box = 4 + row * 19;

      if (idx == menuIndex) {
        // Inverted rounded highlight pill
        u8g2.drawRBox(5, y_box, 118, 18, 2);
        u8g2.setDrawColor(0); // Black on white pill
        u8g2.drawDisc(11, y_box + 9, 2); // Bullet icon
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(18, y_box + 12, menuTitles[idx]);
        u8g2.setDrawColor(1);
      } else {
        // Normal text with directional indicators
        u8g2.setFont(u8g2_font_4x6_tr);
        if (row == 0 && idx > 0) {
          u8g2.drawStr(8, y_box + 12, "^ ");
        } else if (row == 2 && idx < MENU_ITEMS_COUNT - 1) {
          u8g2.drawStr(8, y_box + 12, "v ");
        } else {
          u8g2.drawStr(8, y_box + 12, "  ");
        }
        u8g2.drawStr(16, y_box + 12, menuTitles[idx]);
      }
    }
  } else if (menuState == MENU_STATE_DEADBAND_L) {
    // Left Stick Deadband (50 to 400)
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(42, 8, "L-STICK DEADBAND");
    u8g2.setDrawColor(1);

    char db_buf[28];
    snprintf(db_buf, sizeof(db_buf), "THROTTLE DB: +/-%u", remoteConfig.deadband_l);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(16, 28, db_buf);

    u8g2.drawFrame(16, 36, 96, 6);
    int fill_w = ((remoteConfig.deadband_l - 50) * 94) / 350;
    if (fill_w > 94) fill_w = 94;
    if (fill_w < 0) fill_w = 0;
    if (fill_w > 0) u8g2.drawBox(17, 37, fill_w, 4);
    u8g2.drawVLine(17 + fill_w, 34, 10);

    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "LX: [-/+] 10  |  PR: ENTER");
  } else if (menuState == MENU_STATE_DEADBAND_R) {
    // Right Stick Deadband (50 to 1800 for high-drift filtering)
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(42, 8, "R-STICK DEADBAND");
    u8g2.setDrawColor(1);

    char db_buf[28];
    snprintf(db_buf, sizeof(db_buf), "STEER DB: +/-%u", remoteConfig.deadband_r);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(18, 28, db_buf);

    u8g2.drawFrame(16, 36, 96, 6);
    int fill_w = ((remoteConfig.deadband_r - 50) * 94) / 1750;
    if (fill_w > 94) fill_w = 94;
    if (fill_w < 0) fill_w = 0;
    if (fill_w > 0) u8g2.drawBox(17, 37, fill_w, 4);
    u8g2.drawVLine(17 + fill_w, 34, 10);

    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "LX: [-/+] 50 (MAX 1800) | PR: OK");
  } else if (menuState == MENU_STATE_BOOT_CALIB) {
    // Boot Calibration Screen Toggle
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(36, 8, "BOOT CALIBRATION");
    u8g2.setDrawColor(1);

    u8g2.setFont(u8g2_font_5x7_tr);
    if (remoteConfig.boot_calib == 1) {
      u8g2.drawStr(14, 28, "360* SPIN: [ ENABLED ]");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(10, 42, "Shows calibration screen on boot");
    } else {
      u8g2.drawStr(14, 28, "360* SPIN: [ DISABLED ]");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(10, 42, "Instant boot direct to Main HUD");
    }

    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "PR: TOGGLE  |  PL: BACK");
  } else if (menuState == MENU_STATE_CONTRAST) {
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(42, 8, "OLED CONTRAST");
    u8g2.setDrawColor(1);

    int pct = (remoteConfig.contrast * 100) / 255;
    char ct_buf[24];
    snprintf(ct_buf, sizeof(ct_buf), "BRIGHTNESS: %d%%", pct);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(18, 28, ct_buf);

    u8g2.drawFrame(16, 36, 96, 6);
    int fill_w = (remoteConfig.contrast * 94) / 255;
    if (fill_w > 94) fill_w = 94;
    if (fill_w > 0) u8g2.drawBox(17, 37, fill_w, 4);
    u8g2.drawVLine(17 + fill_w, 34, 10);

    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "LX: NUDGE [-/+]  |  PR: ENTER");
  } else if (menuState == MENU_STATE_RATE) {
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(44, 8, "RF REFRESH RATE");
    u8g2.setDrawColor(1);

    const char* rates[3] = {"[ 20 Hz (ECO/RANGE) ]", "[ 40 Hz (STANDARD)  ]", "[ 50 Hz (SPORT)     ]"};
    uint8_t vals[3] = {20, 40, 50};
    u8g2.setFont(u8g2_font_5x7_tr);
    for (uint8_t r = 0; r < 3; r++) {
      int y = 24 + r * 10;
      if (remoteConfig.refresh_rate == vals[r]) {
        u8g2.drawBox(10, y - 8, 108, 9);
        u8g2.setDrawColor(0);
        u8g2.drawStr(14, y - 1, rates[r]);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(14, y - 1, rates[r]);
      }
    }
    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "LY: SELECT  |  PR: CONFIRM");
  } else if (menuState == MENU_STATE_PROFILE) {
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(42, 8, "DRIVE PROFILES");
    u8g2.setDrawColor(1);

    const char* profs[3] = {"1. ARCADE (2-STK)", "2. TANK DRIVE", "3. 1-STICK (LEFT)"};
    u8g2.setFont(u8g2_font_5x7_tr);
    for (uint8_t p = 0; p < 3; p++) {
      int y = 24 + p * 10;
      if (remoteConfig.drive_profile == p) {
        u8g2.drawBox(8, y - 8, 112, 9);
        u8g2.setDrawColor(0);
        u8g2.drawStr(12, y - 1, profs[p]);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(12, y - 1, profs[p]);
      }
    }
    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "LY: SELECT  |  PR: CONFIRM");
  } else if (menuState == MENU_STATE_NOTCH) {
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(40, 8, "POT NOTCH SOUND");
    u8g2.setDrawColor(1);

    u8g2.setFont(u8g2_font_5x7_tr);
    if (remoteConfig.notch_sound) {
      u8g2.drawStr(16, 32, "SOUND: [ ENABLED ]");
    } else {
      u8g2.drawStr(16, 32, "SOUND: [ MUTED ]");
    }
    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "PR: TOGGLE  |  PL: BACK");
  } else if (menuState == MENU_STATE_CALIB) {
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(34, 8, "JOYSTICK CALIBRATION");
    u8g2.setDrawColor(1);

    char cal_buf[32];
    u8g2.setFont(u8g2_font_5x7_tr);
    if (calibStep == 0) {
      u8g2.drawStr(10, 22, "STEP 1: LEAVE AT REST");
      u8g2.setFont(u8g2_font_4x6_tr);
      snprintf(cal_buf, sizeof(cal_buf), "LX:%u  LY:%u", filt_lx, filt_ly);
      u8g2.drawStr(12, 33, cal_buf);
      snprintf(cal_buf, sizeof(cal_buf), "RX:%u  RY:%u", filt_rx, filt_ry);
      u8g2.drawStr(12, 43, cal_buf);
      u8g2.drawHLine(0, 52, 128);
      u8g2.drawStr(4, 61, "PR: CAPTURE REST CENTER");
    } else if (calibStep == 1) {
      u8g2.drawStr(10, 22, "STEP 2: FULL CIRCLES");
      u8g2.setFont(u8g2_font_4x6_tr);
      snprintf(cal_buf, sizeof(cal_buf), "LX:%u-%u LY:%u-%u", remoteConfig.min_lx, remoteConfig.max_lx, remoteConfig.min_ly, remoteConfig.max_ly);
      u8g2.drawStr(6, 33, cal_buf);
      snprintf(cal_buf, sizeof(cal_buf), "RX:%u-%u RY:%u-%u", remoteConfig.min_rx, remoteConfig.max_rx, remoteConfig.min_ry, remoteConfig.max_ry);
      u8g2.drawStr(6, 43, cal_buf);
      u8g2.drawHLine(0, 52, 128);
      u8g2.drawStr(4, 61, "PR: SAVE LIMITS & EXIT");
    } else {
      u8g2.drawStr(14, 26, "CALIBRATION STORED!");
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(12, 38, "Saved to Flash EEPROM.");
      u8g2.drawStr(12, 47, "Zero baselines synchronized.");
      u8g2.drawHLine(0, 52, 128);
      u8g2.drawStr(4, 61, "PR / PL: EXIT TO MENU");
    }
  } else if (menuState == MENU_STATE_INFO) {
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 7, "< BACK");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(44, 8, "SYSTEM INFORMATION");
    u8g2.setDrawColor(1);

    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(8, 20, "PLATFORM : CYBERROVER X5 HUD OS");
    u8g2.drawStr(8, 28, "ARCHITECT: VEER PRATAP SAW");
    u8g2.drawStr(8, 36, "ACCOLADE : 1ST PRIZE STATE CHAMP");
    u8g2.drawStr(8, 44, "HARDWARE : STM32 72MHz CORTEX-M3");

    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 61, "PL: BACK TO SETTINGS MENU");
  }

  u8g2.sendBuffer();
}

// =============================================================================
//  MENU NAVIGATION & INPUT HANDLER (SAFE ISOLATION)
// =============================================================================
void handleSettingsMenuInput(uint32_t now) {
  static uint32_t lastNavMove = 0;

  // Dynamic Navigation Thresholds tied directly to the pilot's calibrated deadband settings:
  // (Zero hardcoded magic numbers like 35% or 40% — fully adaptive to remoteConfig deadband)
  int deadband_margin_l = max(30, (int)(remoteConfig.deadband_l / 10)); // Variable 10% deadband guard
  int nav_threshold_l   = (int)remoteConfig.deadband_l + deadband_margin_l;
  int diff_ly           = (int)filt_ly - (int)auto_center_ly;
  int diff_lx           = (int)filt_lx - (int)auto_center_lx;

  // Left Stick LY (PB0) controls UP/DOWN; LX (PB1) controls LEFT/RIGHT:
  bool stick_up   = (diff_ly >= nav_threshold_l);   // Forward / Up
  bool stick_down = (diff_ly <= -nav_threshold_l);  // Backward / Down
  bool stick_left = (diff_lx <= -nav_threshold_l);  // Left
  bool stick_right= (diff_lx >= nav_threshold_l);   // Right

  // Button inputs:
  // digInputs[2] = Push Left (PA8)  -> BACK
  // digInputs[3] = Push Right (PA15) -> ENTER / SELECT
  bool btn_enter = digInputs[3].state; // Active-HIGH
  bool btn_back  = digInputs[2].state; // Active-HIGH

  // Right Stick RX (PA4) Redundant Fallback Navigation:
  // Dynamically tied to remoteConfig.deadband_r (Variable threshold, NOT a hardcoded 35%):
  // Deflecting RIGHT beyond deadband boundary -> ENTER / SELECT / GO FRONT
  // Deflecting LEFT beyond deadband boundary  -> BACK / RETURN
  int deadband_margin_r = max(30, (int)(remoteConfig.deadband_r / 10)); // Variable 10% deadband guard
  int nav_threshold_r   = (int)remoteConfig.deadband_r + deadband_margin_r;
  int diff_rx           = (int)filt_rx - (int)auto_center_rx;

  bool rx_stick_right   = (diff_rx >= nav_threshold_r);
  bool rx_stick_left    = (diff_rx <= -nav_threshold_r);

  static bool lastEnterState = false;
  static bool lastBackState  = false;
  static bool lastRxRight    = false;
  static bool lastRxLeft     = false;
  static uint32_t lastRxNavTime = 0;

  bool rxEnterPressed = false;
  bool rxBackPressed  = false;

  if (now - lastRxNavTime >= 200) {
    if (rx_stick_right && !lastRxRight) {
      rxEnterPressed = true;
      lastRxNavTime  = now;
    } else if (rx_stick_left && !lastRxLeft) {
      rxBackPressed  = true;
      lastRxNavTime  = now;
    }
  }
  lastRxRight = rx_stick_right;
  lastRxLeft  = rx_stick_left;

  bool enterPressed = (btn_enter && !lastEnterState) || rxEnterPressed;
  bool backPressed  = (btn_back  && !lastBackState)  || rxBackPressed;
  lastEnterState = btn_enter;
  lastBackState  = btn_back;

  if (menuState == MENU_STATE_ROOT) {
    if (backPressed) {
      // Allow exiting Settings Menu back to Dashboard HUD via Right Stick Left
      currentPage = 0;
      triggerAlert(ALERT_TOGGLE_OFF);
      return;
    }
    if (now - lastNavMove >= 200) {
      if (stick_up) {
        lastNavMove = now;
        if (menuIndex > 0) {
          menuIndex--;
          if (menuIndex < menuScrollTop) menuScrollTop = menuIndex;
          triggerAlert(ALERT_BUTTON_CLICK);
        }
      } else if (stick_down) {
        lastNavMove = now;
        if (menuIndex < MENU_ITEMS_COUNT - 1) {
          menuIndex++;
          if (menuIndex > menuScrollTop + 2) menuScrollTop = menuIndex - 2;
          triggerAlert(ALERT_BUTTON_CLICK);
        }
      }
    }

    if (enterPressed) {
      triggerAlert(ALERT_TOGGLE_ON);
      switch (menuIndex) {
        case 0: menuState = MENU_STATE_CALIB; calibStep = 0; break;
        case 1: menuState = MENU_STATE_DEADBAND_L; break;
        case 2: menuState = MENU_STATE_DEADBAND_R; break;
        case 3: menuState = MENU_STATE_BOOT_CALIB; break;
        case 4: menuState = MENU_STATE_PROFILE; break;
        case 5: menuState = MENU_STATE_RATE; break;
        case 6: menuState = MENU_STATE_CONTRAST; break;
        case 7: menuState = MENU_STATE_NOTCH; break;
        case 8: menuState = MENU_STATE_INFO; break;
      }
    }
  } else {
    // Inside a Submenu:
    if (backPressed) {
      triggerAlert(ALERT_TOGGLE_OFF);
      saveRemoteSettings();
      menuState = MENU_STATE_ROOT;
      return;
    }

    switch (menuState) {
      case MENU_STATE_DEADBAND_L:
        if (now - lastNavMove >= 150) {
          if (stick_left && remoteConfig.deadband_l > 50) {
            lastNavMove = now;
            remoteConfig.deadband_l = (remoteConfig.deadband_l >= 60) ? remoteConfig.deadband_l - 10 : 50;
            triggerAlert(ALERT_POT_NOTCH);
          } else if (stick_right && remoteConfig.deadband_l < 400) {
            lastNavMove = now;
            remoteConfig.deadband_l = (remoteConfig.deadband_l <= 390) ? remoteConfig.deadband_l + 10 : 400;
            triggerAlert(ALERT_POT_NOTCH);
          }
        }
        if (enterPressed) {
          saveRemoteSettings();
          triggerAlert(ALERT_BUTTON_CLICK);
          menuState = MENU_STATE_ROOT;
        }
        break;

      case MENU_STATE_DEADBAND_R:
        if (now - lastNavMove >= 150) {
          if (stick_left && remoteConfig.deadband_r > 50) {
            lastNavMove = now;
            remoteConfig.deadband_r = (remoteConfig.deadband_r >= 100) ? remoteConfig.deadband_r - 50 : 50;
            triggerAlert(ALERT_POT_NOTCH);
          } else if (stick_right && remoteConfig.deadband_r < 1800) {
            lastNavMove = now;
            remoteConfig.deadband_r = (remoteConfig.deadband_r <= 1750) ? remoteConfig.deadband_r + 50 : 1800;
            triggerAlert(ALERT_POT_NOTCH);
          }
        }
        if (enterPressed) {
          saveRemoteSettings();
          triggerAlert(ALERT_BUTTON_CLICK);
          menuState = MENU_STATE_ROOT;
        }
        break;

      case MENU_STATE_BOOT_CALIB:
        if (enterPressed || stick_left || stick_right || stick_up || stick_down) {
          remoteConfig.boot_calib = (remoteConfig.boot_calib == 0) ? 1 : 0;
          triggerAlert(ALERT_POT_NOTCH);
          saveRemoteSettings();
          if (enterPressed) {
            menuState = MENU_STATE_ROOT;
          }
        }
        break;

      case MENU_STATE_CONTRAST:
        if (now - lastNavMove >= 150) {
          if (stick_left && remoteConfig.contrast > 25) {
            lastNavMove = now;
            remoteConfig.contrast = (remoteConfig.contrast >= 50) ? remoteConfig.contrast - 25 : 25;
            u8g2.setContrast(remoteConfig.contrast);
            triggerAlert(ALERT_POT_NOTCH);
          } else if (stick_right && remoteConfig.contrast < 255) {
            lastNavMove = now;
            remoteConfig.contrast = (remoteConfig.contrast <= 230) ? remoteConfig.contrast + 25 : 255;
            u8g2.setContrast(remoteConfig.contrast);
            triggerAlert(ALERT_POT_NOTCH);
          }
        }
        if (enterPressed) {
          saveRemoteSettings();
          triggerAlert(ALERT_BUTTON_CLICK);
          menuState = MENU_STATE_ROOT;
        }
        break;

      case MENU_STATE_RATE:
        if (now - lastNavMove >= 200) {
          if (stick_left || stick_up) {
            lastNavMove = now;
            if (remoteConfig.refresh_rate == 50) remoteConfig.refresh_rate = 40;
            else if (remoteConfig.refresh_rate == 40) remoteConfig.refresh_rate = 20;
            triggerAlert(ALERT_POT_NOTCH);
          } else if (stick_right || stick_down) {
            lastNavMove = now;
            if (remoteConfig.refresh_rate == 20) remoteConfig.refresh_rate = 40;
            else if (remoteConfig.refresh_rate == 40) remoteConfig.refresh_rate = 50;
            triggerAlert(ALERT_POT_NOTCH);
          }
        }
        if (enterPressed) {
          saveRemoteSettings();
          triggerAlert(ALERT_BUTTON_CLICK);
          menuState = MENU_STATE_ROOT;
        }
        break;

      case MENU_STATE_PROFILE:
        if (now - lastNavMove >= 200) {
          if (stick_up) {
            lastNavMove = now;
            if (remoteConfig.drive_profile > 0) {
              remoteConfig.drive_profile--;
              triggerAlert(ALERT_POT_NOTCH);
            }
          } else if (stick_down) {
            lastNavMove = now;
            if (remoteConfig.drive_profile < 2) {
              remoteConfig.drive_profile++;
              triggerAlert(ALERT_POT_NOTCH);
            }
          }
        }
        if (enterPressed) {
          saveRemoteSettings();
          triggerAlert(ALERT_BUTTON_CLICK);
          menuState = MENU_STATE_ROOT;
        }
        break;

      case MENU_STATE_NOTCH:
        if (now - lastNavMove >= 200) {
          if (stick_up || stick_right) {
            lastNavMove = now;
            if (!remoteConfig.notch_sound) {
              remoteConfig.notch_sound = 1;
              triggerAlert(ALERT_POT_NOTCH);
            }
          } else if (stick_down || stick_left) {
            lastNavMove = now;
            if (remoteConfig.notch_sound) {
              remoteConfig.notch_sound = 0;
              triggerAlert(ALERT_POT_NOTCH);
            }
          }
        }
        if (enterPressed) {
          saveRemoteSettings();
          triggerAlert(ALERT_BUTTON_CLICK);
          menuState = MENU_STATE_ROOT;
        }
        break;

      case MENU_STATE_CALIB:
        if (calibStep == 0) {
          if (enterPressed) {
            // Multi-sample averaging (32 samples) to eliminate ADC noise and ripple
            uint32_t s_lx = 0, s_ly = 0, s_rx = 0, s_ry = 0;
            for (uint8_t i = 0; i < 32; i++) {
              s_lx += analogRead(PIN_STICK_LX);
              s_ly += analogRead(PIN_STICK_LY);
              int raw_rx = analogRead(PIN_STICK_RX);
              s_rx += (raw_rx <= 4095) ? (4095 - raw_rx) : 2048;
              s_ry += analogRead(PIN_STICK_RY);
              delay(2);
            }
            remoteConfig.center_lx = (uint16_t)(s_lx / 32);
            remoteConfig.center_ly = (uint16_t)(s_ly / 32);
            remoteConfig.center_rx = (uint16_t)(s_rx / 32);
            remoteConfig.center_ry = (uint16_t)(s_ry / 32);

            // Synchronize live active running zero-centers immediately!
            auto_center_lx = remoteConfig.center_lx;
            auto_center_ly = remoteConfig.center_ly;
            auto_center_rx = remoteConfig.center_rx;
            auto_center_ry = remoteConfig.center_ry;
            auto_center_done = true;

            // Reset min/max bounds to current center baselines
            remoteConfig.min_lx = remoteConfig.center_lx;
            remoteConfig.max_lx = remoteConfig.center_lx;
            remoteConfig.min_ly = remoteConfig.center_ly;
            remoteConfig.max_ly = remoteConfig.center_ly;
            remoteConfig.min_rx = remoteConfig.center_rx;
            remoteConfig.max_rx = remoteConfig.center_rx;
            remoteConfig.min_ry = remoteConfig.center_ry;
            remoteConfig.max_ry = remoteConfig.center_ry;

            calibStep = 1;
            triggerAlert(ALERT_BUTTON_CLICK);
          }
        } else if (calibStep == 1) {
          // Continuously track full stick circle limits across all 4 axes
          uint16_t cur_lx = analogRead(PIN_STICK_LX);
          uint16_t cur_ly = analogRead(PIN_STICK_LY);
          int raw_rx      = analogRead(PIN_STICK_RX);
          uint16_t cur_rx = (raw_rx <= 4095) ? (4095 - raw_rx) : 2048;
          uint16_t cur_ry = analogRead(PIN_STICK_RY);

          if (cur_lx < remoteConfig.min_lx) remoteConfig.min_lx = cur_lx;
          if (cur_lx > remoteConfig.max_lx) remoteConfig.max_lx = cur_lx;
          if (cur_ly < remoteConfig.min_ly) remoteConfig.min_ly = cur_ly;
          if (cur_ly > remoteConfig.max_ly) remoteConfig.max_ly = cur_ly;
          if (cur_rx < remoteConfig.min_rx) remoteConfig.min_rx = cur_rx;
          if (cur_rx > remoteConfig.max_rx) remoteConfig.max_rx = cur_rx;
          if (cur_ry < remoteConfig.min_ry) remoteConfig.min_ry = cur_ry;
          if (cur_ry > remoteConfig.max_ry) remoteConfig.max_ry = cur_ry;

          if (enterPressed) {
            saveRemoteSettings();
            // Re-affirm live active centers
            auto_center_lx = remoteConfig.center_lx;
            auto_center_ly = remoteConfig.center_ly;
            auto_center_rx = remoteConfig.center_rx;
            auto_center_ry = remoteConfig.center_ry;
            calibStep = 2;
            triggerAlert(ALERT_LINK_HANDSHAKE);
          }
        } else if (calibStep == 2) {
          if (enterPressed || backPressed) {
            menuState = MENU_STATE_ROOT;
          }
        }
        break;

      case MENU_STATE_INFO:
        if (enterPressed || backPressed) {
          menuState = MENU_STATE_ROOT;
        }
        break;
    }
  }
}


// =============================================================================
//  BOOT ANIMATION: UPiR FLUID CHARGING BATTERY WITH "CYBERROVER" BRANDING
// =============================================================================
void playBootAnimation() {
  const int xoff = 29; // Centered horizontally on 128px screen: (128 - 70) / 2 = 29
  const int yoff = 18; // Battery vertical position (height = 40px, bottom at y = 58)

  u8g2.setBitmapMode(1); // Transparent bitmap overlay mode
  u8g2.setFontMode(1);   // Transparent font background mode

  // Sampling accumulators for auto-zero resting center calibration
  uint32_t sum_lx = 0;
  uint32_t sum_ly = 0;
  uint32_t sum_rx = 0;
  uint32_t sum_ry = 0;
  uint16_t sample_count = 0;

  int anim_frame = 0;
  for (int battery_fill_perc = 0; battery_fill_perc <= 100; battery_fill_perc += 2) {
    u8g2.clearBuffer();

    // Sample analog joystick axes at rest across animation frames (pilot leaves sticks untouched)
    for (uint8_t s = 0; s < 4; s++) {
      sum_lx += analogRead(PIN_STICK_LX);
      sum_ly += analogRead(PIN_STICK_LY);
      int raw_rx = analogRead(PIN_STICK_RX);
      sum_rx += (raw_rx <= 4095) ? (4095 - raw_rx) : 2048;
      sum_ry += analogRead(PIN_STICK_RY);
      sample_count++;
    }

    // 1. "CYBERROVER" typography cleanly centered directly above the battery
    u8g2.setFont(u8g2_font_5x7_tr);
    int text_w = u8g2.getStrWidth("CYBERROVER");
    u8g2.drawStr((128 - text_w) / 2, 12, "CYBERROVER");

    // 2. Outer battery shell outline (70x40px)
    u8g2.drawXBMP(xoff, yoff, 70, 40, epd_bitmap_battery_outline);

    // 3. Dynamic liquid fill body (fills from 4px to 61px)
    int fill_width = map(battery_fill_perc, 0, 100, 4, 61);
    u8g2.drawRBox(xoff + 3, yoff + 3, fill_width, 34, 1);

    // 4. Fluid metaball / blob wave animation at the charging front
    u8g2.setClipWindow(xoff + 3, yoff + 3, xoff + 3 + 61, yoff + 3 + 34);
    u8g2.drawXBMP(xoff + 3 + fill_width - 1, yoff + 3, 32, 34, epd_bitmap_allArray[anim_frame]);
    u8g2.setMaxClipWindow();

    // Redraw battery outline to preserve crisp border definition
    u8g2.drawXBMP(xoff, yoff, 70, 40, epd_bitmap_battery_outline);

    // 5. Centered Dynamic Percentage Counter with Pixel-Inverted Contrast (XOR Mode)
    // White background liquid inverts text pixels to black; dark background inverts to white!
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%d%%", battery_fill_perc);
    u8g2.setFont(u8g2_font_helvB14_tr);
    int pct_w = u8g2.getStrWidth(pct_str);
    int pct_x = xoff + 3 + (61 - pct_w) / 2; // Perfectly centered horizontally in 61px chamber
    int pct_y = yoff + 3 + 23;              // Perfectly centered vertically in 34px chamber
    u8g2.setFontMode(1);                    // MANDATORY: Transparent font mode (suppresses background box so only glyph strokes invert!)
    u8g2.setDrawColor(2);                   // 2 = XOR Invert Mode (Per-pixel automatic contrast)
    u8g2.drawStr(pct_x, pct_y, pct_str);
    u8g2.setDrawColor(1);                   // Restore standard white draw color
    u8g2.setFontMode(0);                    // Restore default font mode

    u8g2.sendBuffer();

    anim_frame = (anim_frame + 1) % epd_bitmap_allArray_LEN;
  }

  // Calculate resting zero-offsets from collected boot samples
  if (sample_count > 0) {
    auto_center_lx = (uint16_t)(sum_lx / sample_count);
    auto_center_ly = (uint16_t)(sum_ly / sample_count);
    auto_center_rx = (uint16_t)(sum_rx / sample_count);
    auto_center_ry = (uint16_t)(sum_ry / sample_count);
    auto_center_done = true;

    // Synchronize remoteConfig centers with measured resting values
    remoteConfig.center_lx = auto_center_lx;
    remoteConfig.center_ly = auto_center_ly;
    remoteConfig.center_rx = auto_center_rx;
    remoteConfig.center_ry = auto_center_ry;
  }

  // Hold 100% full battery state briefly before transition
  delay(150);
}

// =============================================================================
//  BOOT JOYSTICK 360° SPIN CALIBRATION SCREEN (DIRECT TRANSITION FROM BOOT)
// =============================================================================
void runBootStickCalibration() {
  bool done = false;
  uint32_t lastFrame = 0;
  Serial.println(F("\n[BOOT CALIB] Swirl sticks 360* -> Press PR to Save & Drive | PL to Skip"));

  while (!done) {
    uint32_t now = millis();

    // Check Buttons
    bool pr = (digitalRead(PIN_PUSH_RIGHT) == HIGH);
    bool pl = (digitalRead(PIN_PUSH_LEFT) == HIGH);

    static bool lastPr = false;
    static bool lastPl = false;
    bool prClick = (pr && !lastPr);
    bool plClick = (pl && !lastPl);
    lastPr = pr;
    lastPl = pl;

    // PL = Skip to Drive immediately with saved EEPROM settings
    if (plClick) {
      triggerAlert(ALERT_BUTTON_CLICK);
      done = true;
      break;
    }

    // Read raw stick values
    uint16_t c_lx = analogRead(PIN_STICK_LX);
    uint16_t c_ly = analogRead(PIN_STICK_LY);
    int r_rx      = analogRead(PIN_STICK_RX);
    uint16_t c_rx = (r_rx <= 4095) ? (4095 - r_rx) : 2048;
    uint16_t c_ry = analogRead(PIN_STICK_RY);

    // Expand bounds during 360° swirl
    if (c_lx < remoteConfig.min_lx) remoteConfig.min_lx = c_lx;
    if (c_lx > remoteConfig.max_lx) remoteConfig.max_lx = c_lx;
    if (c_ly < remoteConfig.min_ly) remoteConfig.min_ly = c_ly;
    if (c_ly > remoteConfig.max_ly) remoteConfig.max_ly = c_ly;
    if (c_rx < remoteConfig.min_rx) remoteConfig.min_rx = c_rx;
    if (c_rx > remoteConfig.max_rx) remoteConfig.max_rx = c_rx;
    if (c_ry < remoteConfig.min_ry) remoteConfig.min_ry = c_ry;
    if (c_ry > remoteConfig.max_ry) remoteConfig.max_ry = c_ry;

    if (prClick) {
      // Ensure valid min/max spans (minimum 300 span from center)
      if (remoteConfig.min_lx >= auto_center_lx - 100) remoteConfig.min_lx = (auto_center_lx > 1000) ? auto_center_lx - 1000 : 0;
      if (remoteConfig.max_lx <= auto_center_lx + 100) remoteConfig.max_lx = (auto_center_lx < 3095) ? auto_center_lx + 1000 : 4095;
      if (remoteConfig.min_ly >= auto_center_ly - 100) remoteConfig.min_ly = (auto_center_ly > 1000) ? auto_center_ly - 1000 : 0;
      if (remoteConfig.max_ly <= auto_center_ly + 100) remoteConfig.max_ly = (auto_center_ly < 3095) ? auto_center_ly + 1000 : 4095;
      if (remoteConfig.min_rx >= auto_center_rx - 100) remoteConfig.min_rx = (auto_center_rx > 1000) ? auto_center_rx - 1000 : 0;
      if (remoteConfig.max_rx <= auto_center_rx + 100) remoteConfig.max_rx = (auto_center_rx < 3095) ? auto_center_rx + 1000 : 4095;
      if (remoteConfig.min_ry >= auto_center_ry - 100) remoteConfig.min_ry = (auto_center_ry > 1000) ? auto_center_ry - 1000 : 0;
      if (remoteConfig.max_ry <= auto_center_ry + 100) remoteConfig.max_ry = (auto_center_ry < 3095) ? auto_center_ry + 1000 : 4095;

      for (uint8_t ch = 0; ch < 4; ch++) {
        filterStates[ch].init = false;
      }

      saveRemoteSettings();
      triggerAlert(ALERT_LINK_HANDSHAKE);

      // Brief visual confirmation banner
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(12, 32, "CALIBRATION SAVED!");
      u8g2.sendBuffer();
      delay(300);

      done = true;
      break;
    }

    // OLED Render at 25 FPS
    if (now - lastFrame >= 40) {
      lastFrame = now;
      u8g2.clearBuffer();

      u8g2.drawBox(0, 0, 128, 10);
      u8g2.setDrawColor(0);
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(2, 7, "PL: SKIP");
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(34, 8, "STICK CALIBRATION");
      u8g2.setDrawColor(1);

      u8g2.drawStr(16, 22, "SPIN GIMBALS 360*");
      u8g2.setFont(u8g2_font_4x6_tr);
      char buf[32];
      snprintf(buf, sizeof(buf), "LX:%u-%u  LY:%u-%u", remoteConfig.min_lx, remoteConfig.max_lx, remoteConfig.min_ly, remoteConfig.max_ly);
      u8g2.drawStr(6, 33, buf);
      snprintf(buf, sizeof(buf), "RX:%u-%u  RY:%u-%u", remoteConfig.min_rx, remoteConfig.max_rx, remoteConfig.min_ry, remoteConfig.max_ry);
      u8g2.drawStr(6, 43, buf);
      u8g2.drawHLine(0, 52, 128);
      u8g2.drawStr(4, 61, "PR: SAVE & DRIVE  |  PL: SKIP");
      u8g2.sendBuffer();
    }
  }
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
#if defined(Serial1)
  Serial1.begin(115200);
#endif
  delay(500);

  // 1. Release JTAG to allow PB3, PB4, PA15 to act as standard digital GPIO
  releaseJTAG();

  // 1.5. Configure Digital Inputs with dedicated pull-ups / pull-downs (ELIMINATES GHOST TOUCHES & FLOATING PINS)
  pinMode(PIN_TOGGLE_LEFT,  INPUT_PULLUP);   // PB3: Active-LOW (LOW = ARM, HIGH = PARK)
  pinMode(PIN_TOGGLE_RIGHT, INPUT_PULLUP);   // PB4: Active-LOW (LOW = MUTE, HIGH = SOUND)
  pinMode(PIN_STICK_CLK_L,  INPUT_PULLUP);   // PA12: Active-LOW
  pinMode(PIN_STICK_CLK_R,  INPUT_PULLUP);   // PA11: Active-LOW
  pinMode(PIN_PUSH_LEFT,    INPUT_PULLUP);   // PA8: Ground-switched with internal 3.3V pull-up reference
  pinMode(PIN_PUSH_RIGHT,   INPUT_PULLUP);   // PA15: Ground-switched with internal 3.3V pull-up reference

  // Arming Safety Interlock: If powered up while Toggle Left is already in ARM position, require cycling to PARK first
  if (digitalRead(PIN_TOGGLE_LEFT) == LOW) {
    armSafetyLockout = true;
  }

  // 2. Configure 12-bit ADC resolution and Hardware I2C Pins explicitly
  #if defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__)
    analogReadResolution(12);
    Wire.setSCL(OLED_SCL_PIN);
    Wire.setSDA(OLED_SDA_PIN);
  #endif

  // 3. Configure Hardware I2C & U8g2 Display
  Wire.begin();
  u8g2.begin();
  u8g2.setBusClock(400000);
  Wire.setClock(400000); // Enforce true 400 kHz Fast I2C after u8g2 init
  loadRemoteSettings();
  u8g2.setContrast(remoteConfig.contrast);

  // 3.5. Fluid Battery Boot Animation with Auto-Centering Sample Collection
  playBootAnimation();

  // 4.5. Joystick Calibration Screen (Conditional on boot_calib setting or button overrides)
  bool bypassCalib = (digitalRead(PIN_PUSH_LEFT) == HIGH);  // Hold PL on boot to bypass directly to HUD
  bool forceCalib  = (digitalRead(PIN_PUSH_RIGHT) == HIGH); // Hold PR on boot to force calibration screen
  if ((remoteConfig.boot_calib == 1 && !bypassCalib) || forceCalib) {
    runBootStickCalibration();
  }

  // 5. Configure Actuator & Heartbeat Pins
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_HAPTIC, OUTPUT);
  pinMode(PIN_LED_HB, OUTPUT);
  setOutputs(false, false);
  digitalWrite(PIN_LED_HB, HIGH); // Off for active-low PC13

  // Immediate Startup Confirmation (Audio / Haptic)
  playStartupConfirmation();
}

// =============================================================================
//  MAIN LOOP
// =============================================================================
uint32_t lastTxTime        = 0;
uint32_t lastOledTime      = 0;
uint32_t lastPrintTime     = 0;
uint32_t lastHeartbeat     = 0;
uint32_t frameCounter      = 0;
uint32_t lastLinkLossAlert = 0;
uint32_t lastAckSuccess    = 0;

bool lastTxAck             = false;
bool wasLinkConnected      = false;
bool turboActive           = false;

void loop() {
  uint32_t now = millis();

  // Non-blocking Audio-Haptic Engine update
  updateAlertEngine();

  // ====================================================================
  //  1. CHECK DIGITAL EVENTS & MUTE SWITCH (35ms Rock-Solid Debounce)
  // ====================================================================
  for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++) {
    bool raw = digitalRead(digInputs[i].pin);
    if (raw != digInputs[i].lastRaw) {
      digInputs[i].lastRaw = raw;
      digInputs[i].lastDebounceTime = now;
    }

    if ((now - digInputs[i].lastDebounceTime) >= 35) {
      bool isCurrentActive = (digInputs[i].lastRaw == digInputs[i].activeLevel);

      if (i == 0 && armSafetyLockout) {
        // Toggle Left (PB3): When pilot physically moves switch to PARK (HIGH), clear safety lockout!
        if (!isCurrentActive) {
          armSafetyLockout = false;
        }
      }

      if (isCurrentActive != digInputs[i].state) {
        digInputs[i].state = isCurrentActive;

        if (digInputs[i].isToggle) {
          if (isCurrentActive) {
            triggerAlert(ALERT_TOGGLE_ON);
          } else {
            triggerAlert(ALERT_TOGGLE_OFF);
          }
        } else {
          if (isCurrentActive) {
            // If in Settings Menu (Page 4), suppress generic button click for PA8/PA15 so menu tones remain crisp
            if (currentPage != 3 || (i != 2 && i != 3)) {
              triggerAlert(ALERT_BUTTON_CLICK);
            }

            // Left Stick Click (PA12) -> TURBO MODE TOGGLE (Instant 100% Speed)
            if (i == 4) {
              turboActive = !turboActive;
              if (turboActive) {
                triggerAlert(ALERT_TURBO_KICK);
              } else {
                triggerAlert(ALERT_BUTTON_CLICK);
              }
            }
          }
        }
      }
    }
  }

  bool isMuted = digInputs[1].state; // Toggle Right PB4 is Mute
  bool isArmed = (currentPage != 3 && !armSafetyLockout && digInputs[0].state); // Toggle Left PB3 is Arm

  // Retain last commanded turn ratio for Serial Monitor reporting
  static uint8_t last_turn_ratio = 0;

  // ====================================================================
  //  1.5. READ & CONDITION ANALOG INPUTS & PAGE SELECTION (Every Cycle)
  // ====================================================================
  // Read and Condition Joysticks with 3-Stage Filtering (alpha = 0.70)
  filt_lx = applyAnalogFilter(0, analogRead(PIN_STICK_LX), 0.70f);
  filt_ly = applyAnalogFilter(1, analogRead(PIN_STICK_LY), 0.70f);
  int raw_rx        = analogRead(PIN_STICK_RX);
  uint16_t inv_rx   = (raw_rx <= 4095) ? (4095 - raw_rx) : 0; // Inverted for 90° mount
  filt_rx = applyAnalogFilter(2, inv_rx, 0.70f);
  filt_ry = applyAnalogFilter(3, analogRead(PIN_STICK_RY), 0.70f);

  // RIGHT JOYSTICK HARDWARE CROSS-TALK ISOLATION & LARGE DEADBAND DRIFT FILTER
  int diff_lx = abs((int)filt_lx - (int)auto_center_lx);
  int diff_ly = abs((int)filt_ly - (int)auto_center_ly);
  if (diff_lx <= (int)remoteConfig.deadband_l) filt_lx = auto_center_lx;
  if (diff_ly <= (int)remoteConfig.deadband_l) filt_ly = auto_center_ly;

  if (remoteConfig.drive_profile == 0 || remoteConfig.drive_profile == 2) {
    filt_ry = auto_center_ry; // Fully isolate vertical axis
    int diff_rx = abs((int)filt_rx - (int)auto_center_rx);
    if (diff_rx <= (int)remoteConfig.deadband_r) {
      filt_rx = auto_center_rx;
    }
  } else {
    int diff_rx = abs((int)filt_rx - (int)auto_center_rx);
    int diff_ry = abs((int)filt_ry - (int)auto_center_ry);
    if (diff_rx <= (int)remoteConfig.deadband_r) filt_rx = auto_center_rx;
    if (diff_ry <= (int)remoteConfig.deadband_r) filt_ry = auto_center_ry;
  }

  // Compute Calibrated Normalized Percentages (-100% to +100%)
  live_lx_pct  = calculateStickPercent(filt_lx, auto_center_lx, remoteConfig.deadband_l, remoteConfig.min_lx, remoteConfig.max_lx);
  live_thr_pct = calculateStickPercent(filt_ly, auto_center_ly, remoteConfig.deadband_l, remoteConfig.min_ly, remoteConfig.max_ly);
  live_str_pct = calculateStickPercent(filt_rx, auto_center_rx, remoteConfig.deadband_r, remoteConfig.min_rx, remoteConfig.max_rx);

  // Read and Condition 3 Potentiometers (alpha = 0.25, Inverted: Clockwise = 100% Full)
  int raw_pot_l      = analogRead(PIN_POT_LEFT);
  uint16_t inv_pot_l = (raw_pot_l <= 4095) ? (4095 - raw_pot_l) : 0;
  txPacket.pot_l     = applyAnalogFilter(4, inv_pot_l, 0.25f);   // Speed Governor

  int raw_pot_c      = analogRead(PIN_POT_CENTER);
  uint16_t inv_pot_c = (raw_pot_c <= 4095) ? (4095 - raw_pot_c) : 0;
  txPacket.pot_c     = applyAnalogFilter(5, inv_pot_c, 0.25f);   // Turning Radius (0% to 75%)

  int raw_pot_r      = analogRead(PIN_POT_RIGHT);
  uint16_t inv_pot_r = (raw_pot_r <= 4095) ? (4095 - raw_pot_r) : 0;
  txPacket.pot_r     = applyAnalogFilter(6, inv_pot_r, 0.25f);   // Display Selector Knob (PA2)

  // Dynamic Display Page Selection via Pot Right (PA2) with 100-count hysteresis
  static uint16_t lastPotR = 0;
  if (abs((int)txPacket.pot_r - (int)lastPotR) > 80) {
    lastPotR = txPacket.pot_r;
    uint8_t targetPage = currentPage;
    if (txPacket.pot_r < 950) {
      targetPage = 0; // Page 1: Dashboard HUD
    } else if (txPacket.pot_r > 1050 && txPacket.pot_r < 1950) {
      targetPage = 1; // Page 2: 7-Channel Spectrum Analyzer
    } else if (txPacket.pot_r > 2050 && txPacket.pot_r < 2950) {
      targetPage = 2; // Page 3: Combat Powertrain MFD
    } else if (txPacket.pot_r > 3050) {
      targetPage = 3; // Page 4: Settings Menu
    }

    if (targetPage != currentPage) {
      if (currentPage == 3 && menuState != MENU_STATE_ROOT) {
        saveRemoteSettings();
        menuState = MENU_STATE_ROOT;
      }
      currentPage = targetPage;
      triggerAlert(ALERT_POT_NOTCH);
    }
  }

  // Haptic Notches for Speed Governor (Pot L) and Turn Radius (Pot C)
  static uint32_t lastPotNotchTime = 0;
  uint16_t potValues[2] = { txPacket.pot_l, txPacket.pot_c };
  for (uint8_t p = 0; p < 2; p++) {
    int notch = potValues[p] / 205; // 20 discrete notches
    if (potTrackers[p].lastNotch == -1) {
      potTrackers[p].lastNotch   = notch;
      potTrackers[p].lastReading = potValues[p];
    } else {
      if (notch != potTrackers[p].lastNotch && abs((int)potValues[p] - potTrackers[p].lastReading) > 120) {
        potTrackers[p].lastNotch   = notch;
        potTrackers[p].lastReading = potValues[p];
        if (remoteConfig.notch_sound && (now - lastPotNotchTime >= 150)) {
          lastPotNotchTime = now;
          triggerAlert(ALERT_POT_NOTCH);
        }
      }
    }
  }

  // Dynamic Turning Radius Computation from Center Pot (PA1)
  uint8_t turn_ratio = (uint8_t)map(txPacket.pot_c, 0, 4095, 0, 75);

  // ====================================================================
  //  1.6. SETTINGS MENU INPUT PROCESSING (When on Page 4)
  // ====================================================================
  if (currentPage == 3) {
    handleSettingsMenuInput(now);
  }

  // ====================================================================
  //  1.7. RECEIVE LIVE RETURN TELEMETRY FROM ESP32-CAM (USART1 PA10)
  //  Format: $RTL,<rssi>,<cur_l_ma>,<cur_r_ma>,<bat_mv>,<failsafe>*
  // ====================================================================
  static char telemRxBuf[64];
  static uint8_t telemRxIdx = 0;

  auto processTelemByte = [&](char c) {
    if (c == '$') {
      telemRxIdx = 0;
      telemRxBuf[telemRxIdx++] = c;
    } else if (telemRxIdx > 0 && telemRxIdx < sizeof(telemRxBuf) - 1) {
      telemRxBuf[telemRxIdx++] = c;
      if (c == '*' || c == '\n') {
        telemRxBuf[telemRxIdx] = '\0';
        if (strncmp(telemRxBuf, "$RTL,", 5) == 0) {
          char* p = telemRxBuf + 5;
          telem_rssi      = (int16_t)strtol(p, &p, 10); if (*p == ',') p++;
          telem_cur_l     = (float)strtol(p, &p, 10) / 1000.0f; if (*p == ',') p++;
          telem_cur_r     = (float)strtol(p, &p, 10) / 1000.0f; if (*p == ',') p++;
          telem_bat_v     = (float)strtol(p, &p, 10) / 1000.0f; if (*p == ',') p++;
          telem_failsafe  = (uint8_t)strtol(p, &p, 10);
          lastTelemRxTime = now;
          if (!telem_link_ok) {
            telem_link_ok = true;
            if (!wasLinkConnected) {
              wasLinkConnected = true;
              triggerAlert(ALERT_LINK_HANDSHAKE);
            }
          }
        }
        telemRxIdx = 0;
      }
    }
  };

  while (Serial.available()) {
    processTelemByte((char)Serial.read());
  }
#if defined(Serial1)
  while (Serial1.available()) {
    processTelemByte((char)Serial1.read());
  }
#endif

  if (telem_link_ok && (now - lastTelemRxTime > 2500)) {
    telem_link_ok = false;
    if (wasLinkConnected) {
      wasLinkConnected = false;
      triggerAlert(ALERT_LINK_LOST);
    }
  }

  // ====================================================================
  //  2. TELEOPERATION PACKET TRANSMISSION (Configured Rate: 20/40/50 Hz)
  // ====================================================================
  static bool wasInSettings = false;
  if (currentPage == 3) {
    if (!wasInSettings) {
      wasInSettings = true;
      // Send one single neutral emergency stop frame so rover halts instantly and safely
      CyberPacket stopPkt;
      memset(&stopPkt, 0, sizeof(CyberPacket));
      stopPkt.device_id    = CYBER_DEVICE_ID;
      stopPkt.seq_id       = (uint8_t)(frameCounter & 0xFF);
      stopPkt.timestamp_ms = (uint16_t)(now & 0xFFFF);
      stopPkt.frame_id     = (uint16_t)(++frameCounter & 0xFFFF);
      stopPkt.speed_gov    = 15;
      setCyberButtons(stopPkt, false, isMuted, false, false, false, false, 3);
      setCyberModeFlags(stopPkt, remoteConfig.drive_profile, remoteConfig.refresh_rate);
      stopPkt.crc8         = computeCyberChecksum(stopPkt);

      uint8_t stopFrame[19];
      stopFrame[0]  = CYBER_FRAME_HEADER_1; // 0x5A
      stopFrame[1]  = CYBER_FRAME_HEADER_2; // 0xA5
      memcpy(&stopFrame[2], &stopPkt, sizeof(CyberPacket));
      stopFrame[18] = CYBER_FRAME_TRAILER;  // 0xED
      Serial.write(stopFrame, 19);
#if defined(Serial1)
      Serial1.write(stopFrame, 19);
      Serial1.flush();
#endif
      Serial.flush();
    }
    // STRICTLY SUPPRESS CONTINUOUS TELEOP TRANSMISSIONS WHILE IN SETTINGS MENU!
  } else {
    wasInSettings = false;

    uint32_t txInterval = 1000 / (remoteConfig.refresh_rate > 0 ? remoteConfig.refresh_rate : 50);
    if (now - lastTxTime >= txInterval) {
      if (now - lastTxTime < 2 * txInterval) {
        lastTxTime += txInterval; // Phase-locked rate maintenance
      } else {
        lastTxTime = now;
      }

      // Assemble Calibrated Payload for Rover (Transmits 0-4095 with exact 2048 center)
      if (remoteConfig.drive_profile == 0) { // Profile 0: ARCADE (2-STICK: Left Y Throttle, Right X Steering)
        live_trk_l_pct    = live_thr_pct;
        live_trk_r_pct    = 0; // Strictly quarantined to zero in Arcade Mode!
        txPacket.stick_ly = mapPercentToADC(live_thr_pct);
        txPacket.stick_rx = mapPercentToADC(live_str_pct);
        txPacket.stick_lx = 2048; // Centered / quarantined for drive
        txPacket.stick_ry = 2048; // Strictly quarantined: isolates hardware crosstalk on Veer's gimbal
      } else if (remoteConfig.drive_profile == 1) { // Profile 1: TANK DRIVE (2-STICK: Left Y Track L, Right Y Track R)
        live_trk_l_pct    = live_thr_pct;
        live_trk_r_pct    = calculateStickPercent(filt_ry, auto_center_ry, remoteConfig.deadband_r, remoteConfig.min_ry, remoteConfig.max_ry);
        txPacket.stick_ly = mapPercentToADC(live_trk_l_pct);
        txPacket.stick_ry = mapPercentToADC(live_trk_r_pct);
        txPacket.stick_lx = 2048; // Centered
        txPacket.stick_rx = 2048; // Centered
      } else { // Profile 2: 1-STICK (LEFT STICK ONLY: Left Y Throttle, Left X Steering)
        live_trk_l_pct    = live_thr_pct;
        live_trk_r_pct    = 0;
        live_str_pct      = live_lx_pct; // Left Stick X steers the rover!
        txPacket.stick_ly = mapPercentToADC(live_thr_pct);
        txPacket.stick_rx = mapPercentToADC(live_lx_pct); // Transmitted as vehicle steering channel
        txPacket.stick_lx = 2048;
        txPacket.stick_ry = 2048; // Completely quarantine erratic Right Stick!
      }

      // Pack 6 Digital Channels
      txPacket.toggle_l    = digInputs[0].state ? 1 : 0; // Arm / Disarm
      txPacket.toggle_r    = isMuted ? 1 : 0;            // Mute Flag
      txPacket.push_l      = digInputs[2].state ? 1 : 0;
      txPacket.push_r      = digInputs[3].state ? 1 : 0;
      txPacket.stick_clk_l = digInputs[4].state ? 1 : 0;
      txPacket.stick_clk_r = 0; // Isolated broken gimbal tactile switch

      txPacket.frame_id    = ++frameCounter;

      // TRANSMIT COMPACT 16-BYTE CYBERPACKET (19-Byte UART Frame @ 20/40/50 Hz)
      CyberPacket cyberPkt;
      cyberPkt.device_id    = CYBER_DEVICE_ID;
      cyberPkt.seq_id       = (uint8_t)(frameCounter & 0xFF);
      cyberPkt.timestamp_ms = (uint16_t)(now & 0xFFFF);
      cyberPkt.axis_lx      = (int8_t)live_lx_pct;
      cyberPkt.axis_ly      = (int8_t)live_thr_pct;
      cyberPkt.axis_rx      = (int8_t)live_str_pct;
      cyberPkt.axis_ry      = (int8_t)live_trk_r_pct;

      float current_gov   = 0.15f + 0.85f * ((float)txPacket.pot_l / 4095.0f);
      if (current_gov > 1.0f)  current_gov = 1.0f;
      if (current_gov < 0.15f) current_gov = 0.15f;
      cyberPkt.speed_gov  = (uint8_t)(current_gov * 100.0f);
      cyberPkt.turn_ratio = (uint8_t)constrain((int)turn_ratio, 0, 75);
      cyberPkt.pot_aux    = (uint8_t)constrain((int)((txPacket.pot_r * 100) / 4095), 0, 100);

      bool isArmedState   = (digInputs[0].state && currentPage != 3 && !armSafetyLockout);
      setCyberButtons(cyberPkt,
                      isArmedState,
                      isMuted,
                      digInputs[2].state,  // PL
                      digInputs[3].state,  // PR
                      digInputs[4].state,  // CKL / Turbo
                      false,               // CKR decommissioned
                      currentPage);
      setCyberModeFlags(cyberPkt, remoteConfig.drive_profile, remoteConfig.refresh_rate);
      cyberPkt.frame_id   = (uint16_t)(frameCounter & 0xFFFF);
      cyberPkt.crc8       = computeCyberChecksum(cyberPkt);

      uint8_t cyberFrame[19];
      cyberFrame[0]  = CYBER_FRAME_HEADER_1; // 0x5A
      cyberFrame[1]  = CYBER_FRAME_HEADER_2; // 0xA5
      memcpy(&cyberFrame[2], &cyberPkt, sizeof(CyberPacket));
      cyberFrame[18] = CYBER_FRAME_TRAILER;  // 0xED
      Serial.write(cyberFrame, 19);
#if defined(Serial1)
      Serial1.write(cyberFrame, 19);
#endif

      last_turn_ratio = turn_ratio;
    }
  }

  // ====================================================================
  //  3. DYNAMIC SPEED GOVERNOR & DASHBOARD COMPUTATION
  // ====================================================================
  float governor_factor = 0.15f + 0.85f * ((float)txPacket.pot_l / 4095.0f);
  if (governor_factor > 1.0f) governor_factor = 1.0f;
  if (governor_factor < 0.15f) governor_factor = 0.15f;

  float max_governed_speed = MAX_SPEED_CMPS * governor_factor;
  float max_governed_rpm   = MAX_MOTOR_RPM  * governor_factor;

  // Determine Throttle Deflection
  float throttle_ratio = 0.0f;
  bool is_forward = false;
  bool is_reverse = false;

  if (currentPage != 3) {
    if (remoteConfig.drive_profile == 0 || remoteConfig.drive_profile == 2) { // Profile 0: 2-Stick Arcade | Profile 2: 1-Stick Left
      if (live_thr_pct > 0) {
        is_forward = true;
        throttle_ratio = (float)live_thr_pct / 100.0f;
      } else if (live_thr_pct < 0) {
        is_reverse = true;
        throttle_ratio = (float)(-live_thr_pct) / 100.0f;
      }
    } else { // Profile 1: TANK DRIVE
      int avg_trk = (abs(live_trk_l_pct) + abs(live_trk_r_pct)) / 2;
      throttle_ratio = (float)avg_trk / 100.0f;
      if ((live_trk_l_pct + live_trk_r_pct) < 0) {
        is_reverse = true;
      } else if (avg_trk > 0) {
        is_forward = true;
      }
    }
    if (throttle_ratio > 1.0f) throttle_ratio = 1.0f;
  }

  float target_speed = 0.0f;
  float target_rpm   = 0.0f;

  if (currentPage != 3 && (is_forward || is_reverse)) {
    target_speed = throttle_ratio * max_governed_speed;
    target_rpm   = throttle_ratio * max_governed_rpm;
  }

  char drive_mode = 'P';
  if (currentPage == 3 || !digInputs[0].state || armSafetyLockout) {
    drive_mode = 'P'; // Disarmed / Park Mode ('P')
  } else if (is_reverse) {
    drive_mode = 'R'; // Reverse Mode ('R')
  } else {
    drive_mode = 'D'; // Drive Mode Armed ('D')
  }

  // Fluid, frame-rate independent exponential gauge smoothing (0.8s time constant):
  static uint32_t lastSmoothTick = 0;
  if (lastSmoothTick == 0) lastSmoothTick = now;
  float dt = (float)(now - lastSmoothTick) / 1000.0f;
  lastSmoothTick = now;
  if (dt > 0.1f) dt = 0.1f; // Clamp in case of large interval
  float smooth_alpha = 1.0f - expf(-4.5f * dt);
  speed_smooth += (target_speed - speed_smooth) * smooth_alpha;
  rpm_smooth   += (target_rpm   - rpm_smooth)   * smooth_alpha;

  // Steering Blinkers & Turn Indicators:
  // Arcade Mode: Right Stick X (STR). Pushing Left (< -15%) -> Blinker Left (◄).
  //                                  Pushing Right (> +15%) -> Blinker Right (►).
  // Tank Mode  : Track Differential (TRK_L - TRK_R). Right track faster -> Turn Left (◄).
  //                                                  Left track faster -> Turn Right (►).
  bool blinker_l = false;
  bool blinker_r = false;
  if (currentPage != 3) {
    if (remoteConfig.drive_profile == 0 || remoteConfig.drive_profile == 2) {
      blinker_l = (live_str_pct < -15);
      blinker_r = (live_str_pct > 15);
    } else {
      int diff = live_trk_l_pct - live_trk_r_pct;
      blinker_l = (diff <= -25); // Right track faster -> Vehicle turns Left
      blinker_r = (diff >= 25);  // Left track faster -> Vehicle turns Right
    }
  }
  bool blink_state = ((now / 250) % 2 == 0);

  // ====================================================================
  //  4. NON-BLOCKING OLED DASHBOARD REFRESH (10 FPS = 100ms interval)
  //  Tuned to 10 FPS to guarantee 100% CPU priority for 50 Hz RF teleop
  // ====================================================================
  if (now - lastOledTime >= 100) {
    lastOledTime = now;
    if (currentPage == 0) {
      renderDashboardHUD(drive_mode, governor_factor, txPacket.pot_c, txPacket.pot_r,
                         blinker_l, blinker_r, blink_state, telem_link_ok, isMuted, turboActive);
    } else if (currentPage == 1) {
      renderSpectrumAnalyzer(filt_lx, filt_ly,
                             filt_rx, filt_ry,
                             txPacket.pot_l, txPacket.pot_c, txPacket.pot_r);
    } else if (currentPage == 2) {
      renderCompactTelemetryPage(txPacket, telem_link_ok, isMuted, now);
    } else {
      renderSettingsMenuPage();
    }
  }
  // ====================================================================
  //  5. HEARTBEAT LED (1 Hz blink)
  // ====================================================================
  if (now - lastHeartbeat >= 500) {
    lastHeartbeat = now;
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
  }
}