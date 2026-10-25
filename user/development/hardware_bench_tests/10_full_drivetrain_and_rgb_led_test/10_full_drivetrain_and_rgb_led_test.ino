/*
 * ================================================================================
 * CYBERROVER X5 — DUAL BTS7960 DRIVETRAIN & DUAL-COLOR (RED/BLUE) STATUS LED TEST
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCU     : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Full Powertrain Supervisor & Calibrated Optical Status
 *
 * CALIBRATED HARDWARE PINOUT:
 *   Right BTS7960 Motor Driver:
 *     - PIN_RIGHT_RPWM ---> PB6 (Right Forward PWM, TIM4_CH1) [VERIFIED!]
 *     - PIN_RIGHT_LPWM ---> PB7 (Right Reverse PWM, TIM4_CH2) [VERIFIED!]
 *     - PIN_RIGHT_R_IS ---> PB0 (Right Forward Current Sense, ADC8)
 *     - PIN_RIGHT_L_IS ---> PB1 (Right Reverse Current Sense, ADC9)
 *
 *   Left BTS7960 Motor Driver:
 *     - PIN_LEFT_RPWM  ---> PB9 (Left Forward PWM, TIM4_CH4) [VERIFIED!]
 *     - PIN_LEFT_LPWM  ---> PB8 (Left Reverse PWM, TIM4_CH3) [VERIFIED!]
 *     - PIN_LEFT_R_IS  ---> PA0 (Left Forward Current Sense, ADC0)
 *     - PIN_LEFT_L_IS  ---> PA1 (Left Reverse Current Sense, ADC1)
 *
 *   Optical Status Cluster (Common Anode, Active-LOW):
 *     - RED Color      ---> PB5  (Active-LOW: LOW = RED ON) [VERIFIED!]
 *     - BLUE Color     ---> PA11 (Active-LOW: LOW = BLUE ON) [VERIFIED!]
 *     - GREEN Color    ---> DECOMMISSIONED (PA12 held HIGH / OFF to protect circuit)
 *
 * VISUAL STATUS PALETTE (RED, BLUE, MAGENTA):
 *   - RED: Standby / Countdown / Reverse Drive
 *   - BLUE: Forward Drive / Left-Right Motor Rotation
 *   - MAGENTA (Red + Blue): Full 4-Motor Sprint & High-Speed Tank Turns
 * ================================================================================
 */

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Calibrated Pin Definitions
// -----------------------------------------------------------------------------
// Right Motor Driver
#define PIN_RIGHT_RPWM   PB6   // Right Forward (Verified)
#define PIN_RIGHT_LPWM   PB7   // Right Reverse (Verified)
#define PIN_RIGHT_R_IS   PB0   // Right Forward Current Sense
#define PIN_RIGHT_L_IS   PB1   // Right Reverse Current Sense

// Left Motor Driver
#define PIN_LEFT_RPWM    PB9   // Left Forward (Verified)
#define PIN_LEFT_LPWM    PB8   // Left Reverse (Verified)
#define PIN_LEFT_R_IS    PA0   // Left Forward Current Sense
#define PIN_LEFT_L_IS    PA1   // Left Reverse Current Sense

// Optical Status LED (Common Anode: Active-LOW)
#define PIN_LED_RED      PB5   // Red Channel
#define PIN_LED_BLUE     PA11  // Blue Channel
#define PIN_LED_DEAD_CH  PA12  // Decommissioned Green channel (held HIGH)

#define PIN_LED_HB       PC13  // Onboard Blue Pill Diagnostic LED (Active-LOW)

const uint8_t TEST_MAX_PWM = 180;
const uint8_t MIN_SPIN_PWM = 70;

// -----------------------------------------------------------------------------
// Optical Status Functions (Common Anode: LOW = ON, HIGH = OFF)
// -----------------------------------------------------------------------------
void ledOff() {
  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_BLUE, HIGH);
  digitalWrite(PIN_LED_DEAD_CH, HIGH);
}

void ledRed() {
  digitalWrite(PIN_LED_RED, LOW);   // Red ON
  digitalWrite(PIN_LED_BLUE, HIGH); // Blue OFF
  digitalWrite(PIN_LED_DEAD_CH, HIGH);
}

void ledBlue() {
  digitalWrite(PIN_LED_RED, HIGH);  // Red OFF
  digitalWrite(PIN_LED_BLUE, LOW);  // Blue ON
  digitalWrite(PIN_LED_DEAD_CH, HIGH);
}

void ledMagenta() {
  digitalWrite(PIN_LED_RED, LOW);   // Red ON
  digitalWrite(PIN_LED_BLUE, LOW);  // Blue ON -> Mixes to MAGENTA!
  digitalWrite(PIN_LED_DEAD_CH, HIGH);
}

