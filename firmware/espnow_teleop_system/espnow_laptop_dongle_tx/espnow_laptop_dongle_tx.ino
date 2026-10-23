/*
 * ================================================================================
 * CYBERROVER X5 — ESP-NOW USB LAPTOP DONGLE (BI-DIRECTIONAL LINK & RSSI v3.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target Hardware: AI-Thinker ESP32-CAM (Plugged into Laptop USB via FTDI / CH340)
 * Subsystem      : Real-Time Teleoperation Dongle with Bi-Directional RSSI & Feedback
 *
 * NEW IN v3.5:
 *   1. Hardware Channel Lock: Enforces Wi-Fi Channel 1 via 'esp_wifi_set_channel()'.
 *   2. Dual-Address Transmission: Unicast to Target MAC + Broadcast fallback.
 *   3. Bi-Directional Packet Return: Receives 'RoverFeedbackPacket' from Rover S3.
 *   4. Live Rover RSSI at Dongle Antenna: Measures true RF signal strength in dBm.
 *   5. Real-Time Telemetry Stream to Python: Reports RSSI, ACK status, and Motor PWM.
 * ================================================================================
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#define ESPNOW_WIFI_CHANNEL 1

// Target Rover ESP32-S3 MAC Address (Default Factory efuse)
uint8_t roverAddress[6]     = { 0x1C, 0xDB, 0xD4, 0x4B, 0x08, 0x40 };
uint8_t broadcastAddress[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
bool peerRegistered = false;
bool broadcastRegistered = false;

// -----------------------------------------------------------------------------
// Control Packet to Rover S3 (8 Bytes)
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

GamerControlPacket txPacket;
RoverFeedbackPacket lastRoverFeedback;

uint32_t txCounter = 0;
bool     lastTxSuccess = false;
uint32_t totalPacketsSent = 0;
uint32_t totalAcksReceived = 0;
int      lastRoverRssiAtDongle = 0;
uint32_t lastFeedbackTime = 0;

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
    }
  }
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  lastRoverRssiAtDongle = 0;
  if (mac && memcmp(roverAddress, mac, 6) != 0) {
    updatePeerAddress(mac);
  }
#endif

  if (len == sizeof(RoverFeedbackPacket)) {
    memcpy(&lastRoverFeedback, data, sizeof(RoverFeedbackPacket));
    lastFeedbackTime = millis();
    lastTxSuccess = true;
    totalAcksReceived++;
  }
}

// Transmit integer packet over ESP-NOW
bool transmitPacket(uint8_t cmd_id, uint8_t speed, uint8_t gear, uint8_t turbo) {
  txCounter++;
  totalPacketsSent++;
  txPacket.packet_id = txCounter;
  txPacket.cmd_id    = (cmd_id <= 8) ? cmd_id : 0;
  txPacket.speed     = speed;
  txPacket.gear      = gear;
  txPacket.turbo     = turbo;

  esp_err_t res = ESP_OK;
  // If peer registered, unicast directly; fallback to broadcast periodically if searching
  if (peerRegistered && (lastTxSuccess || (txCounter % 10 != 0))) {
    res = esp_now_send(roverAddress, (uint8_t *)&txPacket, sizeof(txPacket));
  } else {
    res = esp_now_send(broadcastAddress, (uint8_t *)&txPacket, sizeof(txPacket));
  }

  return (res == ESP_OK);
}

void setup() {
  Serial.begin(115200);
  delay(400);

  // 1. HARDWARE WI-FI CHANNEL LOCK (Forces Channel 1 permanently)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
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

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — ESP32-CAM USB DONGLE (BI-DIRECTIONAL TELEOP v3.5)            "));
  Serial.print(F(">>> Dongle MAC     : ")); Serial.println(dMacStr);
  Serial.print(F(">>> Target Rover S3: ")); Serial.println(rMacStr);
  Serial.print(F(">>> Channel        : Wi-Fi Channel ")); Serial.println(ESPNOW_WIFI_CHANNEL);
  Serial.println(F("================================================================================\n"));
}

void loop() {
  uint32_t now = millis();

  while (Serial.available()) {
    int peekByte = Serial.peek();

    // CASE A: Dynamic MAC configuration command (e.g. "MAC:1C:DB:D4:4B:08:40\n")
    if (peekByte == 'M') {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.startsWith("MAC:")) {
        String macStr = line.substring(4);
        macStr.replace(":", ""); macStr.replace("-", ""); macStr.replace(" ", "");
        if (macStr.length() == 12) {
          uint8_t newMac[6];
          for (int i = 0; i < 6; i++) {
            String byteStr = macStr.substring(i * 2, i * 2 + 2);
            newMac[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
          }
          if (updatePeerAddress(newMac)) {
            Serial.print(F("MAC_SET_OK:"));
            Serial.println(line.substring(4));
          }
        }
      }
      continue;
    }

    // CASE B: Framed 6-Byte High-Speed Teleop Packet from Python:
    // Format: [0xAA] [CMD_ID:uint8] [SPEED:uint8] [GEAR:uint8] [TURBO:uint8] [0x55]
    if (peekByte == 0xAA) {
      if (Serial.available() >= 6) {
        uint8_t header = Serial.read(); // 0xAA
        uint8_t rawCmd = Serial.read(); // Command ID (0 to 8)
        uint8_t spd    = Serial.read(); // PWM (0 - 255)
        uint8_t gear   = Serial.read(); // Gear (0 - 9)
        uint8_t turbo  = Serial.read(); // Turbo flag (0 or 1)
        uint8_t footer = Serial.read(); // 0x55

        if (footer == 0x55) {
          uint8_t clean_cmd = sanitizeCommandId(rawCmd);
          transmitPacket(clean_cmd, spd, gear, turbo);

          // Report live status back to Python HUD every 5 packets (~15 Hz)
          if (txCounter % 5 == 0) {
            bool linkAlive = (now - lastFeedbackTime < 500) || lastTxSuccess;
            Serial.print(F("STAT:"));
            Serial.print(linkAlive ? "ACK_OK" : "NO_ACK");
            Serial.print(F(",RSSI:"));
            Serial.print(lastRoverRssiAtDongle);
            Serial.print(F(",SENT:"));
            Serial.print(totalPacketsSent);
            Serial.print(F(",ACKS:"));
            Serial.print(totalAcksReceived);
            Serial.print(F(",ML:"));
            Serial.print(lastRoverFeedback.motor_left_pwm);
            Serial.print(F(",MR:"));
            Serial.print(lastRoverFeedback.motor_right_pwm);
            Serial.println();
          }
        }
      } else {
        break;
      }
      continue;
    }

    // CASE C: Text commands from Serial Monitor
    String typed = Serial.readStringUntil('\n');
    typed.trim();
    if (typed.length() > 0) {
      uint8_t cmd_val = sanitizeCommandId((uint8_t)typed.charAt(0));
      transmitPacket(cmd_val, 150, 5, 0);
      Serial.print(F(">>> Sent CMD: ")); Serial.println(cmd_val);
    }
  }
}
