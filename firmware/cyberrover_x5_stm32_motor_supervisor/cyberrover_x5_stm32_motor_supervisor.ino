/*
 * ================================================================================
 * CYBERROVER X5 — STM32 MOTOR SUPERVISOR & HARDWARE GEARBOX PROTECTOR v1.1
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCU     : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Autonomous Drivetrain Slew-Rate Limiter & Transport Bridge
 *
 * NEW IN v1.1 (GEARBOX SHOCK & INDOOR UNPREDICTABILITY PROTECTION):
 *   1. 100 Hz Slew-Rate Acceleration Limiter:
 *      - Replaces instant speed jumps with smooth progressive ramps (ACCEL_STEP = 5.0).
 *      - Eliminates stripped spur gear teeth, motor shaft shear, and battery voltage sag.
 *   2. Asymmetric Deceleration (DECEL_STEP = 12.0):
 *      - Brakes swiftly (~120ms) for high safety without violent backward flip inertia.
 *   3. 40ms Direction Reversal Dead-Time:
 *      - Enforces a neutral dwell when reversing polarity, preventing H-bridge shoot-through.
 *   4. Transparent Transport Independence:
 *      - STM32 handles 100% of motor physics, current sensing, and watchdog fail-safes.
 *      - The upstream receiver (ESP32-S3, ESP-NOW, or NRF24) is a pure plug-and-play radio!
 *
 * PIN CONNECTIONS:
 *   - Right Motor: PB6 (Forward PWM), PB7 (Reverse PWM)
 *   - Left Motor : PB9 (Forward PWM), PB8 (Reverse PWM)
 *   - Current IS : PA0 (L_FWD), PA1 (L_REV), PB0 (R_FWD), PB1 (R_REV)
 *   - Status LED : PB5 (RED), PA11 (BLUE) [Common Anode Active-LOW]
 *   - UART Bridge: PA9 (TX1 to RX), PA10 (RX1 to TX) @ 115200 baud
 * ================================================================================
 */

#include <Arduino.h>

// Drivetrain PWM Pins (Verified)
#define PIN_RIGHT_RPWM   PB6
#define PIN_RIGHT_LPWM   PB7
#define PIN_LEFT_RPWM    PB9
#define PIN_LEFT_LPWM    PB8

// Current Sense Analog Pins
#define PIN_LEFT_R_IS    PA0
#define PIN_LEFT_L_IS    PA1
#define PIN_RIGHT_R_IS   PB0
#define PIN_RIGHT_L_IS   PB1

// Status LED Pins (Common Anode: Active-LOW)
#define PIN_LED_RED      PB5
#define PIN_LED_BLUE     PA11
#define PIN_LED_HB       PC13

// -----------------------------------------------------------------------------
// Drivetrain Ramping & Gearbox Protection Dynamics (100 Hz Loop)
// -----------------------------------------------------------------------------
int16_t targetSpeedLeft   = 0;
int16_t targetSpeedRight  = 0;
float   actualSpeedLeft   = 0.0f;
float   actualSpeedRight  = 0.0f;
uint8_t currentTurnRatio  = 33; // Dynamic turning ratio (0% = sharp skid pivot, 75% = wide sweep)

// Slew Rates per 10ms tick (at 100 Hz):
// ACCEL_STEP = 8.0 -> 800 PWM/sec (~140ms from 48 to 165 cruise). Crisp, punchy, immediate throttle response!
// DECEL_STEP = 18.0 -> 1800 PWM/sec (~90ms from 165 to 0 stop). Fast, crisp, safe dead-man stop!
const float ACCEL_STEP    = 8.0f;
const float DECEL_STEP    = 18.0f;
const float MIN_START_PWM = 48.0f; // Soft-start inception threshold to bypass static friction deadband
const float MIN_DRIVE_PWM = 45.0f; // Below this, motors lock to 0V ground immediately

uint32_t lastRampTick     = 0;
uint32_t lastZeroDwellL   = 0;
uint32_t lastZeroDwellR   = 0;
const uint32_t REVERSE_DWELL_MS = 35; // 35ms pause when reversing polarity

// Watchdog & Telemetry Timing
const uint32_t WATCHDOG_TIMEOUT_MS = 650; // 650ms dead-man failsafe (absorbs 2.4 GHz RF jitter, stops false cutouts)
uint32_t lastCommandTime = 0;
bool watchdogTripped = true;
uint32_t lastTelemTime = 0;

