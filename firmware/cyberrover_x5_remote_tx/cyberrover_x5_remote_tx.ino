/*
 * ================================================================================
 * CYBERROVER X5 — 13-CHANNEL REMOTE TRANSMITTER FIRMWARE v1.3 (HUD OS)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras, Jharkhand
 * Platform       : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Handheld Remote Controller Transmitter Node
 * Display Target : 1.3" I2C OLED (SH1106 @ PB6/PB7) via U8g2 Full Framebuffer
 *
 * MAJOR UPGRADES (v1.3):
 *   - PAGE 1 VIRTUAL COCKPIT HUD (UPiR Automotive Cluster Architecture):
 *     * Left Dial  : Moving arc for Vehicle Speed (cm/s) + Left Turn Blinker.
 *     * Right Dial : Mirrored moving arc for Motor RPM (0-600) + Right Turn Blinker.
 *     * Center Top : Coolant / Temp icon + Decorative arch dots + RF Heartbeat.
 *     * Center Mid : Colon ':' + Drive Mode (P / N / D / R / T) + Dynamic power hashes.
 *     * Center Btm : Gas pump icon + Curved Fuel/Battery gauge arc (E <-> F).
 *     * Bottom Rgt : UPiR / X5 watermark signature.
 *   - DYNAMIC SPEED GOVERNOR INTEGRATION (Pot Left: PA0):
 *     * Live scaling: Governs top speed from 15% crawl (~47 cm/s) to 100% sprint (314 cm/s).
 *     * Full stick deflection accurately respects governor ceiling on both numbers and arcs!
 *   - NON-BLOCKING DUAL-LOOP ARCHITECTURE:
 *     * NRF24 RF Link : 40 Hz (25ms) packet transmission on Channel 108.
 *     * U8g2 Display  : 25 FPS (40ms) non-blocking refresh via 400 kHz Fast I2C.
 *     * Audio-Haptic  : Phase-driven zero-overhead event state machine.
 *
 * HARDWARE PIN WIRING:
 *   ANALOG (7 Channels):
 *     1. PB1  ---> Left Stick X    (ADC9) [Steering / Turn Blinkers]
 *     2. PB0  ---> Left Stick Y    (ADC8) [Throttle / Speed & RPM]
 *     3. PA4  ---> Right Stick X   (ADC4) [Inverted: 4095 - raw_rx for 90° mount]
 *     4. PA3  ---> Right Stick Y   (ADC3) [Cam Tilt]
 *     5. PA0  ---> Pot Left        (ADC0) [Speed Governor]
 *     6. PA1  ---> Pot Center      (ADC1) [Camera Tilt Trim]
 *     7. PA2  ---> Pot Right       (ADC2) [Auxiliary / Battery Sim]
 *
 *   DIGITAL (6 Active Channels):
 *     1. PB4  ---> Toggle Left     (Active-LOW  : LOW = Armed,  HIGH = Park/Safe)
 *     2. PB5  ---> Toggle Right    (Active-LOW  : LOW = Tank Mode, HIGH = Normal)
 *     3. PA15 ---> Push Left       (Active-HIGH : HIGH = PRESSED, LOW = RELEASED) [NC]
 *     4. PB3  ---> Push Right      (Active-HIGH : HIGH = PRESSED, LOW = RELEASED) [NC]
 *     5. PA11 ---> Left Stick Click(Active-LOW  : LOW = PRESSED, HIGH = UP)
 *     6. PA12 ---> Right Stick Click(Active-LOW : LOW = PRESSED, HIGH = UP)
 *
 *   I2C OLED DISPLAY (1.3" SH1106 / 0.96" SSD1306):
 *     - PB6   ---> SCL (Hardware I2C1 Clock @ 400 kHz)
 *     - PB7   ---> SDA (Hardware I2C1 Data)
 *     - 5V    ---> VCC (from MT3608 Boost Converter)
 *     - GND   ---> GND
 *
 *   ACTUATORS & INDICATORS:
 *     - PB8   ---> Piezo Buzzer
 *     - PB9   ---> Haptic Vibration Motor (via Transistor/MOSFET)
 *     - PC13  ---> Onboard Heartbeat LED (Active-LOW)
 *
 *   NRF24L01+ RF TRANSCEIVER (Native SPI1):
 *     - PA5   ---> SCK   (Hardware SPI1 Clock)
 *     - PA6   ---> MISO  (Hardware SPI1 Master-In-Slave-Out)
 *     - PA7   ---> MOSI  (Hardware SPI1 Master-Out-Slave-In)
 *     - PB10  ---> CE    (Chip Enable)
 *     - PB11  ---> CSN   (Chip Select Not)
 *     - 3.3V  ---> VCC   (with 10uF - 100uF capacitor to GND)
 *     - GND   ---> GND
 * ================================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <U8g2lib.h>

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
#define PIN_STICK_LX    PB1
#define PIN_STICK_LY    PB0
#define PIN_STICK_RX    PA4
#define PIN_STICK_RY    PA3
#define PIN_POT_LEFT    PA0
#define PIN_POT_CENTER  PA1
#define PIN_POT_RIGHT   PA2

// 6 Active Digital Pins
#define PIN_TOGGLE_LEFT  PB4
#define PIN_TOGGLE_RIGHT PB5
#define PIN_PUSH_LEFT    PA15
#define PIN_PUSH_RIGHT   PB3
#define PIN_STICK_CLK_L  PA11
#define PIN_STICK_CLK_R  PA12

// Actuator Pins
#define PIN_BUZZER       PB8
#define PIN_HAPTIC       PB9
#define PIN_LED_HB       PC13

// NRF24L01 Control Pins
#define PIN_NRF_CE       PB10
#define PIN_NRF_CSN      PB11

// Deadband and Rover Limits
#define ADC_CENTER      2048
#define ADC_DEADBAND    140   // 1908 to 2188 reads zero throttle
#define MAX_SPEED_CMPS  314   // 314 cm/s = 11.31 km/h top sprint
#define MAX_MOTOR_RPM   600   // 600 RPM gearhead output

// =============================================================================
//  RADIO & DATA PACKET DEFINITIONS
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
  uint8_t  toggle_l;    // 1 = ON, 0 = OFF
  uint8_t  toggle_r;    // 1 = ON, 0 = OFF
  uint8_t  push_l;      // 1 = PRESSED, 0 = RELEASED
  uint8_t  push_r;      // 1 = PRESSED, 0 = RELEASED
  uint8_t  stick_clk_l; // 1 = PRESSED, 0 = UP
  uint8_t  stick_clk_r; // 1 = PRESSED, 0 = UP
  uint32_t frame_id;    // Rolling frame sequence counter
};

RoverPacket txPacket;
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
bool radioReady = false;

// =============================================================================
//  DIGITAL CONTROL TRACKING
// =============================================================================
struct DigitalInput {
  uint8_t pin;
  const char* name;
  const char* tag;
  bool isToggle;
  bool activeLevel;
  bool state;
};

DigitalInput digInputs[] = {
  {PIN_TOGGLE_LEFT,  "Toggle Left  (PB4) ", "TL",  true,  LOW,  false},
  {PIN_TOGGLE_RIGHT, "Toggle Right (PB5) ", "TR",  true,  LOW,  false},
  {PIN_PUSH_LEFT,    "Push Left    (PA15)", "PL",  false, HIGH, false},
  {PIN_PUSH_RIGHT,   "Push Right   (PB3) ", "PR",  false, HIGH, false},
  {PIN_STICK_CLK_L,  "Stick Click L(PA11)", "CKL", false, LOW,  false},
  {PIN_STICK_CLK_R,  "Stick Click R(PA12)", "CKR", false, LOW,  false}
};

const uint8_t NUM_DIG_INPUTS = sizeof(digInputs) / sizeof(digInputs[0]);

// =============================================================================
//  POTENTIOMETER TRACKING (Left, Center, Right)
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
//  BITMAPPED GRAPHICS ASSETS (UPiR CLUSTER HUD)
// =============================================================================

// Left Turn Blinker Arrow (8x7 px)
static const unsigned char PROGMEM bitmap_arrow_left[] = {
  0x10, 0x30, 0x70, 0xFF, 0x70, 0x30, 0x10
};

// Right Turn Blinker Arrow (8x7 px)
static const unsigned char PROGMEM bitmap_arrow_right[] = {
  0x08, 0x0C, 0x0E, 0xFF, 0x0E, 0x0C, 0x08
};

// Coolant / Engine Temperature Thermometer Icon (9x9 px)
static const unsigned char PROGMEM bitmap_temp_icon[] = {
  0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x38, 0x00, 0x38, 0x00,
  0x54, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0x7C, 0x00
};

// Gas Pump / Fuel Icon (8x8 px)
static const unsigned char PROGMEM bitmap_fuel_icon[] = {
  0x70, 0x5A, 0x7A, 0x5E, 0x52, 0x72, 0x70, 0xFE
};

// Authentic UPiR Logo Watermark (16x4 px)
static const unsigned char PROGMEM bitmap_upir_logo[] = {
  0x15, 0xD7, 0x15, 0x45, 0x15, 0x96, 0x19, 0x15
};

// =============================================================================
//  HIGH-EFFICIENCY PHASE-DRIVEN AUDIO-HAPTIC ENGINE
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

AlertEvent currentAlert   = ALERT_NONE;
uint32_t   alertStartTime = 0;
uint8_t    alertPhase     = 0;

void setOutputs(bool buzzer, bool haptic) {
  digitalWrite(PIN_BUZZER, buzzer ? HIGH : LOW);
  digitalWrite(PIN_HAPTIC, haptic ? HIGH : LOW);
}

void playStartupBeep() {
  Serial.println(F(">>> [SOUND] System Ready Beep..."));
  digitalWrite(PIN_BUZZER, HIGH);
  digitalWrite(PIN_HAPTIC, HIGH);
  delay(80);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_HAPTIC, LOW);
}

void triggerAlert(AlertEvent evt) {
  if (currentAlert == ALERT_LINK_HANDSHAKE && (evt == ALERT_POT_NOTCH || evt == ALERT_TURBO_KICK)) {
    return;
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
      setOutputs(false, true);
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
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 75) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_TOGGLE_OFF:
      if (alertPhase == 0 && elapsed >= 20) {
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 50) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_BUTTON_CLICK:
      if (alertPhase == 0 && elapsed >= 30) {
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 70) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_POT_NOTCH:
      if (alertPhase == 0 && elapsed >= 12) {
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 40) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_TURBO_KICK:
      if (elapsed >= 120) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    case ALERT_LINK_LOST:
      if (alertPhase == 0 && elapsed >= 50) {
        setOutputs(false, false);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 85) {
        setOutputs(true, true);
        alertPhase = 2;
      } else if (alertPhase == 2 && elapsed >= 135) {
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
//  JTAG RELEASE ROUTINE (Frees PA15, PB3, PB4 as normal GPIO)
// =============================================================================
void releaseJTAG() {
  #if defined(__HAL_RCC_AFIO_CLK_ENABLE) && defined(__HAL_AFIO_REMAP_SWJ_NOJTAG)
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
  #elif defined(AFIO_MAPR_SWJ_CFG_JTAGDISABLE)
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
  #elif defined(disableDebugPorts)
    disableDebugPorts();
  #endif
}

// =============================================================================
//  OLED INSTRUMENT CLUSTER RENDERING HELPERS
// =============================================================================
float speed_smooth = 0.0f;
float rpm_smooth   = 0.0f;
float fuel_smooth  = 88.0f;

// Draws an anti-aliased solid arc between start_deg and end_deg
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

// Draws the outer track tick marks and dots around gauge dial
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

// Renders the complete Page 1 Virtual Cockpit HUD
void renderDashboardHUD(char drive_mode, float governor_factor, bool blinker_l, bool blinker_r, bool blink_state, bool rf_connected) {
  u8g2.clearBuffer();

  // ---------------------------------------------------------------------------
  // 1. LEFT GAUGE: SPEEDOMETER (cm/s)
  // ---------------------------------------------------------------------------
  const int cx_l = 30;
  const int cy_l = 34;
  const int r_in_l = 21;
  const int r_out_l = 24;
  const int r_track_l = 26;

  // Outer track (215 deg down to 35 deg = 180 deg span)
  drawDialTrack(cx_l, cy_l, r_track_l, 35.0f, 215.0f);

  // Moving Arc (fills along speed relative to absolute max)
  float speed_frac = speed_smooth / (float)MAX_SPEED_CMPS;
  if (speed_frac > 1.0f) speed_frac = 1.0f;
  if (speed_frac < 0.02f) speed_frac = 0.02f; // Minimum resting tick

  float speed_current_deg = 215.0f - (speed_frac * 180.0f);
  drawThickArc(cx_l, cy_l, r_in_l, r_out_l, speed_current_deg, 215.0f);

  // Governor Limit Tick on outer rim (shows currently dialed max speed cap)
  float gov_deg = 215.0f - (governor_factor * 180.0f);
  float gov_rad = gov_deg * 0.0174532925f;
  int gx1 = cx_l + (int)(25 * cos(gov_rad) + 0.5f);
  int gy1 = cy_l - (int)(25 * sin(gov_rad) + 0.5f);
  int gx2 = cx_l + (int)(28 * cos(gov_rad) + 0.5f);
  int gy2 = cy_l - (int)(28 * sin(gov_rad) + 0.5f);
  u8g2.drawLine(gx1, gy1, gx2, gy2);

  // Speed Digital Readout (Logisoso16 Modern Clean Numerals)
  int display_speed_val = (int)(speed_smooth + 0.5f);
  char speed_str[8];
  itoa(display_speed_val, speed_str, 10);
  u8g2.setFont(u8g2_font_logisoso16_tn);
  int speed_str_w = u8g2.getStrWidth(speed_str);
  u8g2.drawStr(cx_l - (speed_str_w / 2), 40, speed_str);

  // Speed Unit Sub-label
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx_l - 8, 48, "CM/S");

  // Left Blinker Arrow
  if (blinker_l && blink_state) {
    u8g2.drawXBMP(4, 3, 8, 7, bitmap_arrow_left);
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

  // Outer track (-35 deg up to 145 deg)
  drawDialTrack(cx_r, cy_r, r_track_r, -35.0f, 145.0f);

  // Moving Arc
  float rpm_frac = rpm_smooth / (float)MAX_MOTOR_RPM;
  if (rpm_frac > 1.0f) rpm_frac = 1.0f;
  if (rpm_frac < 0.02f) rpm_frac = 0.02f;

  float rpm_current_deg = -35.0f + (rpm_frac * 180.0f);
  drawThickArc(cx_r, cy_r, r_in_r, r_out_r, -35.0f, rpm_current_deg);

  // RPM Digital Readout
  int display_rpm_val = (int)(rpm_smooth + 0.5f);
  char rpm_str[8];
  itoa(display_rpm_val, rpm_str, 10);
  u8g2.setFont(u8g2_font_logisoso16_tn);
  int rpm_str_w = u8g2.getStrWidth(rpm_str);
  u8g2.drawStr(cx_r - (rpm_str_w / 2), 40, rpm_str);

  // RPM Unit Sub-label
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx_r - 6, 48, "RPM");

  // Right Blinker Arrow
  if (blinker_r && blink_state) {
    u8g2.drawXBMP(116, 3, 8, 7, bitmap_arrow_right);
  } else {
    u8g2.drawPixel(116, 6);
    u8g2.drawPixel(123, 6);
  }

  // ---------------------------------------------------------------------------
  // 3. CENTER COLUMN: STATUS & TELEMETRY HUD
  // ---------------------------------------------------------------------------
  // A. Coolant / Temp Icon
  u8g2.drawXBMP(60, 4, 9, 9, bitmap_temp_icon);
  u8g2.drawPixel(58, 2);
  u8g2.drawPixel(64, 1);
  u8g2.drawPixel(70, 2);

  // RF Heartbeat / Link Dot
  if (rf_connected) {
    u8g2.drawDisc(64, 15, 1); // Solid dot when paired
  } else if (blink_state) {
    u8g2.drawCircle(64, 15, 1); // Hollow flash when disconnected
  }

  // B. Colon & Drive Mode Indicator
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(62, 29, ":");

  char mode_str[2] = { drive_mode, '\0' };
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(61, 39, mode_str);

  // Dynamic Power Hashes ('///') scaling with current throttle
  int num_hashes = (int)(speed_frac * 4.0f + 0.5f);
  if (num_hashes < 1) num_hashes = 1;
  for (int h = 0; h < num_hashes; h++) {
    int hx = 72 + h * 3;
    int hy = 43 - h * 2;
    u8g2.drawLine(hx, hy, hx + 2, hy - 4);
  }

  // C. Fuel Pump Icon & Curved Gauge Arc
  u8g2.drawXBMP(60, 46, 8, 8, bitmap_fuel_icon);

  const int cx_fuel = 64;
  const int cy_fuel = 40;
  const int r_fuel  = 20;

  // Track arc & E/F ticks
  for (float fa = 245.0f; fa <= 295.0f; fa += 2.0f) {
    float frad = fa * 0.0174532925f;
    int fx = cx_fuel + (int)(r_fuel * cos(frad) + 0.5f);
    int fy = cy_fuel - (int)(r_fuel * sin(frad) + 0.5f);
    u8g2.drawPixel(fx, fy);
  }
  u8g2.drawLine(52, 57, 52, 60);
  u8g2.drawLine(76, 57, 76, 60);

  // Fuel Fill Arc
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

  // D. Authentic UPiR / X5 Logo Watermark
  u8g2.drawXBMP(110, 58, 16, 4, bitmap_upir_logo);

  // Send Framebuffer to Glass
  u8g2.sendBuffer();
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. Release JTAG to allow PB4, PB3, PA15 to act as standard digital GPIO
  releaseJTAG();

  // 2. Configure 12-bit ADC resolution (0 - 4095)
  #if defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__)
    analogReadResolution(12);
    Wire.setSCL(OLED_SCL_PIN);
    Wire.setSDA(OLED_SDA_PIN);
  #endif

  // 3. Configure Hardware I2C & U8g2 Display
  Wire.begin();
  Wire.setClock(400000); // 400 kHz Fast I2C
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.clearBuffer();

  // Crisp Startup Splash
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(16, 28, "CYBERROVER X5");
  u8g2.drawStr(24, 42, "INITIALIZING...");
  u8g2.sendBuffer();

  // 4. Configure Digital Inputs with internal pull-ups
  for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++) {
    pinMode(digInputs[i].pin, INPUT_PULLUP);
    bool raw = digitalRead(digInputs[i].pin);
    digInputs[i].state = (raw == digInputs[i].activeLevel);
  }

  // 5. Configure Actuator & Heartbeat Pins
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_HAPTIC, OUTPUT);
  pinMode(PIN_LED_HB, OUTPUT);
  setOutputs(false, false);
  digitalWrite(PIN_LED_HB, HIGH); // Off for active-low PC13

  // 6. Initialize NRF24L01 Module
  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — 13-CHANNEL REMOTE TRANSMITTER (HUD OS v1.3)                   "));
  Serial.println(F("  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           "));
  Serial.println(F("================================================================================"));
  Serial.print(F(">>> Initializing NRF24L01+ Radio on SPI1... "));

  SPI.begin();

  if (radio.begin()) {
    radio.setPALevel(RF24_PA_LOW);           // Bench power (safe from brownout)
    radio.setDataRate(RF24_250KBPS);         // 250 kbps for max range & noise rejection
    radio.setChannel(RF_CHANNEL);            // Channel 108 (2.508 GHz)
    radio.setRetries(5, 15);                 // 1500us delay, 15 retries (maximum RF reliability)
    radio.setPayloadSize(sizeof(RoverPacket));
    radio.openWritingPipe(RF_PIPE);
    radio.stopListening();                   // Transmitter mode
    radioReady = true;
    Serial.println(F("[PASSED - SILICON ACTIVE]"));
    Serial.print(F(">>> Broadcasting on Channel "));
    Serial.print(RF_CHANNEL);
    Serial.print(F(" | Speed: 250 kbps | Loop: 40 Hz (25ms) | Payload: "));
    Serial.print(sizeof(RoverPacket));
    Serial.println(F(" Bytes"));
  } else {
    Serial.println(F("[FAILED!] Check 3.3V power, 10uF cap, and SPI1 wiring!"));
    radioReady = false;
  }

  Serial.println(F("================================================================================"));
  Serial.println(F("System Ready! Live transmission & stream running below (115200 baud)...         "));
  Serial.println();

  // Trigger Clean Startup Beep (80ms industrial confirmation)
  playStartupBeep();
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
  //  1. CHECK DIGITAL EVENTS (Toggles, Buttons, Gimbal Clicks)
  // ====================================================================
  for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++) {
    bool raw = digitalRead(digInputs[i].pin);
    bool isCurrentActive = (raw == digInputs[i].activeLevel);

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
          triggerAlert(ALERT_BUTTON_CLICK);
        }
      }

      Serial.print(F(">>> [EVENT] "));
      Serial.print(digInputs[i].name);
      Serial.print(F(" -> "));
      Serial.println(isCurrentActive ? F("[ ACTIVE / ON  ]") : F("[ IDLE / OFF ]"));
    }
  }

  // ====================================================================
  //  2. READ INPUTS & TRANSMIT RF PACKET (Every 25ms = 40 Hz)
  // ====================================================================
  if (now - lastTxTime >= 25) {
    lastTxTime = now;

    // Read Joysticks (12-bit: 0 - 4095)
    txPacket.stick_lx = analogRead(PIN_STICK_LX);
    txPacket.stick_ly = analogRead(PIN_STICK_LY);
    int raw_rx        = analogRead(PIN_STICK_RX);
    txPacket.stick_rx = (raw_rx <= 4095) ? (4095 - raw_rx) : 0; // Inverted for 90° mount
    txPacket.stick_ry = analogRead(PIN_STICK_RY);

    // Read 3 Potentiometers
    txPacket.pot_l    = analogRead(PIN_POT_LEFT);   // Speed Governor
    txPacket.pot_c    = analogRead(PIN_POT_CENTER); // Camera Tilt
    txPacket.pot_r    = analogRead(PIN_POT_RIGHT);  // Aux / Battery test

    // Haptic Notches for ALL 3 Potentiometers
    uint16_t potValues[3] = { txPacket.pot_l, txPacket.pot_c, txPacket.pot_r };
    for (uint8_t p = 0; p < 3; p++) {
      int notch = potValues[p] / 205; // 20 discrete notches (5% each)
      if (potTrackers[p].lastNotch == -1) {
        potTrackers[p].lastNotch   = notch;
        potTrackers[p].lastReading = potValues[p];
      } else {
        if (notch != potTrackers[p].lastNotch && abs((int)potValues[p] - potTrackers[p].lastReading) > 65) {
          potTrackers[p].lastNotch   = notch;
          potTrackers[p].lastReading = potValues[p];
          triggerAlert(ALERT_POT_NOTCH);
        }
      }
    }

    // Full Throttle Turbo Kick (past 90% stick deflection)
    bool isInTurboZone = (txPacket.stick_ly > 3750 || txPacket.stick_ly < 350);
    if (isInTurboZone) {
      if (!turboActive) {
        turboActive = true;
        triggerAlert(ALERT_TURBO_KICK);
      }
    } else if (txPacket.stick_ly > 700 && txPacket.stick_ly < 3400) {
      turboActive = false;
    }

    // Read 6 Digital Channels
    txPacket.toggle_l    = digInputs[0].state ? 1 : 0;
    txPacket.toggle_r    = digInputs[1].state ? 1 : 0;
    txPacket.push_l      = digInputs[2].state ? 1 : 0;
    txPacket.push_r      = digInputs[3].state ? 1 : 0;
    txPacket.stick_clk_l = digInputs[4].state ? 1 : 0;
    txPacket.stick_clk_r = digInputs[5].state ? 1 : 0;

    txPacket.frame_id    = ++frameCounter;

    // Transmit over NRF24L01+
    if (radioReady) {
      lastTxAck = radio.write(&txPacket, sizeof(RoverPacket));

      if (lastTxAck) {
        lastAckSuccess = now;
        if (!wasLinkConnected) {
          wasLinkConnected = true;
          triggerAlert(ALERT_LINK_HANDSHAKE);
          Serial.println(F("\n>>> [RF LINK ESTABLISHED] Paired to CyberRover X5! <<<\n"));
        }
      } else {
        if (wasLinkConnected && (now - lastAckSuccess > 1000)) {
          if (now - lastLinkLossAlert >= 2500) {
            lastLinkLossAlert = now;
            triggerAlert(ALERT_LINK_LOST);
          }
        }
      }
    }
  }

  // ====================================================================
  //  3. DYNAMIC SPEED GOVERNOR & DASHBOARD COMPUTATION
  // ====================================================================
  // Speed Governor scaling:
  // Dial CCW (0)    -> 15% crawl limit (~47 cm/s, 90 RPM)
  // Dial CW  (4095) -> 100% full sprint (314 cm/s, 600 RPM)
  float governor_factor = 0.15f + 0.85f * ((float)txPacket.pot_l / 4095.0f);
  if (governor_factor > 1.0f) governor_factor = 1.0f;
  if (governor_factor < 0.15f) governor_factor = 0.15f;

  float max_governed_speed = MAX_SPEED_CMPS * governor_factor;
  float max_governed_rpm   = MAX_MOTOR_RPM  * governor_factor;

  // Determine Drive Mode & Target Speed/RPM
  char drive_mode = 'N';
  float target_speed = 0.0f;
  float target_rpm   = 0.0f;

  if (!digInputs[0].state) {
    // Toggle Left is OFF (Disarmed) -> Park Mode 'P'
    drive_mode = 'P';
    target_speed = 0.0f;
    target_rpm   = 0.0f;
  } else if (digInputs[1].state) {
    // Toggle Right is ON -> Tank Turn Mode 'T'
    drive_mode = 'T';
    float ratio = abs((int)txPacket.stick_ly - ADC_CENTER) / 2048.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    target_speed = ratio * (max_governed_speed * 0.70f);
    target_rpm   = ratio * (max_governed_rpm   * 0.70f);
  } else if (txPacket.stick_ly > (ADC_CENTER + ADC_DEADBAND)) {
    // Forward -> Drive Mode 'D'
    drive_mode = 'D';
    float ratio = (float)(txPacket.stick_ly - (ADC_CENTER + ADC_DEADBAND)) / (4095.0f - (ADC_CENTER + ADC_DEADBAND));
    if (ratio > 1.0f) ratio = 1.0f;
    target_speed = ratio * max_governed_speed;
    target_rpm   = ratio * max_governed_rpm;
  } else if (txPacket.stick_ly < (ADC_CENTER - ADC_DEADBAND)) {
    // Reverse -> Reverse Mode 'R'
    drive_mode = 'R';
    float ratio = (float)((ADC_CENTER - ADC_DEADBAND) - txPacket.stick_ly) / (ADC_CENTER - ADC_DEADBAND);
    if (ratio > 1.0f) ratio = 1.0f;
    target_speed = ratio * (max_governed_speed * 0.75f);
    target_rpm   = ratio * (max_governed_rpm   * 0.75f);
  } else {
    // Centered inside deadband -> Neutral 'N'
    drive_mode = 'N';
    target_speed = 0.0f;
    target_rpm   = 0.0f;
  }

  // Smooth Speed & RPM with Exponential Moving Average (EMA)
  speed_smooth += (target_speed - speed_smooth) * 0.22f;
  rpm_smooth   += (target_rpm   - rpm_smooth)   * 0.22f;

  // Steering Blinkers
  bool blinker_l = (txPacket.stick_lx < (ADC_CENTER - 500));
  bool blinker_r = (txPacket.stick_lx > (ADC_CENTER + 500));
  bool blink_state = ((now / 250) % 2 == 0); // 2 Hz flash cadence

  // ====================================================================
  //  4. NON-BLOCKING OLED DASHBOARD REFRESH (25 FPS = 40ms interval)
  // ====================================================================
  if (now - lastOledTime >= 40) {
    lastOledTime = now;
    renderDashboardHUD(drive_mode, governor_factor, blinker_l, blinker_r, blink_state, wasLinkConnected && lastTxAck);
  }

  // ====================================================================
  //  5. PERIODIC LIVE TELEMETRY STREAM (Every 100ms = 10 Hz)
  // ====================================================================
  if (now - lastPrintTime >= 100) {
    lastPrintTime = now;

    Serial.print(F("[FRM #"));
    Serial.print(txPacket.frame_id);
    Serial.print(F(" | RF:"));
    if (radioReady) {
      Serial.print(lastTxAck ? F("ACK_OK") : F("TX_NO_ACK"));
    } else {
      Serial.print(F("OFFLINE"));
    }
    Serial.print(F("] "));

    Serial.print(F("MODE:"));
    Serial.print(drive_mode);
    Serial.print(F(" | SPD:"));
    Serial.print((int)speed_smooth);
    Serial.print(F("/"));
    Serial.print((int)max_governed_speed);
    Serial.print(F(" cm/s | RPM:"));
    Serial.print((int)rpm_smooth);
    Serial.print(F(" | GOV:"));
    Serial.print((int)(governor_factor * 100));
    Serial.print(F("%"));

    Serial.print(F(" || POTS: [L:"));
    Serial.print(txPacket.pot_l);
    Serial.print(F(" C:"));
    Serial.print(txPacket.pot_c);
    Serial.print(F(" R:"));
    Serial.print(txPacket.pot_r);
    Serial.print(F("]"));

    Serial.println();
  }

  // ====================================================================
  //  6. HEARTBEAT LED (1 Hz blink)
  // ====================================================================
  if (now - lastHeartbeat >= 500) {
    lastHeartbeat = now;
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
  }
}
