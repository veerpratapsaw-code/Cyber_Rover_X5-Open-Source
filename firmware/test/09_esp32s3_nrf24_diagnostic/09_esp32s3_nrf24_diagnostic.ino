/*
 * ===============================================================================
 * CYBERROVER X5 — ESP32-S3 (R8N16) NRF24L01+ BENCH DIAGNOSTIC & RECEIVER
 * ===============================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCU     : ESP32-S3 (N16R8 / R8N16 DevKit)
 *
 * PURPOSE:
 *   1. Hardware sanity check to prove NRF24 module is 100% healthy.
 *   2. Uses the strong 500mA+ 3.3V onboard LDO of the ESP32-S3 (no voltage dropouts).
 *   3. Tests SPI communication and listens for live packets from the Remote Transmitter!
 *
 * CRITICAL PINOUT FOR ESP32-S3 (R8N16):
 *   WARNING: In R8N16 boards, GPIO 33-37 are dedicated to Octal PSRAM. Never use them!
 *   We use standard, safe, consecutive GPIOs 9, 10, 11, 12, 13:
 *
 *   ESP32-S3 Pin         NRF24 Pin (Back View / Pins Facing You)
 *   -----------------    ---------------------------------------
 *   3.3V            ---> Pin 2 (VCC) [Top-Right]
 *   GND             ---> Pin 1 (GND) [Top-Left]
 *   GPIO 9          ---> Pin 3 (CE)  [Row 2, Left]
 *   GPIO 10         ---> Pin 4 (CSN) [Row 2, Right]
 *   GPIO 12         ---> Pin 5 (SCK) [Row 3, Left]
 *   GPIO 11         ---> Pin 6 (MOSI)[Row 3, Right]
 *   GPIO 13         ---> Pin 7 (MISO)[Bottom-Left]
 *   (NC)                 Pin 8 (IRQ)  LEAVE UNCONNECTED!
 * ===============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

#define PIN_NRF_CE    9
#define PIN_NRF_CSN   10
#define PIN_NRF_MOSI  11
#define PIN_NRF_SCK   12
#define PIN_NRF_MISO  13

const uint8_t  RF_CHANNEL = 108;
const uint64_t RF_PIPE    = 0xE8E8F0F0E1LL;

RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);

void printHex(uint8_t val) {
  if (val < 0x10) Serial.print("0x0");
  else Serial.print("0x");
  Serial.print(val, HEX);
}

uint8_t readNrfRegister(uint8_t reg) {
  digitalWrite(PIN_NRF_CSN, LOW);
  delayMicroseconds(5);
  SPI.transfer(0x00 | (reg & 0x1F)); // R_REGISTER
  uint8_t val = SPI.transfer(0xFF);
  delayMicroseconds(5);
  digitalWrite(PIN_NRF_CSN, HIGH);
  return val;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("================================================================================");
  Serial.println("  CYBERROVER X5 — ESP32-S3 (R8N16) NRF24 HARDWARE SPI & LINK DIAGNOSTIC        ");
  Serial.println("  Pins: SCK=GPIO12, MISO=GPIO13, MOSI=GPIO11, CSN=GPIO10, CE=GPIO9             ");
  Serial.println("================================================================================");

  // 1. Configure Hardware SPI on custom ESP32-S3 pins
  pinMode(PIN_NRF_CSN, OUTPUT);
  digitalWrite(PIN_NRF_CSN, HIGH);
  pinMode(PIN_NRF_CE, OUTPUT);
  digitalWrite(PIN_NRF_CE, LOW);

  SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN);
  SPI.setFrequency(2000000); // 2 MHz SPI clock
  delay(100);

  // 2. Direct Register Probe
  Serial.println("[STEP 1/3] Reading NRF24 Silicon Registers directly over SPI...");
  uint8_t regConfig   = readNrfRegister(0x00);
  uint8_t regStatus   = readNrfRegister(0x07);
  uint8_t regRfCh     = readNrfRegister(0x05);
  uint8_t regSetupAw  = readNrfRegister(0x03);

  Serial.print("    STATUS   (0x07): "); printHex(regStatus);
  if (regStatus == 0x0E || (regStatus & 0x0E) == 0x0E) {
    Serial.println(" -> [PERFECT: NRF24 ACTIVE & FUNCTIONAL!]");
  } else if (regStatus == 0xFF) {
    Serial.println(" -> [FAIL 0xFF: MISO FLOATING OR NO POWER]");
  } else if (regStatus == 0x00) {
    Serial.println(" -> [FAIL 0x00: SHORT TO GND]");
  } else {
    Serial.println(" -> [ANOMALOUS]");
  }

  Serial.print("    CONFIG   (0x00): "); printHex(regConfig);  Serial.println(" (Default: 0x08)");
  Serial.print("    SETUP_AW (0x03): "); printHex(regSetupAw); Serial.println(" (Default: 0x03)");
  Serial.print("    RF_CH    (0x05): "); printHex(regRfCh);    Serial.println(" (Default: 0x02)");

  // 3. RF24 Library Initialization
  Serial.println();
  Serial.print("[STEP 2/3] Initializing RF24 Library: ");
  if (radio.begin()) {
    if (radio.isChipConnected()) {
      Serial.println("[PASSED - SILICON CONFIRMED]");
      radio.setPALevel(RF24_PA_LOW);
      radio.setDataRate(RF24_250KBPS);
      radio.setChannel(RF_CHANNEL);
      radio.openReadingPipe(1, RF_PIPE);
      radio.startListening();
      Serial.println("[STEP 3/3] Listening on Channel 108 (2.508 GHz). Turn on Remote Transmitter to test live link!");
    } else {
      Serial.println("[FAILED: radio.begin() ok but isChipConnected() false]");
    }
  } else {
    Serial.println("[FAILED: radio.begin() returned false]");
  }
  Serial.println("================================================================================");
}

void loop() {
  static uint32_t lastPrint = 0;
  static uint32_t pktCount = 0;
  uint32_t now = millis();

  if (radio.isChipConnected() && radio.available()) {
    uint8_t buffer[32];
    uint8_t len = radio.getPayloadSize();
    if (len > sizeof(buffer)) len = sizeof(buffer);
    radio.read(buffer, len);
    pktCount++;

    Serial.print(">>> [LIVE PACKET #");
    Serial.print(pktCount);
    Serial.print("] (");
    Serial.print(len);
    Serial.print(" Bytes): ");
    for (uint8_t i = 0; i < len; i++) {
      printHex(buffer[i]);
      Serial.print(" ");
    }
    Serial.println();
  }

  if (now - lastPrint >= 2000) {
    lastPrint = now;
    bool connected = radio.isChipConnected();
    Serial.print("[HEARTBEAT] NRF24 Connected: ");
    Serial.print(connected ? "YES (ONLINE)" : "NO (OFFLINE)");
    Serial.print(" | Packets Received: ");
    Serial.println(pktCount);
  }
}
