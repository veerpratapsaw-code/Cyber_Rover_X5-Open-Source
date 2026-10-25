/*
 * ================================================================================
 * CYBERROVER X5 — HIGH-EFFICIENCY ESP-NOW RECEIVER & STM32 BRIDGE (ESP32-S3) v3.5
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target Hardware: ESP32-S3 DevKit (N16R8 / R8N16 DevKit)
 * Subsystem      : Autonomous Radio Gateway, Bi-Directional RSSI & STM32 Bridge
 *
 * NEW IN v3.5:
 *   1. Hardware Channel Lock: Enforces Wi-Fi Channel 1 via 'esp_wifi_set_channel()'.
 *   2. Bi-Directional Packet Return: Transmits 'RoverFeedbackPacket' back to Dongle.
 *   3. Auto-Pairing: Automatically registers sender MAC and replies with live RSSI.
 *   4. Low-Latency Forwarding: Dispatches 5-byte frame to STM32 UART1 in < 10us.
 *   5. Clean HUD Logging: Relays STM32 motor execution states while rate-limiting $TEL.
 * ================================================================================
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#define ESPNOW_WIFI_CHANNEL 1

// Hardware UART1 to STM32 Blue Pill Motor Supervisor
#define PIN_UART_TX   17  // S3 TX1 -> STM32 PA10 (RX)
#define PIN_UART_RX   18  // S3 RX1 <- STM32 PA9  (TX)

#define FAILSAFE_TIMEOUT_MS  800  // 800ms dead-man timeout: 40 packet cycles of jitter immunity @ 50 Hz

uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

enum CommandID : uint8_t {
  CMD_STOP        = 0,
  CMD_FORWARD     = 1,
  CMD_BACKWARD    = 2,
  CMD_PIVOT_LEFT  = 3,
  CMD_PIVOT_RIGHT = 4,
  CMD_LEFT_FRONT  = 5,
  CMD_RIGHT_FRONT = 6,
  CMD_LEFT_BACK   = 7,
  CMD_RIGHT_BACK  = 8
};

static const char* const CMD_NAMES[] = {
  "STOP/NEUTRAL ", "FORWARD      ", "BACKWARD     ",
  "PIVOT_LEFT   ", "PIVOT_RIGHT  ", "FRONT_LEFT   ",
  "FRONT_RIGHT  ", "BACK_LEFT    ", "BACK_RIGHT   "
};

static const char CMD_CHARS[] = { 'S', 'F', 'B', 'Q', 'E', 'L', 'R', 'G', 'I' };

#pragma pack(push, 1)
struct GamerControlPacket {
  uint32_t packet_id;  // 4 Bytes: Monotonic frame counter
  uint8_t  cmd_id;     // 1 Byte : Numeric Command ID (0 to 8)
  uint8_t  speed;      // 1 Byte : Integer PWM (0 to 255)
  uint8_t  gear;       // 1 Byte : Integer Gear (0 to 9)
  uint8_t  turbo;      // 1 Byte : Integer Flag (0 = Normal, 1 = Turbo 100%)
};
#pragma pack(pop)
static_assert(sizeof(GamerControlPacket) == 8, "Error: GamerControlPacket size mismatch");

#pragma pack(push, 1)
struct RoverFeedbackPacket {
  uint32_t ack_packet_id;  // 4 Bytes: Echo back received frame ID
  int8_t   s3_rx_rssi;     // 1 Byte : Signal strength measured by Rover S3 (dBm)
  int16_t  motor_left_pwm; // 2 Bytes: Current Left motor PWM (-255 to +255)
  int16_t  motor_right_pwm;// 2 Bytes: Current Right motor PWM (-255 to +255)
  uint8_t  current_amps_x10;// 1 Byte: Total motor current in 0.1A
  uint8_t  flags;          // 1 Byte : Failsafe & link status flags
  uint8_t  reserved;       // 1 Byte : Alignment
};
#pragma pack(pop)

// -----------------------------------------------------------------------------
// CyberPacket: 16-Byte Packed Binary Teleoperation Packet (v3.0)
// -----------------------------------------------------------------------------
#define CYBER_FRAME_HEADER_1  0x5A
#define CYBER_FRAME_HEADER_2  0xA5
#define CYBER_FRAME_TRAILER   0xED
#define CYBER_PACKET_SIZE     16
#define CYBER_DEVICE_ID       0xC5

#define CYBER_BTN_ARM_BIT     0
#define CYBER_BTN_MUTE_BIT    1
#define CYBER_BTN_PL_BIT      2
#define CYBER_BTN_PR_BIT      3
#define CYBER_BTN_CKL_BIT     4
#define CYBER_BTN_CKR_BIT     5
#define CYBER_PAGE_MASK       0xC0
#define CYBER_PAGE_SHIFT      6

#define CYBER_PROFILE_MASK    0x0F
#define CYBER_RATE_MASK       0xF0
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
  uint16_t frame_id;     // [13-14] High-Resolution Monotonic Frame ID
  uint8_t  crc8;         // [15]    CRC-8-ATM (Polynomial 0x07) over bytes [0..14]
};
#pragma pack(pop)
static_assert(sizeof(CyberPacket) == 16, "Error: CyberPacket size mismatch");

inline bool getCyberArm(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_ARM_BIT)) != 0; }
inline bool getCyberMute(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_MUTE_BIT)) != 0; }
inline bool getCyberBtnPL(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PL_BIT)) != 0; }
inline bool getCyberBtnPR(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PR_BIT)) != 0; }
inline bool getCyberBtnCKL(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKL_BIT)) != 0; }
inline bool getCyberBtnCKR(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKR_BIT)) != 0; }
inline uint8_t getCyberPage(const CyberPacket& p) { return (p.buttons >> CYBER_PAGE_SHIFT) & 0x03; }
inline uint8_t getCyberProfile(const CyberPacket& p) { return (p.mode_flags & CYBER_PROFILE_MASK); }
inline uint8_t getCyberRateHz(const CyberPacket& p) { return ((p.mode_flags >> CYBER_RATE_SHIFT) & 0x0F) * 10; }

inline bool getCyberArm(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_ARM_BIT)) != 0; }
inline bool getCyberMute(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_MUTE_BIT)) != 0; }
inline bool getCyberBtnPL(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PL_BIT)) != 0; }
inline bool getCyberBtnPR(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PR_BIT)) != 0; }
inline bool getCyberBtnCKL(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKL_BIT)) != 0; }
inline bool getCyberBtnCKR(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKR_BIT)) != 0; }
inline uint8_t getCyberPage(const volatile CyberPacket& p) { return (p.buttons >> CYBER_PAGE_SHIFT) & 0x03; }
inline uint8_t getCyberProfile(const volatile CyberPacket& p) { return (p.mode_flags & CYBER_PROFILE_MASK); }
inline uint8_t getCyberRateHz(const volatile CyberPacket& p) { return ((p.mode_flags >> CYBER_RATE_SHIFT) & 0x0F) * 10; }

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

inline uint8_t computeCyberCrc8(const CyberPacket& p) {
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(&p);
  uint8_t crc = 0x00;
  for (size_t i = 0; i < 15; i++) {
    crc = crc8_atm_table[crc ^ raw[i]];
  }
  return crc;
}

inline uint8_t computeCyberCrc8(const volatile CyberPacket& p) {
  const volatile uint8_t* raw = reinterpret_cast<const volatile uint8_t*>(&p);
  uint8_t crc = 0x00;
  for (size_t i = 0; i < 15; i++) {
    crc = crc8_atm_table[crc ^ raw[i]];
  }
  return crc;
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
  if (computeCyberCrc8(p) != p.crc8) return false;
  return true;
}

// -----------------------------------------------------------------------------
// Remote Telemetry Packet from Remote Controller (200 Bytes)
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct RemoteTelemetryPacket {
  uint32_t packet_id;      // 4 Bytes: Monotonic frame counter
  uint8_t  cmd_id;         // 1 Byte : Numeric Command ID (0 to 8)
  uint8_t  speed;          // 1 Byte : Integer PWM (0 to 255)
  uint8_t  gear;           // 1 Byte : Integer Gear (0 to 9)
  uint8_t  turbo;          // 1 Byte : Integer Flag (0 = Normal, 1 = Turbo 100%)
  char     telemetry[192]; // Null-terminated ASCII telemetry string from STM32 Remote
};
#pragma pack(pop)
static_assert(sizeof(RemoteTelemetryPacket) == 200, "Error: RemoteTelemetryPacket size mismatch");

volatile CyberPacket           rxCyberPacket;
volatile GamerControlPacket    rxGamerPacket;
volatile RemoteTelemetryPacket rxRemotePacket;
volatile bool newCyberPacket  = false;
volatile bool newGamerPacket  = false;
volatile bool newRemotePacket = false;
volatile uint32_t lastPacketTimestamp = 0;
volatile uint32_t totalPacketsReceived = 0;
volatile uint32_t lastPacketArrival = 0;
volatile uint32_t currentRxIntervalMs = 0;
int lastRssi = 0;

volatile uint8_t lastSenderMac[6] = {0};
volatile bool    hasSenderMac = false;

inline void setSenderMac(const uint8_t* src) {
  if (!src) return;
  for (int i = 0; i < 6; i++) {
    lastSenderMac[i] = src[i];
  }
  hasSenderMac = true;
}

int16_t lastKnownLeftPwm  = 0;
int16_t lastKnownRightPwm = 0;

uint32_t lastPrintTime = 0;
uint32_t lastRateCalcTime = 0;
uint32_t packetsInLastSec = 0;
uint32_t currentRateHz = 0;

uint8_t  lastDispatchedCmdId = CMD_STOP;
uint8_t  lastDispatchedSpeed = 0;
uint8_t  lastPrintedCmd   = 255;
uint8_t  lastPrintedSpeed = 255;
uint8_t  lastPrintedGear  = 255;
uint8_t  lastPrintedTurbo = 255;
bool failsafeTripped = false;

inline uint8_t sanitizeCommandId(uint8_t raw) {
  if (raw <= 8) return raw;
  switch (raw) {
    case 'F': case 'f': case 'w': case 'W': return CMD_FORWARD;
    case 'B': case 'b': case 's': case 'S': return CMD_BACKWARD;
    case 'A': case 'a':                     return CMD_PIVOT_LEFT;
    case 'D': case 'd':                     return CMD_PIVOT_RIGHT;
    case 'L': case 'l': case 'Q': case 'q': return CMD_LEFT_FRONT;
    case 'R': case 'r': case 'E': case 'e': return CMD_RIGHT_FRONT;
    case 'G': case 'g': case 'Z': case 'z': return CMD_LEFT_BACK;
    case 'I': case 'i': case 'C': case 'c': return CMD_RIGHT_BACK;
    default:                                return CMD_STOP;
  }
}

uint32_t lastForwardTime = 0;

bool ensurePeerRegistered(const uint8_t *mac) {
  if (!mac) return false;
  if (esp_now_is_peer_exist(mac)) return true;

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.ifidx   = WIFI_IF_STA;
  peerInfo.encrypt = false;
  return (esp_now_add_peer(&peerInfo) == ESP_OK);
}

void forwardToStm32(uint8_t cmd_id, uint8_t spd) {
  uint8_t clean_id = (cmd_id <= 8) ? cmd_id : 0;
  char char_cmd = CMD_CHARS[clean_id];
  uint8_t csum = (uint8_t)(char_cmd + spd) & 0xFF;

  // Frame: [0xAA] [CMD_CHAR] [SPEED] [CHECKSUM] [0x55]
  Serial1.write(0xAA);
  Serial1.write((uint8_t)char_cmd);
  Serial1.write(spd);
  Serial1.write(csum);
  Serial1.write(0x55);
  lastForwardTime = millis();
}

void forwardTurnRatioToStm32(uint8_t ratio) {
  uint8_t clean_ratio = (ratio <= 75) ? ratio : 75;
  char char_cmd = 'T';
  uint8_t csum = (uint8_t)(char_cmd + clean_ratio) & 0xFF;

  // Frame: [0xAA] ['T'] [RATIO] [CHECKSUM] [0x55]
  Serial1.write(0xAA);
  Serial1.write((uint8_t)char_cmd);
  Serial1.write(clean_ratio);
  Serial1.write(csum);
  Serial1.write(0x55);
}

// -----------------------------------------------------------------------------
// ESP-NOW Receive Callback (Runs in Wi-Fi Context)
// -----------------------------------------------------------------------------
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (info && info->rx_ctrl) {
    lastRssi = info->rx_ctrl->rssi;
  }
  const uint8_t *srcMac = info ? info->src_addr : nullptr;
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  lastRssi = 0;
  const uint8_t *srcMac = mac;
#endif

  if (len == sizeof(CyberPacket)) {
    CyberPacket temp;
    memcpy(&temp, data, sizeof(CyberPacket));
    if (isCyberPacketValid(temp)) {
      uint32_t arrNow = millis();
      if (lastPacketArrival > 0) {
        currentRxIntervalMs = arrNow - lastPacketArrival;
      }
      lastPacketArrival = arrNow;

      memcpy((void*)&rxCyberPacket, &temp, sizeof(CyberPacket));
      lastPacketTimestamp = arrNow;
      totalPacketsReceived++;
      packetsInLastSec++;
      newCyberPacket = true;

      // Cache genuine sender MAC for asynchronous loop() reply
      setSenderMac(srcMac);
    }
  } else if (len == sizeof(RemoteTelemetryPacket)) {
    memcpy((void*)&rxRemotePacket, data, sizeof(RemoteTelemetryPacket));
    lastPacketTimestamp = millis();
    totalPacketsReceived++;
    packetsInLastSec++;
    newRemotePacket = true;

    // Cache genuine sender MAC for asynchronous loop() reply
    setSenderMac(srcMac);
  } else if (len == sizeof(GamerControlPacket)) {
    memcpy((void*)&rxGamerPacket, data, sizeof(GamerControlPacket));
    lastPacketTimestamp = millis();
    totalPacketsReceived++;
    packetsInLastSec++;
    newGamerPacket = true;

    // Cache genuine sender MAC for asynchronous loop() reply
    setSenderMac(srcMac);
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
  delay(400);

  // 1. HARDWARE WI-FI CHANNEL LOCK & SCAN/HOPPING ELIMINATION
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  esp_wifi_set_ps(WIFI_PS_NONE); // CRITICAL: Disable 802.11 Modem Sleep so RF receiver is ALWAYS ON!
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  uint8_t efuseMac[6];
  esp_read_mac(efuseMac, ESP_MAC_WIFI_STA);
  char macBuffer[18];
  snprintf(macBuffer, sizeof(macBuffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);

  uint8_t verifiedChan = 0;
  wifi_second_chan_t secChan;
  esp_wifi_get_channel(&verifiedChan, &secChan);

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — HIGH-EFFICIENCY ESP-NOW TELEOP RECEIVER (ESP32-S3) v3.5       "));
  Serial.print(F(">>> Station MAC Address : ")); Serial.println(macBuffer);
  Serial.print(F(">>> Wi-Fi Channel       : Channel ")); Serial.print(ESPNOW_WIFI_CHANNEL);
  Serial.print(F(" (Verified Active: ")); Serial.print(verifiedChan); Serial.println(F(")"));
  Serial.print(F(">>> Hardware UART1 Link : GPIO 17 (TX1) -> PA10 | GPIO 18 (RX1) <- PA9\n"));
  Serial.println(F("================================================================================\n"));

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("[ERROR] ESP-NOW Init Failed!"));
    while (true) { delay(1000); }
  }

  esp_now_register_recv_cb(onDataRecv);

  // Register Universal Broadcast Peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Ensure motors start in absolute neutral
  forwardToStm32(CMD_STOP, 0);
}

void loop() {
  uint32_t now = millis();

  // 1. Calculate live throughput Hz
  if (now - lastRateCalcTime >= 1000) {
    currentRateHz = packetsInLastSec;
    packetsInLastSec = 0;
    lastRateCalcTime = now;
  }

  // 2. Process newly arrived 12-byte binary CyberPacket (Real-Time 40/50 Hz Teleop)
  if (newCyberPacket) {
    newCyberPacket = false;
    if (failsafeTripped) {
      Serial.println(F(">>> [FAILSAFE CLEARED] Telemetry link restored! Normal driving active."));
    }
    failsafeTripped = false;

    // Local stack snapshot guarantees atomic consistency and eliminates torn reads
    CyberPacket pkt;
    memcpy(&pkt, (const void*)&rxCyberPacket, sizeof(CyberPacket));

    uint8_t prof = getCyberProfile(pkt);
    bool isArmed = getCyberArm(pkt);
    bool turbo   = getCyberBtnCKL(pkt);
    uint8_t gov  = pkt.speed_gov;
    if (gov < 15)  gov = 15;
    if (gov > 100) gov = 100;

    uint8_t cmd_id = CMD_STOP;
    uint8_t speed  = 0;

    if (isArmed) {
      float gov_max_pwm = 70.0f + ((float)gov / 100.0f) * 185.0f; // 70 to 255 PWM
      if (turbo) gov_max_pwm = 255.0f;

      if (prof == 1) { // TANK MODE
        int ly = pkt.axis_ly;
        int ry = pkt.axis_ry;

        if (ly > 12 && ry > 12) {
          cmd_id = CMD_FORWARD;
          speed  = (uint8_t)map(max(ly, ry), 12, 100, 70, (int)gov_max_pwm);
        } else if (ly < -12 && ry < -12) {
          cmd_id = CMD_BACKWARD;
          speed  = (uint8_t)map(max(abs(ly), abs(ry)), 12, 100, 70, (int)gov_max_pwm);
        } else if (ly > 12 && ry < -12) {
          cmd_id = CMD_PIVOT_RIGHT; // Spin CW
          speed  = (uint8_t)map(max(ly, abs(ry)), 12, 100, 70, (int)gov_max_pwm);
        } else if (ly < -12 && ry > 12) {
          cmd_id = CMD_PIVOT_LEFT;  // Spin CCW
          speed  = (uint8_t)map(max(abs(ly), ry), 12, 100, 70, (int)gov_max_pwm);
        } else if (ly > 12) {
          cmd_id = CMD_RIGHT_FRONT;
          speed  = (uint8_t)map(ly, 12, 100, 70, (int)gov_max_pwm);
        } else if (ry > 12) {
          cmd_id = CMD_LEFT_FRONT;
          speed  = (uint8_t)map(ry, 12, 100, 70, (int)gov_max_pwm);
        } else if (ly < -12) {
          cmd_id = CMD_RIGHT_BACK;
          speed  = (uint8_t)map(abs(ly), 12, 100, 70, (int)gov_max_pwm);
        } else if (ry < -12) {
          cmd_id = CMD_LEFT_BACK;
          speed  = (uint8_t)map(abs(ry), 12, 100, 70, (int)gov_max_pwm);
        } else {
          cmd_id = CMD_STOP;
          speed  = 0;
        }
      } else { // Profile 0: ARCADE | Profile 2: 1-STICK
        int ly = pkt.axis_ly;
        int rx = (prof == 2) ? pkt.axis_lx : pkt.axis_rx;

        int abs_thr = abs(ly);
        int abs_str = abs(rx);

        if (ly > 12) {
          if (rx < -20)      cmd_id = CMD_LEFT_FRONT;
          else if (rx > 20)  cmd_id = CMD_RIGHT_FRONT;
          else               cmd_id = CMD_FORWARD;
          speed = turbo ? 255 : (uint8_t)map(abs_thr, 12, 100, 70, (int)gov_max_pwm);
        } else if (ly < -12) {
          if (rx < -20)      cmd_id = CMD_LEFT_BACK;
          else if (rx > 20)  cmd_id = CMD_RIGHT_BACK;
          else               cmd_id = CMD_BACKWARD;
          speed = turbo ? 255 : (uint8_t)map(abs_thr, 12, 100, 70, (int)gov_max_pwm);
        } else {
          // Neutral throttle: in-place pivot / tank spin
          if (rx < -20) {
            cmd_id = CMD_PIVOT_LEFT;
            speed  = turbo ? 255 : (uint8_t)map(abs_str, 20, 100, 70, (int)gov_max_pwm);
          } else if (rx > 20) {
            cmd_id = CMD_PIVOT_RIGHT;
            speed  = turbo ? 255 : (uint8_t)map(abs_str, 20, 100, 70, (int)gov_max_pwm);
          } else {
            cmd_id = CMD_STOP;
            speed  = 0;
          }
        }
      }
    }

    // Forward dynamic turn ratio to STM32 when it changes
    static uint8_t lastForwardedTurnRatio = 255;
    if (pkt.turn_ratio != lastForwardedTurnRatio) {
      forwardTurnRatioToStm32(pkt.turn_ratio);
      lastForwardedTurnRatio = pkt.turn_ratio;
    }

    // FORWARD IMMEDIATELY TO STM32 MOTOR SUPERVISOR (< 10us) AT FULL 40/50 Hz
    lastDispatchedCmdId = cmd_id;
    lastDispatchedSpeed = speed;
    forwardToStm32(cmd_id, speed);
  }

  // 2.1. Process newly arrived pure remote telemetry packet (Fallback)
  if (newRemotePacket) {
    newRemotePacket = false;
    if (failsafeTripped) {
      Serial.println(F(">>> [FAILSAFE CLEARED] Telemetry link restored! Normal driving active."));
    }
    failsafeTripped = false;
    lastDispatchedCmdId = rxRemotePacket.cmd_id;
    lastDispatchedSpeed = rxRemotePacket.speed;
    forwardToStm32(rxRemotePacket.cmd_id, rxRemotePacket.speed);
  }

  // 2.2. Process newly arrived legacy teleop packet (Fallback)
  if (newGamerPacket) {
    newGamerPacket = false;
    if (failsafeTripped) {
      Serial.println(F(">>> [FAILSAFE CLEARED] Telemetry link restored! Normal driving active."));
    }
    failsafeTripped = false;
    uint8_t cmd = sanitizeCommandId(rxGamerPacket.cmd_id);
    lastDispatchedCmdId = cmd;
    lastDispatchedSpeed = rxGamerPacket.speed;
    forwardToStm32(cmd, rxGamerPacket.speed);
  }

  // ====================================================================
  // ====================================================================
  //  2.3. RECONSTRUCT & LOG TELEMETRY STRING TO SERIAL (4 Hz Single-Buffer)
  //  Single atomic snprintf + Serial.println eliminates USB CDC FIFO blocking!
  // ====================================================================
  static uint32_t lastSerialLogTime = 0;
  static uint32_t lastParkNoticeTime = 0;
  if (totalPacketsReceived > 0 && (now - lastSerialLogTime >= 250)) {
    lastSerialLogTime = now;

    if (failsafeTripped) {
      Serial.println(F("[FAILSAFE ACTIVE - SIGNAL LOST - MOTORS STOPPED]"));
    } else {
      // Snapshot copy to local stack struct
      CyberPacket logPkt;
      memcpy(&logPkt, (const void*)&rxCyberPacket, sizeof(CyberPacket));

      uint8_t prof = getCyberProfile(logPkt);
      const char* pName = (prof == 1) ? "TANK" : ((prof == 2) ? "1-STICK" : "ARCADE");

      bool isArmed = getCyberArm(logPkt);
      bool isMuted = getCyberMute(logPkt);
      uint8_t pl   = getCyberBtnPL(logPkt) ? 1 : 0;
      uint8_t pr   = getCyberBtnPR(logPkt) ? 1 : 0;
      uint8_t ckl  = getCyberBtnCKL(logPkt) ? 1 : 0;
      uint8_t ckr  = getCyberBtnCKR(logPkt) ? 1 : 0;
      uint8_t page = getCyberPage(logPkt);

      char axisBuf[64];
      if (prof == 1) { // Tank
        snprintf(axisBuf, sizeof(axisBuf), "LX:%d%%(ACT) LY:%d%%(ACT) RX:0%%(OFF) RY:%d%%(ACT)",
                 (int)logPkt.axis_lx, (int)logPkt.axis_ly, (int)logPkt.axis_ry);
      } else if (prof == 2) { // 1-Stick
        snprintf(axisBuf, sizeof(axisBuf), "LX:%d%%(ACT) LY:%d%%(ACT) RX:0%%(OFF) RY:0%%(OFF)",
                 (int)logPkt.axis_lx, (int)logPkt.axis_ly);
      } else { // Arcade
        snprintf(axisBuf, sizeof(axisBuf), "LX:%d%%(ACT) LY:%d%%(ACT) RX:%d%%(ACT) RY:0%%(OFF)",
                 (int)logPkt.axis_lx, (int)logPkt.axis_ly, (int)logPkt.axis_rx);
      }

      char stmBuf[32];
      if (isArmed) {
        uint8_t cid = (lastDispatchedCmdId <= 8) ? lastDispatchedCmdId : 0;
        snprintf(stmBuf, sizeof(stmBuf), "CMD:%c SPD:%d", CMD_CHARS[cid], lastDispatchedSpeed);
      } else {
        snprintf(stmBuf, sizeof(stmBuf), "PARK(0.00V)");
      }

      char logLine[350];
      snprintf(logLine, sizeof(logLine),
               "MODE:%s | RATE:%uHz | DEV:0x%02X | SEQ:%u | PERIOD:%ums | CRC:OK | AXIS:[%s] | TOGGLES:[ARM:%s MUTE:%s] | BTNS:[PL:%u PR:%u CKL:%u CKR:%u] | POTS:[GOV:%d%% TURN:%d%% POT3:%d%%(PG%u)] | STM32:[%s]",
               pName, currentRateHz, logPkt.device_id, logPkt.seq_id, currentRxIntervalMs,
               axisBuf,
               isArmed ? "ARMED(D)" : "PARK(P)",
               isMuted ? "ON" : "OFF",
               pl, pr, ckl, ckr,
               (int)logPkt.speed_gov, (int)logPkt.turn_ratio, (int)logPkt.pot_aux, page,
               stmBuf);

      Serial.println(logLine);

      if (!isArmed && (now - lastParkNoticeTime >= 2000)) {
        lastParkNoticeTime = now;
        Serial.println(F(">>> [SAFETY INTERLOCK: PARK ACTIVE] Flip Toggle Left (PB3) to ARM to enable motor drive!"));
      }
    }
  }

  // Periodic Command Heartbeat to STM32 (Every 80ms)
  // Keeps STM32 watchdog active even during wireless jitter
  if (totalPacketsReceived > 0 && !failsafeTripped && (now - lastForwardTime >= 80)) {
    forwardToStm32(lastDispatchedCmdId, lastDispatchedSpeed);
  }

  // 3. Autonomous Dead-Man Safety Watchdog (800ms timeout)
  // Fresh timestamp sampling + signed delta calculation completely eliminates
  // unsigned 32-bit underflow race conditions when packets arrive during loop().
  uint32_t watchdog_now = millis();
  int32_t elapsed_since_packet = (int32_t)(watchdog_now - lastPacketTimestamp);

  if (!failsafeTripped && (totalPacketsReceived > 0) && (elapsed_since_packet > (int32_t)FAILSAFE_TIMEOUT_MS)) {
    failsafeTripped = true;
    forwardToStm32(CMD_STOP, 0);

    Serial.println();
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
    Serial.println(F("⚠️ [FAILSAFE ENGAGED] Signal lost for >800ms! Auto-STOP dispatched."));
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));

    lastPrintedCmd = CMD_STOP;
    lastPrintedSpeed = 0;
  }

  // 4. Bi-Directional Telemetry Feedback Stream to Transmitter (10 Hz Cadence, Every 100ms)
  // Direct unicast back to the transmitter's MAC address once learned.
  // Does NOT spam broadcast frames into the air when transmitter is not connected!
  static uint32_t lastFeedbackSendTime = 0;
  if (now - lastFeedbackSendTime >= 100) {
    lastFeedbackSendTime = now;

    if (hasSenderMac) {
      uint8_t targetMac[6];
      for (int i = 0; i < 6; i++) {
        targetMac[i] = lastSenderMac[i];
      }
      if (ensurePeerRegistered(targetMac)) {
        RoverFeedbackPacket reply;
        reply.ack_packet_id    = (totalPacketsReceived > 0) ? (uint32_t)rxCyberPacket.frame_id : 0;
        reply.s3_rx_rssi       = (int8_t)lastRssi;
        reply.motor_left_pwm   = lastKnownLeftPwm;
        reply.motor_right_pwm  = lastKnownRightPwm;
        reply.current_amps_x10 = (uint8_t)(((abs(lastKnownLeftPwm) + abs(lastKnownRightPwm)) * 30) / 510);
        reply.flags            = failsafeTripped ? 1 : 0;
        reply.reserved         = 0;

        esp_now_send(targetMac, (uint8_t*)&reply, sizeof(reply));
      }
    }
  }

  // 5. Non-blocking parsing of live motor telemetry from STM32 PA9
  static char stm32RxBuf[64];
  static uint8_t stm32RxIdx = 0;

  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
      if (stm32RxIdx > 0) {
        stm32RxBuf[stm32RxIdx] = '\0';

        // Parse fast compact telemetry: "$M,150,150"
        if (strncmp(stm32RxBuf, "$M,", 3) == 0) {
          int l = 0, r = 0;
          if (sscanf(stm32RxBuf + 3, "%d,%d", &l, &r) == 2) {
            lastKnownLeftPwm  = (int16_t)l;
            lastKnownRightPwm = (int16_t)r;
          }
        } else {
          char *lPtr = strstr(stm32RxBuf, "Left=");
          char *rPtr = strstr(stm32RxBuf, "Right=");
          if (lPtr && rPtr) {
            lastKnownLeftPwm  = (int16_t)atoi(lPtr + 5);
            lastKnownRightPwm = (int16_t)atoi(rPtr + 6);
          }
        }
        stm32RxIdx = 0;
      }
    } else if (stm32RxIdx < sizeof(stm32RxBuf) - 1) {
      stm32RxBuf[stm32RxIdx++] = c;
    }
  }
}
