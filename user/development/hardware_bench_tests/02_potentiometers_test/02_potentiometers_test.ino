/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 2: 3x ROTARY POTENTIOMETERS ISOLATION & SWEEP TEST
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 *
 * Analog Channels (12-bit Native: 0 - 4095):
 *   - PA2 (Pin 7) ---> Pot Left   (Speed Governor: Crawl 10% to Sprint 100%)
 *   - PA3 (Pin 8) ---> Pot Center (FPV Camera Tilt: 0° Down to 60° Up)
 *   - PA4 (Pin 9) ---> Pot Right  (Steering Sensitivity / Rate Limiter)
 *
 * Test Objectives:
 *   1. Full range sweep verification (Minimum should approach ~0-50, Maximum ~4050-4095).
 *   2. Wiper health & contact stability (Zero jitter/spikes when resting).
 *   3. Rotation direction verification (CW vs CCW).
 *
 * Output Format:
 *   Clean numeric labels for Arduino Serial Plotter (Ctrl+Shift+L) & Serial Monitor.
 * ===============================================================================
 */

#define PIN_POT_LEFT   PA2
#define PIN_POT_CENTER PA3
#define PIN_POT_RIGHT  PA4

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Set STM32 ADC to native 12-bit resolution (0 - 4095)
  #if defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__)
    analogReadResolution(12);
  #endif

  Serial.println(F("======================================================="));
  Serial.println(F("  CYBERROVER X5 — STAGE 2: 3x POTENTIOMETERS TEST      "));
  Serial.println(F("  PA2: Pot Left | PA3: Pot Center | PA4: Pot Right     "));
  Serial.println(F("  Rotate each knob fully CCW -> CW to verify 0 to 4095 "));
  Serial.println(F("======================================================="));
}

void loop() {
  // Read raw 12-bit ADC values (0 - 4095)
  int potL = analogRead(PIN_POT_LEFT);
  int potC = analogRead(PIN_POT_CENTER);
  int potR = analogRead(PIN_POT_RIGHT);

  // Calculate percentage (0 - 100%)
  int pctL = map(potL, 0, 4095, 0, 100);
  int pctC = map(potC, 0, 4095, 0, 100);
  int pctR = map(potR, 0, 4095, 0, 100);

  // Format compatible with BOTH Serial Monitor and Serial Plotter:
  // "PL:val PC:val PR:val"
  Serial.print(F("PL:"));
  Serial.print(potL);
  Serial.print(F(" PC:"));
  Serial.print(potC);
  Serial.print(F(" PR:"));
  Serial.print(potR);

  // Optional: print percentage at end of line
  Serial.print(F(" | L:"));
  Serial.print(pctL);
  Serial.print(F("% C:"));
  Serial.print(pctC);
  Serial.print(F("% R:"));
  Serial.print(pctR);
  Serial.println(F("%"));

  delay(60); // ~16 Hz smooth update rate
}