// Optical LED Helpers
void ledOff()     { digitalWrite(PIN_LED_RED, HIGH); digitalWrite(PIN_LED_BLUE, HIGH); }
void ledRed()     { digitalWrite(PIN_LED_RED, LOW);  digitalWrite(PIN_LED_BLUE, HIGH); }
void ledBlue()    { digitalWrite(PIN_LED_RED, HIGH); digitalWrite(PIN_LED_BLUE, LOW);  }
void ledMagenta() { digitalWrite(PIN_LED_RED, LOW);  digitalWrite(PIN_LED_BLUE, LOW);  }

// Motor Direction & Orientation Inversion Flags
const bool INVERT_LEFT_MOTOR  = false; // Left motor forward when positive PWM
const bool INVERT_RIGHT_MOTOR = false; // Right motor forward when positive PWM (Verified: Left=+S, Right=+S drives straight forward!)
const bool SWAP_LEFT_RIGHT    = false; // Left and Right channels aligned correctly

// Hardware State Cache to prevent thrashing timer registers 100 times/second
int16_t lastWrittenLeftPwm  = -999;
int16_t lastWrittenRightPwm = -999;

// Forward declaration of telemetry helper
void sendTelemetryToBridge(int16_t leftPwm, int16_t rightPwm, uint32_t now);

// Hardware PWM Register Write (with absolute 0.00V analogWrite ground lock)
void writeHardwarePWM(int16_t leftPwm, int16_t rightPwm) {
  // Apply Channel Swapping if configured
  if (SWAP_LEFT_RIGHT) {
    int16_t temp = leftPwm;
    leftPwm = rightPwm;
    rightPwm = temp;
  }

  // Apply Direction Inversion if configured
  if (INVERT_LEFT_MOTOR)  leftPwm  = -leftPwm;
  if (INVERT_RIGHT_MOTOR) rightPwm = -rightPwm;

  // If output values have not changed, return early to save CPU
  if (leftPwm == lastWrittenLeftPwm && rightPwm == lastWrittenRightPwm) return;
  lastWrittenLeftPwm  = leftPwm;
  lastWrittenRightPwm = rightPwm;

  // Left Motor Driver (PB9 = FWD, PB8 = REV)
  if (leftPwm > 0) {
    analogWrite(PIN_LEFT_LPWM, 0);
    analogWrite(PIN_LEFT_RPWM, constrain(leftPwm, 0, 255));
  } else if (leftPwm < 0) {
    analogWrite(PIN_LEFT_RPWM, 0);
    analogWrite(PIN_LEFT_LPWM, constrain(-leftPwm, 0, 255));
  } else {
    // True 0.00% PWM Timer Zero + 0.00V GND Latch (Completely stops motor crawl!)
    analogWrite(PIN_LEFT_RPWM, 0);
    analogWrite(PIN_LEFT_LPWM, 0);
    digitalWrite(PIN_LEFT_RPWM, LOW);
    digitalWrite(PIN_LEFT_LPWM, LOW);
  }

  // Right Motor Driver (PB6 = FWD, PB7 = REV)
  if (rightPwm > 0) {
    analogWrite(PIN_RIGHT_LPWM, 0);
    analogWrite(PIN_RIGHT_RPWM, constrain(rightPwm, 0, 255));
  } else if (rightPwm < 0) {
    analogWrite(PIN_RIGHT_RPWM, 0);
    analogWrite(PIN_RIGHT_LPWM, constrain(-rightPwm, 0, 255));
  } else {
    // True 0.00% PWM Timer Zero + 0.00V GND Latch (Completely stops motor crawl!)
    analogWrite(PIN_RIGHT_RPWM, 0);
    analogWrite(PIN_RIGHT_LPWM, 0);
    digitalWrite(PIN_RIGHT_RPWM, LOW);
    digitalWrite(PIN_RIGHT_LPWM, LOW);
  }
}