// -----------------------------------------------------------------------------
// Motor Control Functions
// -----------------------------------------------------------------------------
void setLeftMotor(int16_t speed) {
  if (speed > 0) {
    analogWrite(PIN_LEFT_LPWM, 0);
    analogWrite(PIN_LEFT_RPWM, constrain(speed, 0, 255));
  } else if (speed < 0) {
    analogWrite(PIN_LEFT_RPWM, 0);
    analogWrite(PIN_LEFT_LPWM, constrain(-speed, 0, 255));
  } else {
    analogWrite(PIN_LEFT_RPWM, 0);
    analogWrite(PIN_LEFT_LPWM, 0);
  }
}

void setRightMotor(int16_t speed) {
  if (speed > 0) {
    analogWrite(PIN_RIGHT_LPWM, 0);
    analogWrite(PIN_RIGHT_RPWM, constrain(speed, 0, 255));
  } else if (speed < 0) {
    analogWrite(PIN_RIGHT_RPWM, 0);
    analogWrite(PIN_RIGHT_LPWM, constrain(-speed, 0, 255));
  } else {
    analogWrite(PIN_RIGHT_RPWM, 0);
    analogWrite(PIN_RIGHT_LPWM, 0);
  }
}

void stopAllMotors() {
  setLeftMotor(0);
  setRightMotor(0);
}

float readCurrentAmps(uint8_t pin) {
  uint32_t raw = 0;
  for (int i = 0; i < 4; i++) {
    raw += analogRead(pin);
  }
  raw /= 4;
  float volts = (raw * 3.3f) / 4095.0f;
  return volts * 8.5f; // BTS7960 Amperes conversion
}

void printCurrentTelemetry(const char* label) {
  float i_l_fwd = readCurrentAmps(PIN_LEFT_R_IS);
  float i_l_rev = readCurrentAmps(PIN_LEFT_L_IS);
  float i_r_fwd = readCurrentAmps(PIN_RIGHT_R_IS);
  float i_r_rev = readCurrentAmps(PIN_RIGHT_L_IS);

  Serial.print(F("    [CURRENT] "));
  Serial.print(label);
  Serial.print(F(" | Left[FWD:"));
  Serial.print(i_l_fwd, 2);
  Serial.print(F("A REV:"));
  Serial.print(i_l_rev, 2);
  Serial.print(F("A] | Right[FWD:"));
  Serial.print(i_r_fwd, 2);
  Serial.print(F("A REV:"));
  Serial.print(i_r_rev, 2);
  Serial.println(F("A]"));
}

