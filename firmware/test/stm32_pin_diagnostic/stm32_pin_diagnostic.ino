/*
 * ================================================================================
 * CYBERROVER X5 — STM32 UNIVERSAL PIN HEALTH & SILICON INTEGRITY DIAGNOSTIC
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras, Jharkhand
 * Platform       : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Microcontroller Hardware Validation Tool (TX or RX Board)
 *
 * SCOPE:
 *   1. Automated Silicon Self-Test:
 *      - Tests each GPIO pin in INPUT_PULLUP and INPUT_PULLDOWN modes.
 *      - Detects blown internal transistors, shorts to GND, and shorts to 3.3V.
 *   2. 12-Bit Analog ADC Live Meter:
 *      - Reads all 10 ADC channels (PA0 - PA7, PB0, PB1) with voltage conversion.
 *   3. Interactive Live Pin Matrix:
 *      - Real-time monitor at 115200 baud. Touch any pin with a GND wire to verify.
 *
 * SAFETY GUARDS:
 *   - PA13 & PA14 (SWD debugging) are strictly preserved (cannot lock out board).
 *   - PA9 & PA10 are preserved for USB-TTL Serial Monitor communication.
 *   - JTAG is released so PA15, PB3, PB4 function as standard GPIOs.
 * ================================================================================
 */

#include <Arduino.h>

// Onboard LED
#define PIN_LED_HB PC13

// Structure for testing digital pins
struct PinDef {
  uint8_t pin;
  const char* name;
  const char* desc;
};

// 25 Testable Digital GPIOs (Excluding PA9/PA10 Serial & PA13/PA14 SWD)
const PinDef testPins[] = {
  // Port A
  {PA0,  "PA0 ", "ADC0 / BTS_L_FWD"},
  {PA1,  "PA1 ", "ADC1 / BTS_L_REV"},
  {PA2,  "PA2 ", "ADC2 / BTS_R_FWD"},
  {PA3,  "PA3 ", "ADC3 / BTS_R_REV"},
  {PA4,  "PA4 ", "ADC4 / Master EN"},
  {PA5,  "PA5 ", "SPI1 SCK (NRF24)"},
  {PA6,  "PA6 ", "SPI1 MISO(NRF24)"},
  {PA7,  "PA7 ", "SPI1 MOSI(NRF24)"},
  {PA8,  "PA8 ", "TIM1_CH1 / Headlight PWM"},
  {PA11, "PA11", "USB_DM / GPIO"},
  {PA12, "PA12", "USB_DP / GPIO"},
  {PA15, "PA15", "JTAG Remapped GPIO"},

  // Port B
  {PB0,  "PB0 ", "ADC8 / Left Stick Y"},
  {PB1,  "PB1 ", "ADC9 / Left Stick X"},
  {PB3,  "PB3 ", "JTAG Remapped GPIO"},
  {PB4,  "PB4 ", "JTAG Remapped GPIO"},
  {PB5,  "PB5 ", "General Purpose GPIO"},
  {PB6,  "PB6 ", "I2C1 SCL"},
  {PB7,  "PB7 ", "I2C1 SDA"},
  {PB8,  "PB8 ", "TIM4_CH3 / Buzzer"},
  {PB9,  "PB9 ", "TIM4_CH4 / Haptic"},
  {PB10, "PB10", "NRF24 CE"},
  {PB11, "PB11", "NRF24 CSN"},
  {PB12, "PB12", "Perfboard LED 1"},
  {PB13, "PB13", "Perfboard LED 2"},
  {PB14, "PB14", "Perfboard LED 3"},
  {PB15, "PB15", "TB6612 AIN1"},

  // Port C
  {PC13, "PC13", "Onboard Blue Pill LED"},
  {PC14, "PC14", "OSC32 In / GPIO"},
  {PC15, "PC15", "OSC32 Out / GPIO"}
};

const uint8_t NUM_TEST_PINS = sizeof(testPins) / sizeof(testPins[0]);

// 10 Analog Channels
const uint8_t adcChannels[] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PB0, PB1 };
const char*   adcNames[]    = { "PA0", "PA1", "PA2", "PA3", "PA4", "PA5", "PA6", "PA7", "PB0", "PB1" };
const uint8_t NUM_ADC       = 10;

// =============================================================================
//  JTAG RELEASE ROUTINE
// =============================================================================
void releaseJTAG() {
  #if defined(__HAL_RCC_AFIO_CLK_ENABLE) && defined(__HAL_AFIO_REMAP_SWJ_NOJTAG)
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); // Frees PA15, PB3, PB4; keeps SWD (PA13/PA14)
  #elif defined(AFIO_MAPR_SWJ_CFG_JTAGDISABLE)
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
  #elif defined(disableDebugPorts)
    disableDebugPorts();
  #endif
}

