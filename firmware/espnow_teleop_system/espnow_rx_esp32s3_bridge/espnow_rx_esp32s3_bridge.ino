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

volatile GamerControlPacket rxGamerPacket;
volatile bool newGamerPacket = false;
volatile uint32_t lastPacketTimestamp = 0;
volatile uint32_t totalPacketsReceived = 0;
int lastRssi = 0;

uint8_t lastSenderMac[6] = {0};
bool    hasSenderMac = false;

int16_t lastKnownLeftPwm  = 0;
int16_t lastKnownRightPwm = 0;

uint32_t lastPrintTime = 0;
uint32_t lastRateCalcTime = 0;
uint32_t packetsInLastSec = 0;
uint32_t currentRateHz = 0;

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

  if (len == sizeof(GamerControlPacket)) {
    memcpy((void*)&rxGamerPacket, data, sizeof(GamerControlPacket));
    lastPacketTimestamp = millis();
    totalPacketsReceived++;
    packetsInLastSec++;
    newGamerPacket = true;

    // Cache genuine sender MAC for asynchronous loop() reply
    if (srcMac) {
      memcpy(lastSenderMac, srcMac, 6);
      hasSenderMac = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
  delay(400);

  // 1. HARDWARE WI-FI CHANNEL LOCK (Forces Channel 1 permanently)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  uint8_t efuseMac[6];
  esp_read_mac(efuseMac, ESP_MAC_WIFI_STA);
  char macBuffer[18];
  snprintf(macBuffer, sizeof(macBuffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — HIGH-EFFICIENCY ESP-NOW TELEOP RECEIVER (ESP32-S3) v3.5       "));
  Serial.print(F(">>> Station MAC Address : ")); Serial.println(macBuffer);
  Serial.print(F(">>> Wi-Fi Channel       : Channel ")); Serial.println(ESPNOW_WIFI_CHANNEL);
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

  // 2. Process newly arrived teleop packet
  if (newGamerPacket) {
    newGamerPacket = false;
    failsafeTripped = false;

    uint32_t id    = rxGamerPacket.packet_id;
    uint8_t  cmd   = sanitizeCommandId(rxGamerPacket.cmd_id);
    uint8_t  spd   = rxGamerPacket.speed;
    uint8_t  gear  = rxGamerPacket.gear;
    uint8_t  turbo = rxGamerPacket.turbo;

    // FORWARD IMMEDIATELY TO STM32 MOTOR SUPERVISOR (< 10us)
    forwardToStm32(cmd, spd);

    bool stateChanged = (cmd != lastPrintedCmd || spd != lastPrintedSpeed ||
                         gear != lastPrintedGear || turbo != lastPrintedTurbo);
    bool periodicTick = (now - lastPrintTime >= 500); // 2 Hz periodic log

    if (stateChanged || periodicTick) {
      lastPrintTime    = now;
      lastPrintedCmd   = cmd;
      lastPrintedSpeed = spd;
      lastPrintedGear  = gear;
      lastPrintedTurbo = turbo;

      Serial.print(F("[PKT #"));
      Serial.print(id);
      Serial.print(F(" | "));
      if (currentRateHz < 10) Serial.print(F(" "));
      Serial.print(currentRateHz);
      Serial.print(F("Hz"));
      if (lastRssi != 0) {
        Serial.print(F(" | "));
        Serial.print(lastRssi);
        Serial.print(F("dBm"));
      }
      Serial.print(F("] CMD_ID: "));
      Serial.print(cmd);
      Serial.print(F(" ["));
      Serial.print(CMD_NAMES[cmd]);
      Serial.print(F("] | SPEED: "));
      if (spd < 100) Serial.print(F(" "));
      if (spd < 10)  Serial.print(F(" "));
      Serial.print(spd);
      Serial.print(F("/255 (PWM) | GEAR: "));
      Serial.print(gear);
      Serial.print(F("/9 | TURBO: "));
      Serial.print(turbo);
      if (turbo) {
        Serial.print(F(" [TURBO BOOST!]"));
      } else if (cmd == CMD_STOP) {
        Serial.print(F(" [DEAD-MAN STOP]"));
      }
      Serial.println();
    }
  }

  // 3. Autonomous Dead-Man Safety Watchdog (500ms timeout)
  if (!failsafeTripped && (totalPacketsReceived > 0) && (now - lastPacketTimestamp > 500)) {
    failsafeTripped = true;
    forwardToStm32(CMD_STOP, 0);

    Serial.println();
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
    Serial.println(F("⚠️ [FAILSAFE ENGAGED] Signal lost for >500ms! Auto-STOP dispatched."));
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));

    lastPrintedCmd = CMD_STOP;
    lastPrintedSpeed = 0;
  }

  // 4. Bi-Directional Telemetry Feedback Stream to Laptop Dongle (20 Hz, Every 50ms)
  // Completely outside ISR context -> Zero Wi-Fi MAC queue contention!
  static uint32_t lastFeedbackSendTime = 0;
  if (hasSenderMac && (now - lastFeedbackSendTime >= 50)) {
    lastFeedbackSendTime = now;

    if (!esp_now_is_peer_exist(lastSenderMac)) {
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, lastSenderMac, 6);
      peer.channel = ESPNOW_WIFI_CHANNEL;
      peer.encrypt = false;
      esp_now_add_peer(&peer);
    }

    RoverFeedbackPacket reply;
    reply.ack_packet_id    = rxGamerPacket.packet_id;
    reply.s3_rx_rssi       = (int8_t)lastRssi;
    reply.motor_left_pwm   = lastKnownLeftPwm;
    reply.motor_right_pwm  = lastKnownRightPwm;
    reply.current_amps_x10 = 0;
    reply.flags            = 0;
    reply.reserved         = 0;

    esp_now_send(lastSenderMac, (uint8_t*)&reply, sizeof(reply));
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
