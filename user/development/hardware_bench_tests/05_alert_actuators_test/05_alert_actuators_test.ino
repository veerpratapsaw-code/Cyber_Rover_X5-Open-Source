/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 5: ALERT ACTUATORS ISOLATION TEST (BUZZER & HAPTIC)
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 *
 * Actuator Pins (Right Header, Pins 4-5):
 *   - PB9 ---> 5V Active Piezo Buzzer (Audio Alert)
 *   - PB8 ---> Haptic Vibration Motor (Tactile Alert via NPN Transistor / MOSFET)
 *
 * Operational Patterns Tested:
 *   1. Boot Chime (Double chirp: 50ms pulse)
 *   2. Haptic Feedback Pulse (Dual 80ms rumble)
 *   3. Critical Alarm Siren (Rapid 3-pulse warning)
 *
 * NOTE: If physical buzzer/motor are not yet soldered, an LED with 220Ω resistor
 *       or multimeter between PB8/PB9 and GND will verify logic high (3.3V) switching.
 * ===============================================================================
 */

#include <Arduino.h>

#define PIN_BUZZER PB8
#define PIN_HAPTIC PB9
#define PIN_LED_HB PC13

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_HAPTIC, OUTPUT);
  pinMode(PIN_LED_HB, OUTPUT);

  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_HAPTIC, LOW);
  digitalWrite(PIN_LED_HB, HIGH); // Off for active-low PC13

  Serial.println(F("======================================================="));
  Serial.println(F("  CYBERROVER X5 — STAGE 5: BUZZER & HAPTIC TEST        "));
  Serial.println(F("  PB9: Piezo Buzzer | PB8: Haptic Vibration Motor      "));
  Serial.println(F("======================================================="));

  // Boot Chime
  Serial.println(F("[BOOT] Executing Boot Chime..."));
  digitalWrite(PIN_BUZZER, HIGH);
  digitalWrite(PIN_HAPTIC, HIGH);
  digitalWrite(PIN_LED_HB, LOW);
  delay(80);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_HAPTIC, LOW);
  digitalWrite(PIN_LED_HB, HIGH);
  delay(80);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(80);
  digitalWrite(PIN_BUZZER, LOW);

  Serial.println(F("Boot Chime complete! Entering sequential alert test cycle..."));
  Serial.println();
}

uint8_t cycle = 0;

void loop() {
  cycle++;
  Serial.print(F(">>> [CYCLE "));
  Serial.print(cycle);
  Serial.println(F("] ---"));

  // 1. Test Buzzer Single Chirp
  Serial.println(F("  [1/3] Buzzer Single Chirp (PB9 HIGH for 120ms)..."));
  digitalWrite(PIN_BUZZER, HIGH);
  digitalWrite(PIN_LED_HB, LOW);
  delay(120);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_HB, HIGH);
  delay(1500);

  // 2. Test Haptic Dual Rumble
  Serial.println(F("  [2/3] Haptic Vibration Dual Rumble (PB8 HIGH twice)..."));
  digitalWrite(PIN_HAPTIC, HIGH);
  delay(100);
  digitalWrite(PIN_HAPTIC, LOW);
  delay(100);
  digitalWrite(PIN_HAPTIC, HIGH);
  delay(100);
  digitalWrite(PIN_HAPTIC, LOW);
  delay(1500);

  // 3. Test Synchronized Critical Alarm (Siren + Rumble)
  Serial.println(F("  [3/3] Synchronized Emergency Alarm (PB8 + PB9 rapid burst)..."));
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    digitalWrite(PIN_HAPTIC, HIGH);
    digitalWrite(PIN_LED_HB, LOW);
    delay(70);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_HAPTIC, LOW);
    digitalWrite(PIN_LED_HB, HIGH);
    delay(70);
  }

  Serial.println(F("Cycle complete. Waiting 3 seconds before next cycle..."));
  Serial.println();
  delay(3000);
}