// -----------------------------------------------------------------------------
// Non-Blocking 100 Hz Motor Slew-Rate Limiter Engine
// -----------------------------------------------------------------------------
void updateRampingEngine(uint32_t now) {
  if (now - lastRampTick < 10) return;
  lastRampTick = now;

  // 1. RAMP LEFT MOTOR
  if ((targetSpeedLeft > 0 && actualSpeedLeft < 0) || (targetSpeedLeft < 0 && actualSpeedLeft > 0)) {
    // Direction reversal in progress -> brake to zero first
    if (actualSpeedLeft > 0) {
      actualSpeedLeft -= DECEL_STEP;
      if (actualSpeedLeft <= 0) { actualSpeedLeft = 0; lastZeroDwellL = now; }
    } else {
      actualSpeedLeft += DECEL_STEP;
      if (actualSpeedLeft >= 0) { actualSpeedLeft = 0; lastZeroDwellL = now; }
    }
  } else if (actualSpeedLeft == 0 && (now - lastZeroDwellL < REVERSE_DWELL_MS)) {
    // Dwell at 0 for 40ms to protect spur gears from direction shock
  } else {
    // Standard acceleration / deceleration
    if (targetSpeedLeft > actualSpeedLeft) {
      // Soft-Start Inception: jump past static friction deadband immediately when starting from rest
      if (actualSpeedLeft == 0 && targetSpeedLeft >= MIN_START_PWM) {
        actualSpeedLeft = MIN_START_PWM;
      }
      float step = (actualSpeedLeft < 0) ? DECEL_STEP : ACCEL_STEP;
      actualSpeedLeft += step;
      if (actualSpeedLeft > targetSpeedLeft) actualSpeedLeft = targetSpeedLeft;
    } else if (targetSpeedLeft < actualSpeedLeft) {
      if (actualSpeedLeft == 0 && targetSpeedLeft <= -MIN_START_PWM) {
        actualSpeedLeft = -MIN_START_PWM;
      }
      float step = (actualSpeedLeft > 0) ? DECEL_STEP : ACCEL_STEP;
      actualSpeedLeft -= step;
      if (actualSpeedLeft < targetSpeedLeft) actualSpeedLeft = targetSpeedLeft;
    }
  }

  // 2. RAMP RIGHT MOTOR
  if ((targetSpeedRight > 0 && actualSpeedRight < 0) || (targetSpeedRight < 0 && actualSpeedRight > 0)) {
    // Direction reversal in progress -> brake to zero first
    if (actualSpeedRight > 0) {
      actualSpeedRight -= DECEL_STEP;
      if (actualSpeedRight <= 0) { actualSpeedRight = 0; lastZeroDwellR = now; }
    } else {
      actualSpeedRight += DECEL_STEP;
      if (actualSpeedRight >= 0) { actualSpeedRight = 0; lastZeroDwellR = now; }
    }
  } else if (actualSpeedRight == 0 && (now - lastZeroDwellR < REVERSE_DWELL_MS)) {
    // Dwell at 0 for 35ms
  } else {
    if (targetSpeedRight > actualSpeedRight) {
      // Soft-Start Inception: jump past static friction deadband immediately when starting from rest
      if (actualSpeedRight == 0 && targetSpeedRight >= MIN_START_PWM) {
        actualSpeedRight = MIN_START_PWM;
      }
      float step = (actualSpeedRight < 0) ? DECEL_STEP : ACCEL_STEP;
      actualSpeedRight += step;
      if (actualSpeedRight > targetSpeedRight) actualSpeedRight = targetSpeedRight;
    } else if (targetSpeedRight < actualSpeedRight) {
      if (actualSpeedRight == 0 && targetSpeedRight <= -MIN_START_PWM) {
        actualSpeedRight = -MIN_START_PWM;
      }
      float step = (actualSpeedRight > 0) ? DECEL_STEP : ACCEL_STEP;
      actualSpeedRight -= step;
      if (actualSpeedRight < targetSpeedRight) actualSpeedRight = targetSpeedRight;
    }
  }

  // Snap to 0 immediately when target is 0 and speed drops below minimum threshold
  int16_t outL = (int16_t)actualSpeedLeft;
  int16_t outR = (int16_t)actualSpeedRight;
  if (targetSpeedLeft == 0 && abs(outL) <= 45) { actualSpeedLeft = 0; outL = 0; }
  if (targetSpeedRight == 0 && abs(outR) <= 45) { actualSpeedRight = 0; outR = 0; }

  writeHardwarePWM(outL, outR);

  // Send lightweight non-blocking telemetry to ESP32-S3 bridge (10 Hz, never blocks UART!)
  sendTelemetryToBridge(outL, outR, now);

  // ---------------------------------------------------------------------------
  // Human-Readable Diagnostic Logging (STRICTLY to USB Serial, NEVER Serial1!)
  // ---------------------------------------------------------------------------
  static int16_t  lastReportedL = -999;
  static int16_t  lastReportedR = -999;
  static uint32_t lastMotorLogTime = 0;

  bool speedChanged = (abs(outL - lastReportedL) >= 20 ||
                       (outL == 0 && lastReportedL != 0) || (outR == 0 && lastReportedR != 0) ||
                       (outL == targetSpeedLeft && lastReportedL != targetSpeedLeft));
  bool periodicTick = (now - lastMotorLogTime >= 500 && (outL != 0 || outR != 0));

  if (speedChanged || periodicTick) {
    lastReportedL = outL;
    lastReportedR = outR;
    lastMotorLogTime = now;

    if (outL == 0 && outR == 0) {
      Serial.println(F("[MOTORS] STOPPED: Left=0 PWM | Right=0 PWM | 0.00V GND Locked"));
    } else {
      const char* mState = (outL == targetSpeedLeft && outR == targetSpeedRight) ? "CRUISING" : "RAMPING";
      char mBuf[96];
      snprintf(mBuf, sizeof(mBuf), "[MOTORS] %s: Left=%+4d PWM | Right=%+4d PWM | Target: L=%+4d R=%+4d",
               mState, outL, outR, targetSpeedLeft, targetSpeedRight);
      Serial.println(mBuf);
    }
  }

  // Update Optical Status
  if (outL == 0 && outR == 0) {
    if (watchdogTripped) ledRed();
    else ledOff();
  } else if (outL > 0 && outR < 0) {
    ledMagenta(); // Tank Spin CW
  } else if (outL < 0 && outR > 0) {
    ledMagenta(); // Tank Spin CCW
  } else if (outL < 0 && outR < 0) {
    ledRed();     // Reverse
  } else {
    ledBlue();    // Forward / Steering
  }
}

