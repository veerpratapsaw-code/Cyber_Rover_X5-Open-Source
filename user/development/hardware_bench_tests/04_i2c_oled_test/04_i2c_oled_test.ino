/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 4: ADVANCED I2C SCANNER & OLED DIAGNOSTIC TOOL
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 *
 * Primary Purpose:
 *   1. Hardware Bus Line Test: Checks SCL (PB6) and SDA (PB7) voltage state.
 *   2. I2C Bus Unlocking: Toggles SCL 9 times to clear hung slave devices.
 *   3. Continuous I2C Address Scanner: Scans 0x01 to 0x7F to find all connected chips.
 *   4. Direct OLED Glass Test: If an OLED is detected at 0x3C or 0x3D, it immediately
 *      runs both hardware command tests and full-screen illumination cycles!
 *
 * Serial Monitor: 115200 baud.
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

Adafruit_SH1106G displaySH  = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SSD1306 displaySSD = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint8_t detected_oled_addr = 0;
bool oled_found = false;

// -------------------------------------------------------------------------------
// Helper: Release potentially locked I2C bus (9 clock pulses)
// -------------------------------------------------------------------------------
void unlockI2CBus() {
  pinMode(OLED_SCL_PIN, OUTPUT);
  pinMode(OLED_SDA_PIN, INPUT_PULLUP);

  // Send 9 clock pulses on SCL to force slave to release SDA
  for (uint8_t i = 0; i < 9; i++) {
    digitalWrite(OLED_SCL_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(OLED_SCL_PIN, LOW);
    delayMicroseconds(10);
  }

  // Generate STOP condition
  pinMode(OLED_SDA_PIN, OUTPUT);
  digitalWrite(OLED_SDA_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(OLED_SCL_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(OLED_SDA_PIN, HIGH);
  delayMicroseconds(10);

  pinMode(OLED_SCL_PIN, INPUT_PULLUP);
  pinMode(OLED_SDA_PIN, INPUT_PULLUP);
}

// -------------------------------------------------------------------------------
// I2C Bus Scanner
// -------------------------------------------------------------------------------
void scanI2CBus() {
  Serial.println(F("\n-------------------------------------------------------"));
  Serial.println(F(">>> SCANNING I2C BUS (PB6 = SCL, PB7 = SDA)..."));
  Serial.println(F("-------------------------------------------------------"));

  uint8_t deviceCount = 0;
  detected_oled_addr = 0;
  oled_found = false;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.print(F(" [✓ FOUND] Device detected at 7-bit Address: 0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.print(address, HEX);

      if (address == 0x3C) {
        Serial.println(F("  <--- OLED DISPLAY (Primary: 1.3\" SH1106 or 0.96\" SSD1306)"));
        detected_oled_addr = address;
        oled_found = true;
      } else if (address == 0x3D) {
        Serial.println(F("  <--- OLED DISPLAY (Secondary: Alternate Address)"));
        detected_oled_addr = address;
        oled_found = true;
      } else if (address == 0x68) {
        Serial.println(F("  <--- MPU6050 Gyro / Accelerometer"));
      } else if (address == 0x76 || address == 0x77) {
        Serial.println(F("  <--- BMP280 Barometer"));
      } else {
        Serial.println(F("  <--- Unknown / Auxiliary I2C Module"));
      }
      deviceCount++;
    } else if (error == 4) {
      Serial.print(F(" [!] Unknown error at address 0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.println(address, HEX);
    }
  }

  if (deviceCount == 0) {
    Serial.println(F("\n[✕ ERROR] ZERO I2C DEVICES RESPONDED ON PB6 / PB7!"));
    Serial.println(F("DIAGNOSTIC TROUBLESHOOTING CHECKLIST:"));
    Serial.println(F("  1. REVERSE SCL & SDA: Are PB6 and PB7 swapped? Try swapping them."));
    Serial.println(F("  2. POWER RAIL: Is OLED VCC connected to 3.3V or 5V?"));
    Serial.println(F("     * If powered from Blue Pill 3.3V pin, the AMS1117 LDO may be dropping out."));
    Serial.println(F("     * Connect OLED VCC to the 5V output of your MT3608 boost converter!"));
    Serial.println(F("  3. GROUND: Is OLED GND connected firmly to Blue Pill GND?"));
    Serial.println(F("  4. PULL-UPS: Some 1.3\" modules require 4.7k pull-up resistors on SCL & SDA."));
  } else {
    Serial.print(F("\nScan complete. Total devices found: "));
    Serial.println(deviceCount);
  }
}

// -------------------------------------------------------------------------------
// Direct OLED Initialization & Test Pattern
// -------------------------------------------------------------------------------
void testOLED(uint8_t addr) {
  Serial.print(F("\n>>> INITIATING OLED HARDWARE DIAGNOSTIC ON 0x"));
  Serial.println(addr, HEX);

  // 1. Send direct hardware commands to force internal charge pump ON
  Serial.println(F("[1/4] Sending Direct Charge-Pump Boost Commands..."));
  Wire.beginTransmission(addr);
  Wire.write(0x00); // Command stream
  Wire.write(0x8D); // SSD1306 charge pump
  Wire.write(0x14); // 7.5V enable
  Wire.write(0xAD); // SH1106 DC-DC control
  Wire.write(0x8B); // Built-in DC-DC ON
  Wire.write(0x81); // Set Contrast
  Wire.write(0xFF); // Maximum Contrast (255)
  Wire.write(0xAF); // Display ON
  Wire.endTransmission();
  delay(100);

  // 2. Try SH1106 Driver
  Serial.println(F("[2/4] Initializing SH1106 Driver (1.3\" OLED)..."));
  if (displaySH.begin(addr, true)) {
    Serial.println(F("      SH1106 Initialized successfully!"));
    displaySH.clearDisplay();
    displaySH.fillScreen(SH110X_WHITE);
    displaySH.setTextColor(SH110X_BLACK, SH110X_WHITE);
    displaySH.setTextSize(1);
    displaySH.setCursor(10, 16);
    displaySH.print(F("SH1106 (1.3\")"));
    displaySH.setCursor(10, 30);
    displaySH.print(F("CYBERROVER X5"));
    displaySH.setCursor(10, 44);
    displaySH.print(F("I2C ADDR: 0x"));
    displaySH.print(addr, HEX);
    displaySH.display();
  }

  delay(2500);

  // 3. Try SSD1306 Driver
  Serial.println(F("[3/4] Initializing SSD1306 Driver (0.96\" OLED)..."));
  if (displaySSD.begin(SSD1306_SWITCHCAPVCC, addr)) {
    Serial.println(F("      SSD1306 Initialized successfully!"));
    displaySSD.clearDisplay();
    displaySSD.fillScreen(SSD1306_WHITE);
    displaySSD.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    displaySSD.setTextSize(1);
    displaySSD.setCursor(10, 16);
    displaySSD.print(F("SSD1306 (0.96\")"));
    displaySSD.setCursor(10, 30);
    displaySSD.print(F("CYBERROVER X5"));
    displaySSD.setCursor(10, 44);
    displaySSD.print(F("I2C ADDR: 0x"));
    displaySSD.print(addr, HEX);
    displaySSD.display();
  }

  delay(2500);

  // 4. Invert / Flash Pattern Test
  Serial.println(F("[4/4] Running Glass Contrast & Alternating Invert Test..."));
  for (int i = 0; i < 4; i++) {
    Wire.beginTransmission(addr);
    Wire.write(0x00);
    Wire.write(0xA7); // Invert display (white background)
    Wire.endTransmission();
    delay(200);

    Wire.beginTransmission(addr);
    Wire.write(0x00);
    Wire.write(0xA6); // Normal display
    Wire.endTransmission();
    delay(200);
  }
}

// -------------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println(F("======================================================="));
  Serial.println(F("   CYBERROVER X5 — STAGE 4: I2C SCANNER & OLED TEST   "));
  Serial.println(F("======================================================="));

  // Check physical line states before activating hardware I2C
  pinMode(OLED_SCL_PIN, INPUT);
  pinMode(OLED_SDA_PIN, INPUT);
  delay(10);
  int scl_state = digitalRead(OLED_SCL_PIN);
  int sda_state = digitalRead(OLED_SDA_PIN);

  Serial.print(F("Physical Line Check: SCL (PB6) = "));
  Serial.print(scl_state ? F("HIGH (Idle/Pullup OK)") : F("LOW [WARNING: Line is grounded/shorted!]"));
  Serial.print(F(" | SDA (PB7) = "));
  Serial.println(sda_state ? F("HIGH (Idle/Pullup OK)") : F("LOW [WARNING: Line is grounded/shorted!]"));

  // Attempt bus unlock in case a slave hung
  Serial.println(F("Pulsing SCL 9 times to unlock any stuck slave devices..."));
  unlockI2CBus();

  // Configure STM32 Hardware I2C Pins
  #if defined(ARDUINO_ARCH_STM32)
    Wire.setSCL(OLED_SCL_PIN);
    Wire.setSDA(OLED_SDA_PIN);
  #endif

  Wire.begin();
  Wire.setClock(100000); // 100 kHz standard stable I2C
  delay(100);

  // Initial Scan
  scanI2CBus();

  if (oled_found) {
    testOLED(detected_oled_addr);
  }
}

// -------------------------------------------------------------------------------
// LOOP: Scans continuously every 4 seconds so you can plug/wiggle wires live!
// -------------------------------------------------------------------------------
uint32_t lastScanMs = 0;

void loop() {
  uint32_t now = millis();

  if (now - lastScanMs >= 4000) {
    lastScanMs = now;
    scanI2CBus();

    if (oled_found) {
      testOLED(detected_oled_addr);
    }
  }

  delay(20);
}