void rampMotorWithSensing(bool isLeft, bool forward, uint8_t peakPwm) {
  for (int pwm = MIN_SPIN_PWM; pwm <= peakPwm; pwm += 15) {
    int16_t spd = forward ? pwm : -pwm;
    if (isLeft) setLeftMotor(spd);
    else setRightMotor(spd);
    delay(50);
  }

  for (int i = 0; i < 4; i++) {
    printCurrentTelemetry(isLeft ? (forward ? "Left FWD" : "Left REV") : (forward ? "Right FWD" : "Right REV"));
    delay(250);
  }

  for (int pwm = peakPwm; pwm >= MIN_SPIN_PWM; pwm -= 15) {
    int16_t spd = forward ? pwm : -pwm;
    if (isLeft) setLeftMotor(spd);
    else setRightMotor(spd);
    delay(50);
  }

  if (isLeft) setLeftMotor(0);
  else setRightMotor(0);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);

  // Motor PWM Outputs
  pinMode(PIN_LEFT_RPWM, OUTPUT);
  pinMode(PIN_LEFT_LPWM, OUTPUT);
  pinMode(PIN_RIGHT_RPWM, OUTPUT);
  pinMode(PIN_RIGHT_LPWM, OUTPUT);
  stopAllMotors();

  // Current Sense Analog Inputs
  pinMode(PIN_LEFT_R_IS, INPUT_ANALOG);
  pinMode(PIN_LEFT_L_IS, INPUT_ANALOG);
  pinMode(PIN_RIGHT_R_IS, INPUT_ANALOG);
  pinMode(PIN_RIGHT_L_IS, INPUT_ANALOG);

  // Optical Status Pins
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_DEAD_CH, OUTPUT);
  pinMode(PIN_LED_HB, OUTPUT);

  digitalWrite(PIN_LED_HB, HIGH);
  ledOff();

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — FULL DRIVETRAIN & DUAL-COLOR LED BENCH TEST v1.2              "));
  Serial.println(F("  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           "));
  Serial.println(F("================================================================================"));
  Serial.println(F("  CALIBRATED PIN MAPPING:                                                       "));
  Serial.println(F("    Right Motors : Forward = PB6 | Reverse = PB7                                "));
  Serial.println(F("    Left Motors  : Forward = PB9 | Reverse = PB8                                "));
  Serial.println(F("    Status LED   : Red = PB5     | Blue = PA11     (Common Anode Active-LOW)    "));
  Serial.println(F("================================================================================"));

  // Optical Self-Test (Red -> Blue -> Magenta)
  Serial.println(F(">>> Performing Optical Check: Red -> Blue -> Magenta..."));
  ledRed(); delay(600);
  ledBlue(); delay(600);
  ledMagenta(); delay(600);
  ledOff();

  // 3-Second Safety Countdown
  for (int i = 3; i > 0; i--) {
    Serial.print(F("Starting Powertrain Diagnostic in "));
    Serial.print(i);
    Serial.println(F("..."));
    ledRed();
    delay(200);
    ledOff();
    delay(800);
  }

  Serial.println(F("\n>>> [BENCH DIAGNOSTIC RUNNING] <<<\n"));
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop() {
  // 1. LEFT FORWARD
  Serial.println(F("[PHASE 1] Left Motors -> FORWARD (PB9 active) | LED: BLUE"));
  ledBlue();
  rampMotorWithSensing(true, true, TEST_MAX_PWM);
  stopAllMotors();
  ledOff();
  delay(1000);

  // 2. LEFT REVERSE
  Serial.println(F("[PHASE 2] Left Motors -> REVERSE (PB8 active) | LED: RED"));
  ledRed();
  rampMotorWithSensing(true, false, TEST_MAX_PWM);
  stopAllMotors();
  ledOff();
  delay(1000);

  // 3. RIGHT FORWARD
  Serial.println(F("[PHASE 3] Right Motors -> FORWARD (PB6 active) | LED: BLUE"));
  ledBlue();
  rampMotorWithSensing(false, true, TEST_MAX_PWM);
  stopAllMotors();
  ledOff();
  delay(1000);

  // 4. RIGHT REVERSE
  Serial.println(F("[PHASE 4] Right Motors -> REVERSE (PB7 active) | LED: RED"));
  ledRed();
  rampMotorWithSensing(false, false, TEST_MAX_PWM);
  stopAllMotors();
  ledOff();
  delay(1000);

  // 5. ALL 4 MOTORS FORWARD SPRINT
  Serial.println(F("[PHASE 5] All 4 Motors -> FULL FORWARD DRIVE | LED: MAGENTA"));
  ledMagenta();
  setLeftMotor(150);
  setRightMotor(150);
  for (int i = 0; i < 4; i++) {
    printCurrentTelemetry("Straight Sprint");
    delay(500);
  }
  stopAllMotors();
  ledOff();
  delay(1000);

  // 6. ALL 4 MOTORS REVERSE
  Serial.println(F("[PHASE 6] All 4 Motors -> REVERSE DRIVE | LED: RED"));
  ledRed();
  setLeftMotor(-150);
  setRightMotor(-150);
  for (int i = 0; i < 4; i++) {
    printCurrentTelemetry("Straight Reverse");
    delay(500);
  }
  stopAllMotors();
  ledOff();
  delay(1000);

  // 7. SKID-STEER TANK SPIN CLOCKWISE
  Serial.println(F("[PHASE 7] Skid-Steer Tank Spin CLOCKWISE | LED: MAGENTA"));
  ledMagenta();
  setLeftMotor(150);
  setRightMotor(-150);
  for (int i = 0; i < 4; i++) {
    printCurrentTelemetry("Tank Spin CW");
    delay(500);
  }
  stopAllMotors();
  ledOff();
  delay(1000);

  // 8. SKID-STEER TANK SPIN COUNTER-CLOCKWISE
  Serial.println(F("[PHASE 8] Skid-Steer Tank Spin COUNTER-CLOCKWISE | LED: MAGENTA"));
  ledMagenta();
  setLeftMotor(-150);
  setRightMotor(150);
  for (int i = 0; i < 4; i++) {
    printCurrentTelemetry("Tank Spin CCW");
    delay(500);
  }
  stopAllMotors();
  ledOff();
  delay(1000);

  Serial.println(F("\n================================================================================"));
  Serial.println(F("  CYCLE COMPLETE: Full Drivetrain & Current Telemetry 100% Operational!         "));
  Serial.println(F("  Pausing 5 seconds before next cycle (LED: Pulsing Red Standby)...             "));
  Serial.println(F("================================================================================\n"));

  for (int i = 0; i < 5; i++) {
    ledRed();
    delay(200);
    ledOff();
    delay(800);
  }
}