void sendTelemetryToBridge(int16_t leftPwm, int16_t rightPwm, uint32_t now) {
  static uint32_t lastBridgeTelem = 0;
  if (now - lastBridgeTelem >= 100) { // 10 Hz non-blocking stream
    lastBridgeTelem = now;
#if defined(Serial1)
    if ((void*)&Serial1 != (void*)&Serial) {
      char b[16];
      int n = snprintf(b, sizeof(b), "$M,%d,%d\n", (int)leftPwm, (int)rightPwm);
      Serial1.write((const uint8_t*)b, n);
    }
#endif
  }
}

float readAmps(uint8_t pin) {
  uint32_t raw = 0;
  for (int i = 0; i < 4; i++) raw += analogRead(pin);
  raw /= 4;
  return ((raw * 3.3f) / 4095.0f) * 8.5f;
}

void setup() {
  // 1. HARD HARDWARE LOCKOUT: Force all 4 PWM pins to 0.0V GND immediately before doing anything!
  pinMode(PIN_RIGHT_RPWM, OUTPUT);
  pinMode(PIN_RIGHT_LPWM, OUTPUT);
  pinMode(PIN_LEFT_RPWM, OUTPUT);
  pinMode(PIN_LEFT_LPWM, OUTPUT);

  analogWrite(PIN_RIGHT_RPWM, 0);
  analogWrite(PIN_RIGHT_LPWM, 0);
  analogWrite(PIN_LEFT_RPWM, 0);
  analogWrite(PIN_LEFT_LPWM, 0);

  digitalWrite(PIN_RIGHT_RPWM, LOW);
  digitalWrite(PIN_RIGHT_LPWM, LOW);
  digitalWrite(PIN_LEFT_RPWM, LOW);
  digitalWrite(PIN_LEFT_LPWM, LOW);

  lastWrittenLeftPwm  = 0;
  lastWrittenRightPwm = 0;

  Serial.begin(115200);
#if defined(Serial1)
  Serial1.begin(115200);
#endif
  analogReadResolution(12);

  pinMode(PIN_LEFT_R_IS, INPUT_ANALOG);
  pinMode(PIN_LEFT_L_IS, INPUT_ANALOG);
  pinMode(PIN_RIGHT_R_IS, INPUT_ANALOG);
  pinMode(PIN_RIGHT_L_IS, INPUT_ANALOG);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_HB, OUTPUT);
  digitalWrite(PIN_LED_HB, HIGH);

  // Optical boot blip
  ledRed();   delay(50);
  ledBlue();  delay(50);
  ledOff();

  Serial.println(F("\n========================================================"));
  Serial.println(F("  CYBERROVER X5 — STM32 MOTOR SUPERVISOR v1.2 ACTIVE"));
  Serial.println(F("  Hardware Drivetrain & Gearbox Protection Online"));
  Serial.println(F("  Listening on PA10 (RX) @ 115200 baud for Teleop Frames"));
  Serial.println(F("========================================================\n"));

  lastCommandTime = millis();
  watchdogTripped = true;
}