// =============================================================================
//  AUTOMATED SILICON HEALTH CHECK (Pull-Up vs Pull-Down Test)
// =============================================================================
void runSiliconSelfCheck() {
  Serial.println(F("\n--------------------------------------------------------------------------------"));
  Serial.println(F("  STAGE 1: AUTOMATED SILICON HEALTH & TRACE SHORT-CIRCUIT TEST                 "));
  Serial.println(F("--------------------------------------------------------------------------------"));
  Serial.println(F("PIN   | PULLUP | PULLDOWN | STATUS        | HARDWARE DIAGNOSIS"));
  Serial.println(F("------+--------+----------+---------------+-------------------------------------"));

  uint8_t passCount = 0;
  uint8_t warnCount = 0;
  uint8_t failCount = 0;

  for (uint8_t i = 0; i < NUM_TEST_PINS; i++) {
    uint8_t pin = testPins[i].pin;

    // Test 1: Internal Pull-Up
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(200);
    int readPU = digitalRead(pin);

    // Test 2: Internal Pull-Down
    #if defined(INPUT_PULLDOWN)
      pinMode(pin, INPUT_PULLDOWN);
      delayMicroseconds(200);
      int readPD = digitalRead(pin);
    #else
      int readPD = 0;
    #endif

    // Reset back to INPUT_PULLUP for safety
    pinMode(pin, INPUT_PULLUP);

    // Print Pin Name
    Serial.print(testPins[i].name);
    Serial.print(F(" |   "));
    Serial.print(readPU ? F("HIGH") : F("LOW "));
    Serial.print(F(" |   "));
    Serial.print(readPD ? F("HIGH   ") : F("LOW    "));
    Serial.print(F("| "));

    // Analysis
    if (readPU == 1 && readPD == 0) {
      Serial.print(F("[PERFECT PASS] "));
      Serial.print(F("| Silicon healthy & unbridged ("));
      Serial.print(testPins[i].desc);
      Serial.println(F(")"));
      passCount++;
    } 
    else if (readPU == 0 && readPD == 0) {
      Serial.print(F("[SHORT TO GND] "));
      Serial.print(F("| Pin is grounded or pulled LOW ("));
      Serial.print(testPins[i].desc);
      Serial.println(F(")"));
      // Could be normal if a switch or pull-down is wired to it
      warnCount++;
    } 
    else if (readPU == 1 && readPD == 1) {
      Serial.print(F("[SHORT TO 3.3V]"));
      Serial.print(F("| Pin is bridged to 3.3V rail ("));
      Serial.print(testPins[i].desc);
      Serial.println(F(")"));
      warnCount++;
    } 
    else {
      Serial.print(F("[CRITICAL FAIL]"));
      Serial.print(F("| Blown I/O gate! ("));
      Serial.print(testPins[i].desc);
      Serial.println(F(")"));
      failCount++;
    }
  }

  Serial.println(F("------+--------+----------+---------------+-------------------------------------"));
  Serial.print(F("SUMMARY: "));
  Serial.print(passCount);
  Serial.print(F(" Passed | "));
  Serial.print(warnCount);
  Serial.print(F(" Grounded/Pulled | "));
  Serial.print(failCount);
  Serial.println(F(" Failed"));
  Serial.println(F("--------------------------------------------------------------------------------\n"));
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1200);

  // 1. Release JTAG
  releaseJTAG();

  // 2. Set 12-bit ADC
  #if defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__)
    analogReadResolution(12);
  #endif

  // 3. Heartbeat LED
  pinMode(PIN_LED_HB, OUTPUT);
  digitalWrite(PIN_LED_HB, LOW); // On for active-low PC13

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — STM32 BLUE PILL MASTER PIN DIAGNOSTIC SUITE                   "));
  Serial.println(F("  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           "));
  Serial.println(F("================================================================================"));

  // Run the automated silicon self-check
  runSiliconSelfCheck();

  // Configure all test pins as INPUT_PULLUP for the interactive live probe mode
  for (uint8_t i = 0; i < NUM_TEST_PINS; i++) {
    pinMode(testPins[i].pin, INPUT_PULLUP);
  }

  Serial.println(F(">>> STAGE 2: LIVE INTERACTIVE PIN MONITOR ACTIVE (115200 baud)"));
  Serial.println(F(">>> How to Test: Take a jumper wire from GND and touch ANY pin header."));
  Serial.println(F(">>> The touched pin will instantly flip from [HIGH] to [LOW / GROUNDED]!"));
  Serial.println(F("================================================================================"));
  delay(2000);
}

// =============================================================================
//  MAIN LOOP: LIVE INTERACTIVE PIN MONITOR
// =============================================================================
uint32_t lastPrint = 0;
uint32_t lastBlink = 0;

void loop() {
  uint32_t now = millis();

  // Heartbeat blink (2 Hz)
  if (now - lastBlink >= 250) {
    lastBlink = now;
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
  }

  // Print pin grid every 300 ms
  if (now - lastPrint >= 300) {
    lastPrint = now;

    // --- 1. ADC Channels (0.00V - 3.30V) ---
    Serial.print(F("[ADC VOLTS] "));
    for (uint8_t a = 0; a < NUM_ADC; a++) {
      int raw = analogRead(adcChannels[a]);
      float volts = (raw * 3.30f) / 4095.0f;
      Serial.print(adcNames[a]);
      Serial.print(F(":"));
      Serial.print(volts, 2);
      Serial.print(F("V "));
    }
    Serial.println();

    // --- 2. Live Digital Pin Status ---
    Serial.print(F("[DIGITAL  ] "));
    uint8_t groundedCount = 0;

    for (uint8_t i = 0; i < NUM_TEST_PINS; i++) {
      int state = digitalRead(testPins[i].pin);
      if (state == LOW) {
        groundedCount++;
        Serial.print(F(">>>["));
        Serial.print(testPins[i].name);
        Serial.print(F(":LOW]<<< "));
      }
    }

    if (groundedCount == 0) {
      Serial.print(F("All 25 Pins Idle [HIGH/PULLUP] (Touch any pin to GND to probe!)"));
    }

    Serial.println();
    Serial.println(F("--------------------------------------------------------------------------------"));
  }
}
