/*
 * ===============================================================================
 * CYBERROVER X5 — RECEIVER NRF24L01 HARDWARE SPI & LINK DIAGNOSTIC TOOL
 * ===============================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCU     : STM32F103C8T6 (Blue Pill Receiver Node)
 *
 * HARDWARE SPI1 PINOUT (Receiver):
 *   - PA5 ---> NRF SCK   (SPI1 Clock)
 *   - PA6 ---> NRF MISO  (SPI1 Master In Slave Out)
 *   - PA7 ---> NRF MOSI  (SPI1 Master Out Slave In)
 *   - PA3 ---> NRF CE    (Chip Enable)
 *   - PA4 ---> NRF CSN   (Chip Select Not)
 *   - VCC ---> 5V (if black adapter board) OR 3.3V with 10uF-100uF cap
 *   - GND ---> STM32 Common GND
 *
 * PURPOSE:
 *   1. Direct low-level SPI probe (bypasses RF24 library to diagnose exact bus state).
 *   2. Identifies: Floating MISO (0xFF), Grounded/Unpowered (0x00), or Active (0x0E).
 *   3. Dumps NRF24 registers (CONFIG, EN_AA, SETUP_AW, RF_CH, STATUS).
 *   4. Tests RF reception on Channel 108 (2.508 GHz).
 * ===============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

#define PIN_NRF_CE    PA3
#define PIN_NRF_CSN   PA4
#define PIN_LED_HB    PC13

const uint8_t  RF_CHANNEL = 108;
const uint64_t RF_PIPE    = 0xE8E8F0F0E1LL;

RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);

// Direct SPI transfer without RF24 library overhead
uint8_t spiTransferByte(uint8_t cmd) {
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_NRF_CSN, LOW);
  delayMicroseconds(10);
  uint8_t resp = SPI.transfer(cmd);
  delayMicroseconds(10);
  digitalWrite(PIN_NRF_CSN, HIGH);
  SPI.endTransaction();
  return resp;
}

uint8_t readNrfRegister(uint8_t reg) {
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_NRF_CSN, LOW);
  delayMicroseconds(10);
  SPI.transfer(0x00 | (reg & 0x1F)); // R_REGISTER
  uint8_t val = SPI.transfer(0xFF);  // Clock out byte
  delayMicroseconds(10);
  digitalWrite(PIN_NRF_CSN, HIGH);
  SPI.endTransaction();
  return val;
}

void printHex(uint8_t val) {
  if (val < 0x10) Serial.print("0x0");
  else Serial.print("0x");
  Serial.print(val, HEX);
}

void setup() {
  pinMode(PIN_LED_HB, OUTPUT);
  digitalWrite(PIN_LED_HB, LOW); // LED ON initially

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — RECEIVER NRF24 HARDWARE SPI & LINK DIAGNOSTIC               "));
  Serial.println(F("  Pins: SCK=PA5, MISO=PA6, MOSI=PA7, CE=PA3, CSN=PA4                            "));
  Serial.println(F("================================================================================"));

  // 1. Pre-SPI GPIO Electrical Health Check on PA6 (MISO)
  Serial.println(F("[STEP 0/4] Checking Electrical State of PA6 (MISO)..."));
  pinMode(PA6, INPUT_PULLDOWN);
  delay(15);
  int pd_val = digitalRead(PA6);

  pinMode(PA6, INPUT_PULLUP);
  delay(15);
  int pu_val = digitalRead(PA6);

  if (pd_val == 1 && pu_val == 1) {
    Serial.println(F("  >>> [FAULT DETECTED: PA6 IS HARD-TIED TO 3.3V / IRQ] <<<"));
    Serial.println(F("      PA6 reads HIGH even with internal pulldown resistor!"));
    Serial.println(F("      Probable Causes:"));
    Serial.println(F("        1. PA6 is connected to Pin 8 (IRQ) instead of Pin 7 (MISO)."));
    Serial.println(F("        2. PA6 is solder-bridged to 3.3V VCC."));
    Serial.println(F("        3. NRF24 header is mirrored / reversed left-to-right."));
  } else if (pd_val == 0 && pu_val == 0) {
    Serial.println(F("  >>> [FAULT DETECTED: PA6 IS SHORTED TO GND] <<<"));
  } else {
    Serial.println(F("  >>> [PA6 IS FLOATING / HIGH-IMPEDANCE] (Follows internal pull) <<<"));
    Serial.println(F("      The NRF24 silicon is NOT driving MISO!"));
    Serial.println(F("      Probable Causes:"));
    Serial.println(F("        1. NRF24 has NO power (Measure Pin 2 VCC to Pin 1 GND with voltmeter)."));
    Serial.println(F("        2. CSN (PA4 -> Pin 4) is broken / open (NRF24 stays disabled)."));
    Serial.println(F("        3. MISO wire (PA6) is not reaching Pin 7."));
  }
  Serial.println();

  // 2. Explicit pin initialization BEFORE SPI.begin()
  pinMode(PIN_NRF_CSN, OUTPUT);
  digitalWrite(PIN_NRF_CSN, HIGH);
  pinMode(PIN_NRF_CE, OUTPUT);
  digitalWrite(PIN_NRF_CE, LOW);

  SPI.begin();
  delay(100);

  // 2. Direct Low-Level Hardware SPI Probe
  Serial.print(F("[STEP 1/4] Direct Low-Level SPI STATUS Register Probe: "));
  uint8_t rawStatus = spiTransferByte(0xFF); // NOP instruction returns STATUS
  printHex(rawStatus);

  if (rawStatus == 0x0E) {
    Serial.println(F(" -> [PERFECT] NRF24 Silicon Awake & Healthy (Standby-I Mode)!"));
  } else if (rawStatus == 0xFF) {
    Serial.println(F(" -> [FAIL: 0xFF] MISO is FLOATING! Reasons:"));
    Serial.println(F("      a) NRF24 has NO power (Check VCC/GND wires with voltmeter - must be 3.0V-3.6V)."));
    Serial.println(F("      b) MISO (PA6) wire is disconnected, open, or connected to wrong pin."));
    Serial.println(F("      c) Header pinout is mirrored / upside-down."));
  } else if (rawStatus == 0x00) {
    Serial.println(F(" -> [FAIL: 0x00] MISO is SHORTED TO GND or Chip is BROWNED OUT! Reasons:"));
    Serial.println(F("      a) Voltage too low (sagged below 2.5V under TTL power)."));
    Serial.println(F("      b) SCK (PA5) or MOSI (PA7) wire disconnected/shorted to GND."));
    Serial.println(F("      c) CSN (PA4) not driving HIGH."));
  } else {
    Serial.println(F(" -> [ANOMALOUS] Unknown response. Check for wire swap between MISO/MOSI."));
  }

  // 3. Register Dump
  Serial.println();
  Serial.println(F("[STEP 2/4] Reading NRF24 Core Hardware Registers:"));
  uint8_t regConfig   = readNrfRegister(0x00);
  uint8_t regEnAa     = readNrfRegister(0x01);
  uint8_t regSetupAw  = readNrfRegister(0x03);
  uint8_t regRfCh     = readNrfRegister(0x05);
  uint8_t regRfSetup  = readNrfRegister(0x06);
  uint8_t regFifo     = readNrfRegister(0x17);

  Serial.print(F("    CONFIG   (0x00): ")); printHex(regConfig);  Serial.println(F(" (Expected default: 0x08)"));
  Serial.print(F("    EN_AA    (0x01): ")); printHex(regEnAa);    Serial.println(F(" (Expected default: 0x3F)"));
  Serial.print(F("    SETUP_AW (0x03): ")); printHex(regSetupAw); Serial.println(F(" (Expected: 0x03 for 5-byte addr)"));
  Serial.print(F("    RF_CH    (0x05): ")); printHex(regRfCh);    Serial.println(F(" (Expected default: 0x02)"));
  Serial.print(F("    RF_SETUP (0x06): ")); printHex(regRfSetup); Serial.println(F(" (Expected default: 0x0F)"));
  Serial.print(F("    FIFO_STAT(0x17): ")); printHex(regFifo);    Serial.println(F(" (Expected default: 0x11)"));

  // 4. RF24 Library Initialization
  Serial.println();
  Serial.print(F("[STEP 3/4] Initializing RF24 Library: "));
  if (radio.begin()) {
    if (radio.isChipConnected()) {
      Serial.println(F("[PASSED - SILICON CONFIRMED]"));
      radio.setPALevel(RF24_PA_LOW);
      radio.setDataRate(RF24_250KBPS);
      radio.setChannel(RF_CHANNEL);
      radio.setPayloadSize(24); // 24-byte CyberRover X5 protocol
      radio.openReadingPipe(1, RF_PIPE);
      radio.startListening();
      Serial.println(F("[STEP 4/4] Radio Listening on Channel 108 (2.508 GHz). Waiting for Remote..."));
    } else {
      Serial.println(F("[FAILED - radio.begin() returned true but isChipConnected() is FALSE!]"));
      Serial.println(F(">>> CAUTION: SPI bus cannot read SETUP_AW register. Check MISO/MOSI wiring!"));
    }
  } else {
    Serial.println(F("[FAILED - radio.begin() returned FALSE!]"));
  }
  Serial.println(F("================================================================================"));
  Serial.println();
}

void loop() {
  uint32_t now = millis();
  static uint32_t lastPrint = 0;
  static uint32_t pktCount = 0;

  // Toggle Heartbeat LED
  static uint32_t lastHb = 0;
  if (now - lastHb >= 500) {
    lastHb = now;
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
  }

  // Check for incoming packets
  if (radio.isChipConnected() && radio.available()) {
    uint8_t buffer[32];
    uint8_t len = radio.getPayloadSize();
    if (len > sizeof(buffer)) len = sizeof(buffer);
    radio.read(buffer, len);
    pktCount++;

    Serial.print(F(">>> [PACKET RECEIVED #"));
    Serial.print(pktCount);
    Serial.print(F("] Length: "));
    Serial.print(len);
    Serial.print(F(" Bytes | Raw Hex: "));
    for (uint8_t i = 0; i < len; i++) {
      printHex(buffer[i]);
      Serial.print(" ");
    }
    Serial.println();
  }

  // Periodic status update every 2000ms
  if (now - lastPrint >= 2000) {
    lastPrint = now;
    bool connected = radio.isChipConnected();
    uint8_t st = spiTransferByte(0xFF);
    Serial.print(F("[HEARTBEAT] Chip Connected: "));
    Serial.print(connected ? F("YES (OK)") : F("NO (OFFLINE)"));
    Serial.print(F(" | SPI STATUS: "));
    printHex(st);
    Serial.print(F(" | Total Packets Received: "));
    Serial.println(pktCount);
  }
}