// -----------------------------------------------------------------------------
// FSM SERIAL PACKET DECODER ([0xAA] [CMD] [SPEED] [CHECKSUM] [0x55])
// -----------------------------------------------------------------------------
uint8_t rxState = 0;
char    rxCmd   = 'S';
uint8_t rxSpeed = 0;
uint8_t rxCsum  = 0;

void executeCommand(char cmd, uint8_t speed) {
  switch (cmd) {
    case 'F': // Forward Drive (W)
    case 1:
      targetSpeedLeft  = speed;
      targetSpeedRight = speed;
      break;

    case 'B': // Reverse Drive (S)
    case 2:
      targetSpeedLeft  = -speed;
      targetSpeedRight = -speed;
      break;

    case 'Q': // Left Pivot / Spin CCW (A)
    case 3:
      targetSpeedLeft  = -speed;
      targetSpeedRight = speed;
      break;

    case 'E': // Right Pivot / Spin CW (D)
    case 4:
      targetSpeedLeft  = speed;
      targetSpeedRight = -speed;
      break;

    case 'T': // Dynamic Turn Ratio Update from Remote Center Pot (0 - 75%)
      currentTurnRatio = constrain(speed, 0, 75);
      break;

    case 'L': // Front Left Turn (W + A / Q)
    case 5:
      targetSpeedLeft  = (speed * currentTurnRatio) / 100;
      targetSpeedRight = speed;
      break;

    case 'R': // Front Right Turn (W + D / E)
    case 6:
      targetSpeedLeft  = speed;
      targetSpeedRight = (speed * currentTurnRatio) / 100;
      break;

    case 'G': // Back Left Turn (S + A / Z)
    case 7:
      targetSpeedLeft  = -((speed * currentTurnRatio) / 100);
      targetSpeedRight = -speed;
      break;

    case 'I': // Back Right Turn (S + D / C)
    case 8:
      targetSpeedLeft  = -speed;
      targetSpeedRight = -((speed * currentTurnRatio) / 100);
      break;

    case 'S': // Controlled Stop (Release / Dead-Man)
    case 0:
    default:
      // Note: When STOP command is received, PWM speed is intentionally ignored!
      // Motors are commanded to absolute zero to prevent vehicle runaway.
      targetSpeedLeft  = 0;
      targetSpeedRight = 0;
      break;
  }
}

uint32_t lastByteTime    = 0;
char     lastLoggedCmd   = 0;
uint8_t  lastLoggedSpeed = 255;
uint32_t lastLogTime     = 0;

void logStm32Status(char cmd, uint8_t speed, uint32_t now) {
  bool changed = (cmd != lastLoggedCmd || speed != lastLoggedSpeed);
  bool tick    = (now - lastLogTime >= 500); // 2 Hz rate to keep CPU 100% free

  if (changed || tick) {
    lastLoggedCmd   = cmd;
    lastLoggedSpeed = speed;
    lastLogTime     = now;

    const char* name = "STOP";
    if (cmd == 'F' || cmd == 1) name = "FORWARD";
    else if (cmd == 'B' || cmd == 2) name = "BACKWARD";
    else if (cmd == 'Q' || cmd == 3) name = "PIVOT_LEFT";
    else if (cmd == 'E' || cmd == 4) name = "PIVOT_RIGHT";
    else if (cmd == 'L' || cmd == 5) name = "FRONT_LEFT";
    else if (cmd == 'R' || cmd == 6) name = "FRONT_RIGHT";
    else if (cmd == 'G' || cmd == 7) name = "BACK_LEFT";
    else if (cmd == 'I' || cmd == 8) name = "BACK_RIGHT";
    else if (cmd == 'T') name = "SET_TURN_RATIO";

    char buf[96];
    if (cmd == 'T') {
      snprintf(buf, sizeof(buf), "[STM32] RX CMD: 'T' (SET_TURN_RATIO) | RATIO: %3d%%", currentTurnRatio);
    } else {
      snprintf(buf, sizeof(buf), "[STM32] RX CMD: '%c' (%s) | SPD: %3d | TARGET L: %+4d R: %+4d",
               cmd, name, speed, targetSpeedLeft, targetSpeedRight);
    }

    Serial.println(buf);
    // CRITICAL: NEVER print to Serial1! PA9/PA10 is reserved exclusively for high-speed bridge control!
  }
}

