/*
 * ================================================================================
 * CYBERROVER X5 — SINGLE-ESP32 INTEGRATED RECEIVER & MOTOR SUPERVISOR (v1.0)
 * ================================================================================
 * Target Hardware : Standard ESP32 DevKit V1 (30-pin or 38-pin WROOM-32)
 * Function        : Dedicated ESP-NOW 50 Hz Receiver + Dual BTS7960 43A Motor Driver
 * Compatibility   : Pairs 100% with Veer's CyberRover X5 Transmitter & USB Dongle
 * Architect       : Built for Autonomous UGV & Combat Robotics (30-Year Standard)
 * ================================================================================
 * HARDWARE PINOUT (ESP32 DevKit V1 -> Dual BTS7960 Drivers):
 * --------------------------------------------------------------------------------
 * [LEFT TRACK - BTS7960 #1]
 *   - L_RPWM (Forward PWM)  -> GPIO 25 (LEDC Channel 0, 20 kHz, 8-bit)
 *   - L_LPWM (Reverse PWM)  -> GPIO 26 (LEDC Channel 1, 20 kHz, 8-bit)
 *   - L_EN   (R_EN + L_EN)  -> GPIO 27 (Driver Enable: HIGH = Active, LOW = Sleep)
 *
 * [RIGHT TRACK - BTS7960 #2]
 *   - R_RPWM (Forward PWM)  -> GPIO 32 (LEDC Channel 2, 20 kHz, 8-bit)
 *   - R_LPWM (Reverse PWM)  -> GPIO 33 (LEDC Channel 3, 20 kHz, 8-bit)
 *   - R_EN   (R_EN + L_EN)  -> GPIO 14 (Driver Enable: HIGH = Active, LOW = Sleep)
 *
 * [STATUS INDICATOR]
 *   - Onboard Blue LED      -> GPIO 2  (Solid = Linked, Fast Blink = Failsafe/Waiting)
 *
 * [POWER & GROUND]
 *   - VIN Pin               -> 5.0V from UBEC / Buck Converter (NEVER feed 12V here!)
 *   - GND Pin               -> Common Star Ground with Battery & BTS7960 Drivers
 * ================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// -----------------------------------------------------------------------------
// HARDWARE PIN DEFINITIONS
// -----------------------------------------------------------------------------
#define PIN_L_RPWM         25  // Left Forward
#define PIN_L_LPWM         26  // Left Reverse
#define PIN_L_EN           27  // Left Driver Enable

#define PIN_R_RPWM         32  // Right Forward
#define PIN_R_LPWM         33  // Right Reverse
#define PIN_R_EN           14  // Right Driver Enable

#define PIN_STATUS_LED     2   // Onboard Blue LED

// -----------------------------------------------------------------------------
// PWM (LEDC) CONFIGURATION
// -----------------------------------------------------------------------------
#define PWM_FREQ_HZ        20000 // 20 kHz: Ultrasonic, silent motor drive (no coil whine)
#define PWM_RES_BITS       8     // 8-bit resolution (0 to 255)

#define LEDC_CH_L_FWD      0
#define LEDC_CH_L_REV      1
#define LEDC_CH_R_FWD      2
#define LEDC_CH_R_REV      3

// -----------------------------------------------------------------------------
// SAFETY & KINEMATIC TUNING
// -----------------------------------------------------------------------------
#define FAILSAFE_TIMEOUT_MS 650   // Dead-man timeout: stop motors if no packet in 650ms
#define MAX_ACCEL_STEP      15.0f // Slew-rate rate limiter: max PWM change per 20ms loop
#define MIN_DEADBAND        12    // Noise deadband around zero (-12 to +12 reads 0)

#define ESPNOW_CHANNEL      1     // Wi-Fi Channel 1 (Matches Transmitter)

// -----------------------------------------------------------------------------
// PROTOCOL DEFINITIONS (Matching Veer's Transmitter Exactly)
// -----------------------------------------------------------------------------
#define CYBER_DEVICE_ID     0xC5
#define CYBER_BTN_ARM_BIT   0

#pragma pack(push, 1)
// 16-Byte CyberPacket (Full Proportional Teleop from Handheld Remote)
struct CyberPacket {
  uint8_t  device_id;    // 0xC5
  uint8_t  seq_id;       // Rolling Counter
  uint16_t timestamp_ms; // TX Timestamp
  int8_t   axis_lx;      // Left Stick X (-100 to +100)
  int8_t   axis_ly;      // Left Stick Y (-100 to +100) [Throttle]
  int8_t   axis_rx;      // Right Stick X (-100 to +100) [Steering]
  int8_t   axis_ry;      // Right Stick Y (-100 to +100)
  uint8_t  speed_gov;    // Speed Governor (15 to 100%)
  uint8_t  turn_ratio;   // Dynamic Turn Ratio (0 to 75%)
  uint8_t  pot_aux;      // Aux Knob
  uint8_t  buttons;      // bit0: ARM (1=Armed, 0=Disarmed/Park)
  uint8_t  mode_flags;   // Profile & rate flags
  uint16_t frame_id;     // 16-bit Monotonic Frame ID
  uint8_t  crc8;         // CRC-8 Checksum
};

// 8-Byte GamerControlPacket (Compact Command from PC Gamepad / Dongle)
struct GamerControlPacket {
  uint32_t packet_id;    // Monotonic Frame Counter
  uint8_t  cmd_id;       // 0=Stop, 1=Fwd, 2=Back, 3=PivotL, 4=PivotR, 5=FL, 6=FR, 7=BL, 8=BR
  uint8_t  speed;        // Integer PWM (0 to 255)
  uint8_t  gear;         // Gear (0 to 9)
  uint8_t  turbo;        // 1=Turbo 100%, 0=Normal
};

// 12-Byte Return Feedback Packet (Sent back to Transmitter for Telemetry/RSSI)
struct RoverFeedbackPacket {
  uint32_t ack_packet_id;
  int8_t   rover_rx_rssi;
  int16_t  motor_left_pwm;
  int16_t  motor_right_pwm;
  uint8_t  current_amps_x10;
  uint8_t  flags;
  uint8_t  reserved;
};
#pragma pack(pop)

// -----------------------------------------------------------------------------
// GLOBAL STATE VARIABLES
// -----------------------------------------------------------------------------
volatile uint32_t lastPacketReceivedTime = 0;
volatile bool     linkActive             = false;
volatile bool     systemArmed            = false;
uint8_t           txAddress[6]           = {0};
bool              txPeerRegistered       = false;

// Target motor targets (-255.0 to +255.0)
float targetLeftPwm   = 0.0f;
float targetRightPwm  = 0.0f;

// Actual ramped motor outputs (-255.0 to +255.0)
float currentLeftPwm  = 0.0f;
float currentRightPwm = 0.0f;

uint32_t lastLoopTime = 0;

// -----------------------------------------------------------------------------
// CRC-8 ATM CALCULATION (Polynomial 0x07)
// -----------------------------------------------------------------------------
uint8_t computeCRC8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x07;
      else            crc <<= 1;
    }
  }
  return crc;
}

// -----------------------------------------------------------------------------
// HARDWARE MOTOR CONTROL (BTS7960 Direct Drive)
// -----------------------------------------------------------------------------
void setMotorsHardware(int16_t leftPwm, int16_t rightPwm) {
  // Constrain inputs
  leftPwm  = constrain(leftPwm, -255, 255);
  rightPwm = constrain(rightPwm, -255, 255);

  // LEFT TRACK
  if (leftPwm > 0) {
    ledcWrite(LEDC_CH_L_FWD, leftPwm);
    ledcWrite(LEDC_CH_L_REV, 0);
  } else if (leftPwm < 0) {
    ledcWrite(LEDC_CH_L_FWD, 0);
    ledcWrite(LEDC_CH_L_REV, abs(leftPwm));
  } else {
    // Low-side MOSFET braking (both inputs 0 while EN=HIGH)
    ledcWrite(LEDC_CH_L_FWD, 0);
    ledcWrite(LEDC_CH_L_REV, 0);
  }

  // RIGHT TRACK
  if (rightPwm > 0) {
    ledcWrite(LEDC_CH_R_FWD, rightPwm);
    ledcWrite(LEDC_CH_R_REV, 0);
  } else if (rightPwm < 0) {
    ledcWrite(LEDC_CH_R_FWD, 0);
    ledcWrite(LEDC_CH_R_REV, abs(rightPwm));
  } else {
    // Low-side MOSFET braking
    ledcWrite(LEDC_CH_R_FWD, 0);
    ledcWrite(LEDC_CH_R_REV, 0);
  }
}

// Emergency stop all motors immediately
void emergencyStop() {
  targetLeftPwm   = 0.0f;
  targetRightPwm  = 0.0f;
  currentLeftPwm  = 0.0f;
  currentRightPwm = 0.0f;
  setMotorsHardware(0, 0);
}

// -----------------------------------------------------------------------------
// DIFFERENTIAL DRIVE KINEMATICS (Skid Steering Mix)
// -----------------------------------------------------------------------------
void computeSkidSteering(float throttlePct, float steerPct, float speedGovPct, float turnRatioPct) {
  // Deadband check
  if (abs(throttlePct) < 5.0f) throttlePct = 0.0f;
  if (abs(steerPct)    < 5.0f) steerPct    = 0.0f;

  // Scale throttle by speed governor (0.15 to 1.00)
  float govFactor = constrain(speedGovPct / 100.0f, 0.15f, 1.0f);
  float scaledThrottle = throttlePct * govFactor;

  // Scale steering by dynamic turn ratio
  float turnFactor = constrain(turnRatioPct / 100.0f, 0.30f, 1.0f);
  float scaledSteer = steerPct * turnFactor;

  // Differential mix
  float rawLeft  = (scaledThrottle + scaledSteer) * 2.55f; // Convert -100..+100 to -255..+255
  float rawRight = (scaledThrottle - scaledSteer) * 2.55f;

  // Normalization if saturated
  float maxMag = max(abs(rawLeft), abs(rawRight));
  if (maxMag > 255.0f) {
    rawLeft  = (rawLeft / maxMag) * 255.0f;
    rawRight = (rawRight / maxMag) * 255.0f;
  }

  targetLeftPwm  = rawLeft;
  targetRightPwm = rawRight;
}

// Parse 8-byte discrete command
void computeDiscreteCommand(uint8_t cmd_id, uint8_t speed) {
  float spd = (float)speed;
  switch (cmd_id) {
    case 1: targetLeftPwm =  spd; targetRightPwm =  spd; break; // Forward
    case 2: targetLeftPwm = -spd; targetRightPwm = -spd; break; // Backward
    case 3: targetLeftPwm = -spd; targetRightPwm =  spd; break; // 360° Pivot Left
    case 4: targetLeftPwm =  spd; targetRightPwm = -spd; break; // 360° Pivot Right
    case 5: targetLeftPwm = spd * 0.4f; targetRightPwm = spd; break; // Curve Left
    case 6: targetLeftPwm = spd; targetRightPwm = spd * 0.4f; break; // Curve Right
    case 7: targetLeftPwm = -spd * 0.4f; targetRightPwm = -spd; break; // Reverse Left
    case 8: targetLeftPwm = -spd; targetRightPwm = -spd * 0.4f; break; // Reverse Right
    case 0:
    default:
      targetLeftPwm = 0.0f;
      targetRightPwm = 0.0f;
      break;
  }
}

// -----------------------------------------------------------------------------
// ESP-NOW PACKET RECEIVE CALLBACK
// -----------------------------------------------------------------------------
void onDataReceive(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  lastPacketReceivedTime = millis();
  linkActive = true;

  // Auto-register peer for return feedback if not already registered
  if (!txPeerRegistered) {
    memcpy(txAddress, mac_addr, 6);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, txAddress, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      txPeerRegistered = true;
      Serial.printf("[ESP-NOW] Transmitter Auto-Paired: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    }
  }

  // CASE 1: 16-Byte CyberPacket (Proportional Handheld Remote)
  if (len == sizeof(CyberPacket)) {
    CyberPacket *pkt = (CyberPacket*)incomingData;

    // Validate Device ID and CRC
    if (pkt->device_id == CYBER_DEVICE_ID) {
      uint8_t expectedCRC = computeCRC8(incomingData, sizeof(CyberPacket) - 1);
      if (expectedCRC == pkt->crc8) {
        // Read ARM status (bit 0)
        systemArmed = (pkt->buttons & (1 << CYBER_BTN_ARM_BIT)) != 0;

        if (systemArmed) {
          computeSkidSteering((float)pkt->axis_ly, (float)pkt->axis_rx,
                              (float)pkt->speed_gov, (float)pkt->turn_ratio);
        } else {
          // Park Mode: disarmed
          targetLeftPwm  = 0.0f;
          targetRightPwm = 0.0f;
        }
      }
    }
  }
  // CASE 2: 8-Byte GamerControlPacket (PC Gamepad / Dongle)
  else if (len == sizeof(GamerControlPacket)) {
    GamerControlPacket *pkt = (GamerControlPacket*)incomingData;
    systemArmed = true;
    computeDiscreteCommand(pkt->cmd_id, pkt->speed);
  }

  // Send back return telemetry heartbeat
  if (txPeerRegistered) {
    RoverFeedbackPacket fb = {};
    fb.ack_packet_id   = millis();
    fb.rover_rx_rssi   = -45; // Approximate strong near-field RSSI
    fb.motor_left_pwm  = (int16_t)currentLeftPwm;
    fb.motor_right_pwm = (int16_t)currentRightPwm;
    fb.flags           = systemArmed ? 0x01 : 0x00;
    esp_now_send(txAddress, (uint8_t*)&fb, sizeof(fb));
  }
}

// -----------------------------------------------------------------------------
// SETUP INITIALIZATION
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n========================================================");
  Serial.println("  CYBERROVER X5 — SINGLE-ESP32 RECEIVER & MOTOR DRIVER  ");
  Serial.println("========================================================");

  // Initialize LED
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  // Initialize Enable Pins
  pinMode(PIN_L_EN, OUTPUT);
  pinMode(PIN_R_EN, OUTPUT);
  digitalWrite(PIN_L_EN, HIGH); // Enable BTS7960 Left
  digitalWrite(PIN_R_EN, HIGH); // Enable BTS7960 Right

  // Initialize Hardware PWM Channels (LEDC)
  ledcSetup(LEDC_CH_L_FWD, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(LEDC_CH_L_REV, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(LEDC_CH_R_FWD, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(LEDC_CH_R_REV, PWM_FREQ_HZ, PWM_RES_BITS);

  ledcAttachPin(PIN_L_RPWM, LEDC_CH_L_FWD);
  ledcAttachPin(PIN_L_LPWM, LEDC_CH_L_REV);
  ledcAttachPin(PIN_R_RPWM, LEDC_CH_R_FWD);
  ledcAttachPin(PIN_R_LPWM, LEDC_CH_R_REV);

  // Safety: all motors strictly zero at boot
  emergencyStop();

  // Initialize Wi-Fi in Station Mode for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Lock to Channel 1 (Zero hopping)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW Initialization Failed! Halting.");
    while (1) {
      digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
      delay(100);
    }
  }

  esp_now_register_recv_cb(onDataReceive);

  Serial.printf("[INFO] Rover ESP-NOW MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.println("[INFO] Waiting for Transmitter Packets on Channel 1...");
}

// -----------------------------------------------------------------------------
// MAIN SUPERVISOR LOOP (50 Hz / 20ms Execution Cycle)
// -----------------------------------------------------------------------------
void loop() {
  uint32_t now = millis();

  // Run cycle every 20ms (50 Hz)
  if (now - lastLoopTime >= 20) {
    lastLoopTime = now;

    // 1. WATCHDOG FAILSAFE CHECK (650ms timeout)
    if (now - lastPacketReceivedTime > FAILSAFE_TIMEOUT_MS) {
      if (linkActive) {
        linkActive = false;
        emergencyStop();
        Serial.println("[FAILSAFE] Radio Signal Lost! Emergency Brake Engaged.");
      }
      // Blink LED slowly while waiting for signal
      digitalWrite(PIN_STATUS_LED, (now / 500) % 2);
    } else {
      // Signal Active
      if (!systemArmed) {
        // Disarmed / Park Mode: blink LED fast
        emergencyStop();
        digitalWrite(PIN_STATUS_LED, (now / 150) % 2);
      } else {
        // Armed & Running: Solid Blue LED
        digitalWrite(PIN_STATUS_LED, HIGH);

        // 2. SLEW-RATE ACCELERATION LIMITER (Protects gears & prevents wheelies)
        float diffL = targetLeftPwm - currentLeftPwm;
        if (abs(diffL) > MAX_ACCEL_STEP) {
          currentLeftPwm += (diffL > 0) ? MAX_ACCEL_STEP : -MAX_ACCEL_STEP;
        } else {
          currentLeftPwm = targetLeftPwm;
        }

        float diffR = targetRightPwm - currentRightPwm;
        if (abs(diffR) > MAX_ACCEL_STEP) {
          currentRightPwm += (diffR > 0) ? MAX_ACCEL_STEP : -MAX_ACCEL_STEP;
        } else {
          currentRightPwm = targetRightPwm;
        }

        // Apply deadband
        int16_t outL = (abs(currentLeftPwm)  < MIN_DEADBAND) ? 0 : (int16_t)currentLeftPwm;
        int16_t outR = (abs(currentRightPwm) < MIN_DEADBAND) ? 0 : (int16_t)currentRightPwm;

        // 3. DRIVE MOTORS
        setMotorsHardware(outL, outR);
      }
    }
  }
}
