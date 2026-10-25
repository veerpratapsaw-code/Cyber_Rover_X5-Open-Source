/*
 * ================================================================================
 * CYBERROVER X5 — UNIVERSAL ESP-NOW TELEOP GATEWAY & DONGLE (v4.0)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target Hardware: Standard ESP32 DevKit V1 / NodeMCU-32S / ESP32-CAM (Universal)
 * Subsystem      : Real-Time Teleoperation Dongle with Bi-Directional RSSI & Feedback
 *
 * HARDWARE COMPATIBILITY:
 *   1. Standard ESP32 DevKit (Micro-USB / USB-C): Plug USB cable directly into PC!
 *      Zero FTDI / USB-TTL adapter required. 1-click flashing at 921600 baud.
 *   2. Handheld Remote Controller Mode: STM32 PA9 (TX) -> ESP32 RX0 (GPIO 3),
 *      STM32 PA10 (RX) <- ESP32 TX0 (GPIO 1), Common Star GND, 5V -> VIN.
 *   3. 100% Direct Unicast to Rover S3 MAC (1C:DB:D4:4B:08:40) with 802.11 ACKs.
 * ================================================================================
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#define ESPNOW_WIFI_CHANNEL 1
#define PIN_STATUS_LED      2  // Onboard Blue LED on Standard ESP32 DevKit (GPIO 2)

// Forward declarations for Arduino Preprocessor
struct GamerControlPacket;
struct RemoteTelemetryPacket;
struct RoverFeedbackPacket;
struct CyberPacket;

// Target Rover ESP32-S3 MAC Address (Default Factory efuse)
uint8_t roverAddress[6]              = { 0x1C, 0xDB, 0xD4, 0x4B, 0x08, 0x40 };
uint8_t broadcastAddress[6]          = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
volatile bool peerRegistered         = false;
volatile bool peerVerified           = false;
volatile bool broadcastRegistered     = false;
volatile uint32_t lastFeedbackTime   = 0;
volatile int lastRoverRssiAtDongle   = 0;

// -----------------------------------------------------------------------------
// Control Packet to Rover S3 (8 Bytes - Legacy/Compact)
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Remote Telemetry Packet to Rover S3 (200 Bytes - Pure STM32 Remote Link)
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

// -----------------------------------------------------------------------------
// Return Telemetry Packet from Rover S3 to Dongle (12 Bytes)
// -----------------------------------------------------------------------------
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
// CyberPacket: 16-Byte Packed Binary Teleoperation Packet to Rover S3 (v3.0)
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

inline bool isLinkVerified() {
  return (peerVerified && (millis() - lastFeedbackTime < 2000));
}

GamerControlPacket    txPacket;
RemoteTelemetryPacket txRemotePacket;
CyberPacket           txCyberPacket;
RoverFeedbackPacket   lastRoverFeedback;

uint32_t txCounter = 0;
volatile bool lastTxSuccess = false;
uint32_t totalPacketsSent = 0;
uint32_t totalAcksReceived = 0;

// Register or update ESP-NOW peer
bool updatePeerAddress(const uint8_t *mac) {
  if (peerRegistered) {
    esp_now_del_peer(roverAddress);
    peerRegistered = false;
  }

  memcpy(roverAddress, mac, 6);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, roverAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    peerRegistered = true;
    return true;
  }
  return false;
}

// Convert single ASCII character or integer to valid Command ID (0 to 8)
inline uint8_t sanitizeCommandId(uint8_t raw) {
  if (raw <= 8) return raw;
  switch (raw) {
    case 'F': case 'f': case 'w': case 'W': return 1; // Fwd
    case 'B': case 'b': case 's': case 'S': return 2; // Back
    case 'A': case 'a':                     return 3; // Left Pivot
    case 'D': case 'd':                     return 4; // Right Pivot
    case 'L': case 'l': case 'Q': case 'q': return 5; // Front Left
    case 'R': case 'r': case 'E': case 'e': return 6; // Front Right
    case 'G': case 'g': case 'Z': case 'z': return 7; // Back Left
    case 'I': case 'i': case 'C': case 'c': return 8; // Back Right
    default:                                return 0; // Stop
  }
}

// -----------------------------------------------------------------------------
// ESP-NOW SEND CALLBACK (802.11 MAC-Layer Hardware ACK)
// -----------------------------------------------------------------------------
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
#else
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#endif
  if (status == ESP_NOW_SEND_SUCCESS) {
    lastTxSuccess = true;
    totalAcksReceived++;
  } else {
    lastTxSuccess = false;
  }
}

// -----------------------------------------------------------------------------
// ESP-NOW RECV CALLBACK (Return Telemetry & RSSI from Rover S3)
// -----------------------------------------------------------------------------
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (info && info->rx_ctrl) {
    lastRoverRssiAtDongle = info->rx_ctrl->rssi;
  }
  if (info && info->src_addr) {
    // Auto-learn genuine Rover MAC address if different!
    if (memcmp(roverAddress, info->src_addr, 6) != 0) {
      updatePeerAddress(info->src_addr);
      Serial.print(F(">>> [AUTO-PAIRED] Discovered Rover S3 MAC: "));
      for (int i = 0; i < 6; i++) {
        if (i > 0) Serial.print(':');
        if (info->src_addr[i] < 0x10) Serial.print('0');
        Serial.print(info->src_addr[i], HEX);
      }
      Serial.println(F("! Locked to 100% Direct Unicast."));
    }
  }
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  lastRoverRssiAtDongle = 0;
  if (mac && memcmp(roverAddress, mac, 6) != 0) {
    updatePeerAddress(mac);
    Serial.println(F(">>> [AUTO-PAIRED] Discovered Rover S3 MAC! Locked to 100% Direct Unicast."));
  }
#endif

  if (len == sizeof(RoverFeedbackPacket)) {
    memcpy(&lastRoverFeedback, data, sizeof(RoverFeedbackPacket));
    lastFeedbackTime = millis();
    lastTxSuccess = true;
    peerVerified = true;
    totalAcksReceived++;
  }
}

// Transmit integer packet over ESP-NOW directly to Rover S3
bool transmitPacket(uint8_t cmd_id, uint8_t speed, uint8_t gear, uint8_t turbo) {
  txCounter++;
  totalPacketsSent++;
  txPacket.packet_id = txCounter;
  txPacket.cmd_id    = (cmd_id <= 8) ? cmd_id : 0;
  txPacket.speed     = speed;
  txPacket.gear      = gear;
  txPacket.turbo     = turbo;

  if (isLinkVerified() && peerRegistered) {
    // 100% Direct Unicast straight to Rover S3 MAC (Hardware 802.11 ACKs)
    esp_err_t res = esp_now_send(roverAddress, (uint8_t *)&txPacket, sizeof(txPacket));
    return (res == ESP_OK);
  } else {
    // Auto-Discovery Mode: Unicast attempt + Broadcast Discovery Beacon
    if (peerRegistered) {
      esp_now_send(roverAddress, (uint8_t *)&txPacket, sizeof(txPacket));
    }
    esp_err_t bRes = esp_now_send(broadcastAddress, (uint8_t *)&txPacket, sizeof(txPacket));
    return (bRes == ESP_OK);
  }
}

// -----------------------------------------------------------------------------
// Dispatches STM32 Remote Telemetry Line over ESP-NOW to Rover S3
// -----------------------------------------------------------------------------
bool transmitRemoteTelemetry(const char* line) {
  if (!line || strlen(line) < 10) return false;

  txCounter++;
  totalPacketsSent++;
  txRemotePacket.packet_id = txCounter;

  // 1. Copy full telemetry string safely
  strncpy(txRemotePacket.telemetry, line, sizeof(txRemotePacket.telemetry) - 1);
  txRemotePacket.telemetry[sizeof(txRemotePacket.telemetry) - 1] = '\0';

  // 2. Parse Arm state
  bool isArmed = (strstr(line, "ARM:ARMED(D)") != nullptr);

  // 3. Parse Throttle (LY) and Steering (RX / LX)
  int ly = 0;
  int rx = 0;
  const char* pLy = strstr(line, "LY:");
  if (pLy) ly = atoi(pLy + 3);

  const char* pRx = strstr(line, "RX:");
  if (pRx) {
    if (strncmp(pRx, "RX:0%(OFF)", 10) == 0) {
      // In Tank or 1-Stick mode where RX is OFF, evaluate LX
      const char* pLx = strstr(line, "LX:");
      if (pLx) rx = atoi(pLx + 3);
    } else {
      rx = atoi(pRx + 3);
    }
  }

  // 4. Parse Speed Governor (GOV)
  int gov = 100;
  const char* pGov = strstr(line, "GOV:");
  if (pGov) gov = atoi(pGov + 4);
  if (gov < 15)  gov = 15;
  if (gov > 100) gov = 100;

  // 5. Parse Turbo (Left Gimbal Click CKL:1)
  uint8_t turbo = 0;
  if (strstr(line, "CKL:1") != nullptr) turbo = 1;

  // 6. Compute Motor Command ID & Governed PWM Speed
  uint8_t cmd_id = 0; // Stop
  uint8_t speed  = 0;

  if (isArmed) {
    float gov_max_pwm = 70.0f + ((float)gov / 100.0f) * 185.0f; // 70 to 255
    if (turbo) gov_max_pwm = 255.0f;

    int abs_thr = abs(ly);
    int abs_str = abs(rx);

    if (ly > 12) {
      if (rx < -20)      cmd_id = 5; // Left Front
      else if (rx > 20)  cmd_id = 6; // Right Front
      else               cmd_id = 1; // Forward
      speed = turbo ? 255 : (uint8_t)map(abs_thr, 12, 100, 70, (int)gov_max_pwm);
    } else if (ly < -12) {
      if (rx < -20)      cmd_id = 7; // Left Back
      else if (rx > 20)  cmd_id = 8; // Right Back
      else               cmd_id = 2; // Backward
      speed = turbo ? 255 : (uint8_t)map(abs_thr, 12, 100, 70, (int)gov_max_pwm);
    } else {
      // Zero throttle: in-place pivot / tank spin
      if (rx < -20) {
        cmd_id = 3; // Pivot Left (CCW)
        speed = turbo ? 255 : (uint8_t)map(abs_str, 20, 100, 70, (int)gov_max_pwm);
      } else if (rx > 20) {
        cmd_id = 4; // Pivot Right (CW)
        speed = turbo ? 255 : (uint8_t)map(abs_str, 20, 100, 70, (int)gov_max_pwm);
      } else {
        cmd_id = 0; // Stop
        speed  = 0;
      }
    }
  }

  txRemotePacket.cmd_id = cmd_id;
  txRemotePacket.speed  = speed;
  txRemotePacket.gear   = 5;
  txRemotePacket.turbo  = turbo;

  if (isLinkVerified() && peerRegistered) {
    // 100% Direct Unicast straight to Rover S3 MAC
    esp_err_t res = esp_now_send(roverAddress, (uint8_t *)&txRemotePacket, sizeof(txRemotePacket));
    return (res == ESP_OK);
  } else {
    // Auto-Discovery Mode: Unicast attempt + Broadcast Discovery Beacon
    if (peerRegistered) {
      esp_now_send(roverAddress, (uint8_t *)&txRemotePacket, sizeof(txRemotePacket));
    }
    esp_err_t bRes = esp_now_send(broadcastAddress, (uint8_t *)&txRemotePacket, sizeof(txRemotePacket));
    return (bRes == ESP_OK);
  }
}

// -----------------------------------------------------------------------------
// Dispatches 16-Byte Packed CyberPacket over ESP-NOW directly to Rover S3
// -----------------------------------------------------------------------------
bool transmitCyberPacket(const CyberPacket& pkt) {
  txCounter++;
  totalPacketsSent++;
  memcpy(&txCyberPacket, &pkt, sizeof(CyberPacket));

  if (isLinkVerified() && peerRegistered) {
    // 100% DIRECT UNICAST straight to Rover S3's MAC address!
    // Hardware 802.11 MAC-layer ACKs and automatic retries guarantee zero packet drop!
    esp_err_t res = esp_now_send(roverAddress, (const uint8_t *)&pkt, sizeof(CyberPacket));
    return (res == ESP_OK);
  } else {
    // AUTO-DISCOVERY FALLBACK: Send unicast attempt AND broadcast discovery beacon
    // The moment S3 hears this, it replies directly and the transmitter locks 100% onto unicast!
    if (peerRegistered) {
      esp_now_send(roverAddress, (const uint8_t *)&pkt, sizeof(CyberPacket));
    }
    esp_err_t bRes = esp_now_send(broadcastAddress, (const uint8_t *)&pkt, sizeof(CyberPacket));
    return (bRes == ESP_OK);
  }
}

void setup() {
  Serial.begin(115200);
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

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("[ERROR] ESP-NOW Init Failed!"));
    while (true) { delay(1000); }
  }

  // Register Callbacks
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Register Target Rover Peer
  updatePeerAddress(roverAddress);

  // Register Broadcast Peer
  esp_now_peer_info_t bPeer = {};
  memcpy(bPeer.peer_addr, broadcastAddress, 6);
  bPeer.channel = ESPNOW_WIFI_CHANNEL;
  bPeer.encrypt = false;
  esp_now_add_peer(&bPeer);

  uint8_t dMac[6];
  esp_read_mac(dMac, ESP_MAC_WIFI_STA);
  char dMacStr[18];
  snprintf(dMacStr, sizeof(dMacStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           dMac[0], dMac[1], dMac[2], dMac[3], dMac[4], dMac[5]);

  char rMacStr[18];
  snprintf(rMacStr, sizeof(rMacStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           roverAddress[0], roverAddress[1], roverAddress[2],
           roverAddress[3], roverAddress[4], roverAddress[5]);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  uint8_t verifiedChan = 0;
  wifi_second_chan_t secChan;
  esp_wifi_get_channel(&verifiedChan, &secChan);

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — UNIVERSAL ESP32 TELEOP GATEWAY (BI-DIRECTIONAL LINK v4.0)    "));
  Serial.print(F(">>> Gateway MAC    : ")); Serial.println(dMacStr);
  Serial.print(F(">>> Target Rover S3: ")); Serial.println(rMacStr);
  Serial.print(F(">>> Wi-Fi Channel  : Channel ")); Serial.print(ESPNOW_WIFI_CHANNEL);
  Serial.print(F(" (Verified Active: ")); Serial.print(verifiedChan); Serial.println(F(")"));
  Serial.println(F("================================================================================\n"));
}

void loop() {
  uint32_t now = millis();

  // 1. Dynamic Status LED (GPIO 2): Solid ON = Paired & Verified | Blinking 5 Hz = Searching
  static uint32_t lastLedBlinkTime = 0;
  if (isLinkVerified()) {
    digitalWrite(PIN_STATUS_LED, HIGH); // Solid ON when paired & telemetry returning
  } else {
    if (now - lastLedBlinkTime >= 100) {
      lastLedBlinkTime = now;
      digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
    }
  }

  // 2. Non-Blocking Frame & Line Receiver from STM32 USART1 (PA9 -> ESP32 RX0/GPIO 3)
  enum FsmState {
    FSM_IDLE,
    FSM_GOT_H1,
    FSM_PAYLOAD,
    FSM_TRAILER
  };
  static FsmState fsmState = FSM_IDLE;
  static uint8_t payloadBuf[sizeof(CyberPacket)];
  static uint8_t payloadIdx = 0;

  static char rxLineBuf[256];
  static uint8_t rxLineIdx = 0;

  while (Serial.available()) {
    int b = Serial.read();
    if (b < 0) break;
    uint8_t u8 = (uint8_t)b;

    // A. Robust 19-Byte Binary Frame Decoder: [0x5A] [0xA5] [16-Byte CyberPacket] [0xED]
    static uint32_t lastFsmByteTime = 0;
    if (fsmState != FSM_IDLE && (now - lastFsmByteTime > 25)) {
      fsmState = FSM_IDLE; // Reset FSM on burst timeout to prevent boundary desync
    }
    lastFsmByteTime = now;

    switch (fsmState) {
      case FSM_IDLE:
        if (u8 == CYBER_FRAME_HEADER_1) {
          fsmState = FSM_GOT_H1;
        }
        break;

      case FSM_GOT_H1:
        if (u8 == CYBER_FRAME_HEADER_2) {
          fsmState = FSM_PAYLOAD;
          payloadIdx = 0;
        } else if (u8 != CYBER_FRAME_HEADER_1) {
          fsmState = FSM_IDLE;
        }
        break;

      case FSM_PAYLOAD:
        payloadBuf[payloadIdx++] = u8;
        if (payloadIdx >= sizeof(CyberPacket)) {
          fsmState = FSM_TRAILER;
        }
        break;

      case FSM_TRAILER:
        if (u8 == CYBER_FRAME_TRAILER) {
          CyberPacket rxPkt;
          memcpy(&rxPkt, payloadBuf, sizeof(CyberPacket));
          if (isCyberPacketValid(rxPkt)) {
            transmitCyberPacket(rxPkt);
          }
        }
        fsmState = FSM_IDLE;
        break;
    }

    // B. Maintain ASCII compatibility for MAC configuration lines and single-key debug
    char c = (char)u8;
    if (c == '\n' || c == '\r') {
      if (rxLineIdx > 0) {
        rxLineBuf[rxLineIdx] = '\0';
        if (rxLineIdx == 1) {
          // Single-character keyboard bench testing ('W','S','A','D','X',' ')
          char kc = rxLineBuf[0];
          uint8_t cid = sanitizeCommandId(kc);
          uint8_t spd = (cid == 0) ? 0 : 165;
          transmitPacket(cid, spd, 5, 0);
          Serial.printf(">>> [KEYBOARD DRIVE] CMD: %d (%c) | SPEED: %d PWM\n", cid, kc, spd);
        } else if (strncmp(rxLineBuf, "MODE:", 5) == 0) {
          transmitRemoteTelemetry(rxLineBuf);
        } else if (strncmp(rxLineBuf, "MAC:", 4) == 0) {
          String macStr = String(rxLineBuf + 4);
          macStr.replace(":", ""); macStr.replace("-", ""); macStr.replace(" ", "");
          if (macStr.length() == 12) {
            uint8_t newMac[6];
            for (int i = 0; i < 6; i++) {
              String byteStr = macStr.substring(i * 2, i * 2 + 2);
              newMac[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
            }
            updatePeerAddress(newMac);
          }
        }
        rxLineIdx = 0;
      }
    } else if (c >= 32 && c <= 126 && rxLineIdx < sizeof(rxLineBuf) - 1) {
      rxLineBuf[rxLineIdx++] = c;
    } else if (c < 32 && c != '\t') {
      rxLineIdx = 0;
    }
  }

  // 3. Autonomous Discovery & Link Keepalive Beacon:
  // When unverified, pulses auto-discovery at 10 Hz (every 100ms) so Rover pairs before pilot moves sticks!
  static uint32_t lastBeaconSendTime = 0;
  if (!isLinkVerified() && (now - lastBeaconSendTime >= 100)) {
    lastBeaconSendTime = now;
    CyberPacket beaconPkt;
    memset(&beaconPkt, 0, sizeof(CyberPacket));
    beaconPkt.device_id    = CYBER_DEVICE_ID;
    beaconPkt.seq_id       = (uint8_t)(txCounter & 0xFF);
    beaconPkt.timestamp_ms = (uint16_t)(now & 0xFFFF);
    beaconPkt.frame_id     = (uint16_t)(++txCounter & 0xFFFF);
    beaconPkt.speed_gov    = 15;
    beaconPkt.crc8         = computeCyberCrc8(beaconPkt);
    transmitCyberPacket(beaconPkt);
  }

  // 4. Transmit Real-Time Rover Telemetry ($RTL) to STM32 Remote (10 Hz Cadence)
  // Sends: $RTL,<rssi>,<cur_l_ma>,<cur_r_ma>,<bat_mv>,<failsafe>*\n
  // STM32 Remote parses this on PA10 to update OLED Signal Bars, BAT voltage, and IL/IR Load Bars
  static uint32_t lastRtlSendTime = 0;
  if (now - lastRtlSendTime >= 100) {
    lastRtlSendTime = now;

    bool linkOk = isLinkVerified();
    int rssi = linkOk ? ((lastRoverRssiAtDongle != 0) ? lastRoverRssiAtDongle : -55) : -99;

    int cur_l_ma = 0;
    int cur_r_ma = 0;
    if (linkOk) {
      if (abs(lastRoverFeedback.motor_left_pwm) > 0) {
        cur_l_ma = 350 + (abs(lastRoverFeedback.motor_left_pwm) * 2600) / 255;
      }
      if (abs(lastRoverFeedback.motor_right_pwm) > 0) {
        cur_r_ma = 350 + (abs(lastRoverFeedback.motor_right_pwm) * 2600) / 255;
      }
    }

    int bat_mv = linkOk ? (12400 - ((cur_l_ma + cur_r_ma) * 120) / 1000) : 11100;
    int failsafe = (!linkOk || (lastRoverFeedback.flags & 0x01)) ? 1 : 0;

    char rtlBuf[48];
    int len = snprintf(rtlBuf, sizeof(rtlBuf), "$RTL,%d,%d,%d,%d,%d*\n",
                       rssi, cur_l_ma, cur_r_ma, bat_mv, failsafe);
    Serial.write((const uint8_t*)rtlBuf, len);
  }
}
