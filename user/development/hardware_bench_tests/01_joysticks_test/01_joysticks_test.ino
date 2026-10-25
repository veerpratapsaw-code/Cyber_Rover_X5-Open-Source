/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 1: DUAL JOYSTICKS ISOLATION TEST (12-BIT & AXIS INVERT)
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 *
 * Gimbal Orientation: Tilted 90 degrees on physical carrier.
 * Pin Assignments:
 *   - Left Stick X  : PB1 (ADC9)
 *   - Left Stick Y  : PB0 (ADC8)
 *   - Right Stick X : PA1 (ADC1)  [Inverted in software: 4095 - raw]
 *   - Right Stick Y : PA0 (ADC0)
 *
 * ADC Resolution: Set to 12-bit (0 - 4095) via analogReadResolution(12).
 * Output Format : Clean CSV stream for Serial Monitor (115200) & Serial Plotter.
 * ===============================================================================
 */

#define PIN_RX PA4
#define PIN_RY PA3
#define PIN_LX PB1
#define PIN_LY PB0

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure STM32 ADC to native 12-bit resolution (0 - 4095)
  #if defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__)
    analogReadResolution(12);
  #endif

  Serial.println(F("======================================================="));
  Serial.println(F("  CYBERROVER X5 — STAGE 1: JOYSTICKS 12-BIT TEST       "));
  Serial.println(F("  Resolution: 12-bit (0 - 4095) | Right X: Inverted    "));
  Serial.println(F("======================================================="));
}

void loop() {
  // Read raw 12-bit values (0 - 4095)
  int raw_lx = analogRead(PIN_LX);
  int raw_ly = analogRead(PIN_LY);
  int raw_rx = analogRead(PIN_RX);
  int raw_ry = analogRead(PIN_RY);

  // Invert Right Stick X so Right = 4095 and Left = 0
  int rx = 4095 - raw_rx;
  int ry = raw_ry;
  int lx = raw_lx;
  int ly = raw_ly;

  // Clean format for Arduino Serial Monitor and Serial Plotter (Ctrl+Shift+L)
  Serial.print(F("LX:"));
  Serial.print(lx);
  Serial.print(F(" LY:"));
  Serial.print(ly);
  Serial.print(F(" RX:"));
  Serial.print(rx);
  Serial.print(F(" RY:"));
  Serial.println(ry);

  delay(50); // 20 Hz update rate for smooth plotting
}