void handleRxByte(uint8_t b, uint32_t now) {
  // If more than 20ms elapsed since last byte, reset FSM to prevent desync
  if (rxState != 0 && (now - lastByteTime > 20)) {
    rxState = 0;
  }
  lastByteTime = now;

  switch (rxState) {
    case 0: // Waiting for sync header 0xAA
      if (b == 0xAA) rxState = 1;
      break;
    case 1: // Command character ('F', 'B', 'L', 'R', 'Q', 'E', 'G', 'I', 'S' or 0-8)
      rxCmd = (char)b;
      rxState = 2;
      break;
    case 2: // Speed PWM (0 - 255)
      rxSpeed = b;
      rxState = 3;
      break;
    case 3: // Checksum = (cmd + speed) & 0xFF
      rxCsum = b;
      rxState = 4;
      break;
    case 4: // Frame footer 0x55
      if (b == 0x55 && rxCsum == ((uint8_t)(rxCmd + rxSpeed) & 0xFF)) {
        lastCommandTime = now;
        watchdogTripped = false;
        executeCommand(rxCmd, rxSpeed);
        // Toggle PC13 LED on each valid packet as visual link indicator
        digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
        logStm32Status(rxCmd, rxSpeed, now);
      }
      rxState = 0;
      break;
    default:
      rxState = 0;
      break;
  }
}

void loop() {
  uint32_t now = millis();

  // ---------------------------------------------------------------------------
  // 1. READ SERIAL PACKETS FROM RECEIVER MODULE (ESP32-S3 UART1 BRIDGE)
  // ---------------------------------------------------------------------------
  while (Serial.available()) {
    handleRxByte((uint8_t)Serial.read(), now);
  }

#if defined(Serial1)
  while (Serial1.available()) {
    handleRxByte((uint8_t)Serial1.read(), now);
  }
#endif

  // ---------------------------------------------------------------------------
  // 2. HARDWARE DEAD-MAN WATCHDOG FAILSAFE (350ms Timeout)
  // ---------------------------------------------------------------------------
  if (!watchdogTripped && (now - lastCommandTime >= WATCHDOG_TIMEOUT_MS)) {
    watchdogTripped = true;
    targetSpeedLeft  = 0;
    targetSpeedRight = 0;
    actualSpeedLeft  = 0.0f;
    actualSpeedRight = 0.0f;
    writeHardwarePWM(0, 0); // Immediately latch 0.00V Ground lock
    Serial.println(F("⚠️ [STM32 WATCHDOG] Teleop link lost! Emergency 0.00V Stop engaged."));
  }

  // ---------------------------------------------------------------------------
  // 3. EXECUTE 100 Hz SLEW-RATE RAMPING ENGINE
  // ---------------------------------------------------------------------------
  updateRampingEngine(now);

  // ---------------------------------------------------------------------------
  // 4. PERIODIC CURRENT TELEMETRY (To USB Serial Only)
  // ---------------------------------------------------------------------------
  if (now - lastTelemTime >= 2000) {
    lastTelemTime = now;

    float il_fwd = readAmps(PIN_LEFT_R_IS);
    float il_rev = readAmps(PIN_LEFT_L_IS);
    float ir_fwd = readAmps(PIN_RIGHT_R_IS);
    float ir_rev = readAmps(PIN_RIGHT_L_IS);

    char tBuf[48];
    snprintf(tBuf, sizeof(tBuf), "$TEL,%.2f,%.2f,%.2f,%.2f*", il_fwd, il_rev, ir_fwd, ir_rev);
    Serial.println(tBuf);
  }
}
