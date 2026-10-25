/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 3: DIGITAL BUTTONS & SWITCHES BENCH TEST (CALIBRATED)
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 *
 * VALIDATED DIGITAL CONTROLS (5 Active Channels):
 *   1. PB4  ---> Toggle Switch Left   [NO Contact: Active-LOW  (LOW = ON,  HIGH = OFF)]
 *   2. PB5  ---> Toggle Switch Right  [NO Contact: Active-LOW  (LOW = ON,  HIGH = OFF)]
 *   3. PA15 ---> Push Button Left     [NC Contact: Active-HIGH (HIGH = PRESSED, LOW = UP)]
 *   4. PB3  ---> Push Button Right    [NC Contact: Active-HIGH (HIGH = PRESSED, LOW = UP)]
 *   5. PA11 ---> Left Joystick Click  [NO Contact: Active-LOW  (LOW = PRESSED, HIGH = UP)]
 *
 * HARDWARE DECOMMISSION NOTICE:
 *   - PA12 (Right Joystick Click) is DECOMMISSIONED (Hardware internal switch faulty).
 * ===============================================================================
 */

#include <Arduino.h>

// 5 Active Digital Pins
#define PIN_TOGGLE_LEFT   PB4
#define PIN_TOGGLE_RIGHT  PB5
#define PIN_PUSH_LEFT     PA15
#define PIN_PUSH_RIGHT    PB3
#define PIN_LEFT_JS_BTN   PA11

// Control descriptor with custom polarity handling
struct DigitalControl {
  uint8_t pin;
  const char* name;
  const char* tag;
  bool activeLevel; // LOW (false) for standard NO; HIGH (true) for NC push buttons
  bool lastState;   // true = ACTIVE/PRESSED, false = IDLE/RELEASED
};

DigitalControl controls[] = {
  {PIN_TOGGLE_LEFT,  "Toggle Left  (PB4) ", "TL", LOW,  false},
  {PIN_TOGGLE_RIGHT, "Toggle Right (PB5) ", "TR", LOW,  false},
  {PIN_PUSH_LEFT,    "Push Left    (PA15)", "PL", HIGH, false}, // NC Inverted
  {PIN_PUSH_RIGHT,   "Push Right   (PB3) ", "PR", HIGH, false}, // NC Inverted
  {PIN_LEFT_JS_BTN,  "Stick Click L(PA11)", "CL", LOW,  false}
};

const uint8_t NUM_CONTROLS = sizeof(controls) / sizeof(controls[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. SAFELY RELEASE JTAG PINS (PA15, PB3, PB4) FOR STANDARD GPIO
  #if defined(__HAL_RCC_AFIO_CLK_ENABLE) && defined(__HAL_AFIO_REMAP_SWJ_NOJTAG)
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); // Disables JTAG, keeps SWD (PA13/PA14) active
  #elif defined(AFIO_MAPR_SWJ_CFG_JTAGDISABLE)
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
  #elif defined(disableDebugPorts)
    disableDebugPorts();
  #endif

  // 2. CONFIGURE PINS
  // PB4, PB5, PA11 use INPUT_PULLUP (Active-LOW)
  // PA15, PB3 use INPUT_PULLDOWN (or PULLUP with NC to GND)
  for (uint8_t i = 0; i < NUM_CONTROLS; i++) {
    pinMode(controls[i].pin, INPUT_PULLUP);
    
    // Read initial calibrated state
    bool raw = digitalRead(controls[i].pin);
    controls[i].lastState = (raw == controls[i].activeLevel);
  }

  Serial.println(F("======================================================="));
  Serial.println(F("  CYBERROVER X5 — STAGE 3: BUTTONS & SWITCHES (CALIBRATED)"));
  Serial.println(F("  PA12 Right Stick Click DECOMMISSIONED (Hardware Fault)"));
  Serial.println(F("  PA15 & PB3 Inverted to match NC Push Button polarity  "));
  Serial.println(F("======================================================="));
  Serial.println(F("Ready! Listening for clicks and toggles..."));
  Serial.println();
}

uint32_t lastPrintTime = 0;

void loop() {
  uint32_t now = millis();

  // 1. INSTANT EVENT NOTIFICATIONS
  for (uint8_t i = 0; i < NUM_CONTROLS; i++) {
    bool raw = digitalRead(controls[i].pin);
    bool isPressed = (raw == controls[i].activeLevel);

    if (isPressed != controls[i].lastState) {
      controls[i].lastState = isPressed;

      Serial.print(F(">>> [EVENT] "));
      Serial.print(controls[i].name);
      Serial.print(F(" -> "));
      if (isPressed) {
        Serial.println(F(">>> [ ACTIVE / PRESSED ] <<<"));
      } else {
        Serial.println(F("    [ RELEASED / OFF   ]"));
      }
    }
  }

  // 2. PERIODIC SUMMARY LINE (Every 400ms)
  if (now - lastPrintTime >= 400) {
    lastPrintTime = now;

    Serial.print(F("[LIVE STATE] "));
    for (uint8_t i = 0; i < NUM_CONTROLS; i++) {
      bool raw = digitalRead(controls[i].pin);
      bool isPressed = (raw == controls[i].activeLevel);

      Serial.print(controls[i].tag);
      Serial.print(F(":"));
      Serial.print(isPressed ? F("[ON] ") : F("[--] "));
    }
    Serial.println();
  }

  delay(20);
}
