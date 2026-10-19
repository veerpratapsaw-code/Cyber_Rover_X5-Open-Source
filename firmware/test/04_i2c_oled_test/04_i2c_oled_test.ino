/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 4: OLED GLASS ILLUMINATION & DUAL-DRIVER TEST
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 *
 * Symptoms:
 *   I2C ACK received at 0x3C, but physical glass is dark.
 *
 * Solutions Tested:
 *   1. Hardware Charge-Pump Boost: Maximum contrast & display ON commands.
 *   2. Dual-Engine Switch: Runs SH1106 mode for 5 seconds, then SSD1306 mode
 *      for 5 seconds, alternating with ALL PIXELS ON (Solid White).
 *   3. Power Check: Advises switching VCC from 3.3V to 5V (from MT3608) if LDO dropout
 *      is starving the internal charge pump.
 * ===============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SSD1306.h>

#define OLED_SCL_PIN  PB6
#define OLED_SDA_PIN  PB7

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

Adafruit_SH1106G displaySH = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SSD1306 displaySSD = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println(F("======================================================="));
  Serial.println(F("  CYBERROVER X5 — OLED GLASS ILLUMINATION DIAGNOSTIC   "));
  Serial.println(F("======================================================="));

  #if defined(ARDUINO_ARCH_STM32)
    Wire.setSCL(OLED_SCL_PIN);
    Wire.setSDA(OLED_SDA_PIN);
  #endif

  Wire.begin();
  Wire.setClock(100000); // 100 kHz standard stable I2C
  delay(100);

  Serial.println(F("[STEP 1] Testing Hardware Direct Charge-Pump ON Command..."));
  // Direct I2C commands to force internal charge pump ON for SSD1306 / SH1106
  Wire.beginTransmission(0x3C);
  Wire.write(0x00); // Command stream
  Wire.write(0x8D); // Charge pump command (SSD1306)
  Wire.write(0x14); // Enable charge pump (7.5V)
  Wire.write(0xAD); // DC-DC control (SH1106)
  Wire.write(0x8B); // Built-in DC-DC ON
  Wire.write(0xAF); // Display ON
  Wire.endTransmission();
  delay(50);

  Serial.println(F("[STEP 2] Initializing SH1106 and SSD1306 drivers..."));
  displaySH.begin(0x3C, true);
  displaySSD.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  Serial.println(F("Starting Alternating Test (5s SH1106 Full White <-> 5s SSD1306 Full White)..."));
  Serial.println(F("Watch the physical OLED screen now!"));
}

uint32_t lastSwitch = 0;
bool modeSH = true;
uint8_t count = 0;

void loop() {
  uint32_t now = millis();

  if (now - lastSwitch >= 3000) {
    lastSwitch = now;
    modeSH = !modeSH;
    count++;

    if (modeSH) {
      Serial.println(F(">>> [TEST MODE A] Running SH1106 Mode (Full Screen White + Text)..."));
      displaySH.clearDisplay();
      displaySH.fillScreen(SH110X_WHITE);
      displaySH.setTextColor(SH110X_BLACK, SH110X_WHITE);
      displaySH.setTextSize(1);
      displaySH.setCursor(14, 18);
      displaySH.print(F("SH1106 DRIVER"));
      displaySH.setCursor(14, 32);
      displaySH.print(F("CYBERROVER X5"));
      displaySH.setCursor(14, 46);
      displaySH.print(F("COUNT: "));
      displaySH.print(count);
      displaySH.display();
    } else {
      Serial.println(F(">>> [TEST MODE B] Running SSD1306 Mode (Full Screen White + Text)..."));
      displaySSD.clearDisplay();
      displaySSD.fillScreen(SSD1306_WHITE);
      displaySSD.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      displaySSD.setTextSize(1);
      displaySSD.setCursor(14, 18);
      displaySSD.print(F("SSD1306 DRIVER"));
      displaySSD.setCursor(14, 32);
      displaySSD.print(F("CYBERROVER X5"));
      displaySSD.setCursor(14, 46);
      displaySSD.print(F("COUNT: "));
      displaySSD.print(count);
      displaySSD.display();
    }
  }

  delay(50);
}
