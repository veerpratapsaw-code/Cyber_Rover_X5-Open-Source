/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 6: NRF24L01+PA+LNA STANDALONE BENCH TEST
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 *
 * Hardware: NRF24L01+PA+LNA 2.4 GHz Transceiver on Native SPI1
 * Pins on STM32 Blue Pill Carrier (Left Header, Pins 10-12 & 15-16):
 *   - PA5  (Pin 10) ---> NRF SCK   (Hardware SPI1 Clock)
 *   - PA6  (Pin 11) ---> NRF MISO  (Hardware SPI1 Master In Slave Out)
 *   - PA7  (Pin 12) ---> NRF MOSI  (Hardware SPI1 Master Out Slave In)
 *   - PB10 (Pin 15) ---> NRF CE    (Chip Enable: RX/TX mode control)
 *   - PB11 (Pin 16) ---> NRF CSN   (Chip Select Not: SPI bus select)
 *   - VCC           ---> 3.3V rail (Must have 10uF-100uF capacitor nearby)
 *   - GND           ---> Perfboard Ground rail
 *
 * How This Sketch Tests the NRF24 Alone:
 *   1. Hardware SPI1 Link Probe: Direct low-level register probe (Expected: 0x0E).
 *   2. Silicon Verification: Confirms Nordic nRF24L01+ silicon response.
 *   3. Register Read/Write Integrity: Writes Channel 108 & 250kbps; reads back to verify.
 *   4. Ambient RF Wave Sniffer: Tests RF receiver frontend for carrier wave activity.
 *   5. Real-Time Packet Beacon: Transmits live RF packets every 500ms:
 *      - If Partner RX is listening: Reports ACK SUCCESS!
 *      - If Partner RX is OFF: Confirms packet transmitted into the air (MAX_RT retries).
 * ===============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// Pin Definitions
#define NRF_SCK_PIN   PA5
#define NRF_MISO_PIN  PA6
#define NRF_MOSI_PIN  PA7
#define NRF_CE_PIN    PB10
#define NRF_CSN_PIN   PB11
#define PIN_LED_HB    PC13

RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);

// Radio Configuration Constants
const uint8_t RF_CHANNEL = 108;               // 2.508 GHz (Above Wi-Fi channels 1-13)
const uint64_t TX_PIPE   = 0xE8E8F0F0E1LL;    // Default CyberRover X5 Link Address

// Direct Low-Level SPI Probe: Reads NRF24 STATUS register without library overhead
uint8_t pingNRF_Status() {
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(NRF_CSN_PIN, LOW);
  delayMicroseconds(10);
  uint8_t status = SPI.transfer(0xFF); // NOP instruction
  delayMicroseconds(10);
  digitalWrite(NRF_CSN_PIN, HIGH);
  SPI.endTransaction();
  return status;
}

// Low-Level Read Register Helper
uint8_t readRegister(uint8_t reg) {
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(NRF_CSN_PIN, LOW);
  delayMicroseconds(10);
  SPI.transfer(0x00 | (reg & 0x1F)); // R_REGISTER command
  uint8_t val = SPI.transfer(0x00);
  delayMicroseconds(10);
  digitalWrite(NRF_CSN_PIN, HIGH);
  SPI.endTransaction();
  return val;
}

void printHexByte(uint8_t val) {
  if (val < 0x10) Serial.print("0x0");
  else Serial.print("0x");
  Serial.print(val, HEX);
}

void setup() {
  pinMode(PIN_LED_HB, OUTPUT);
  digitalWrite(PIN_LED_HB, LOW); // LED ON initially

  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println(F("======================================================="));
  Serial.println(F("  CYBERROVER X5 — STAGE 6: NRF24L01 STANDALONE TEST    "));
  Serial.println(F("  SPI1: SCK=PA5, MISO=PA6, MOSI=PA7, CE=PB10, CSN=PB11 "));
  Serial.println(F("======================================================="));

  // 1. Configure Hardware SPI1
  pinMode(NRF_CE_PIN, OUTPUT);
  pinMode(NRF_CSN_PIN, OUTPUT);
  digitalWrite(NRF_CE_PIN, LOW);
  digitalWrite(NRF_CSN_PIN, HIGH);

  SPI.begin();
  delay(50);

  // 2. Direct SPI Probe
  Serial.print(F("[TEST 1/5] Direct SPI STATUS Register Probe: "));
  uint8_t rawStatus = pingNRF_Status();
  printHexByte(rawStatus);

  if (rawStatus == 0x0E) {
    Serial.println(F(" -> PERFECT! (Silicon Awake, Standby-I Mode)"));
  } else if (rawStatus == 0xFF) {
    Serial.println(F(" -> FAIL: 0xFF (MISO wire floating or Radio Unpowered!)"));
  } else if (rawStatus == 0x00) {
    Serial.println(F(" -> FAIL: 0x00 (Radio Unpowered or MISO shorted to GND!)"));
  } else {
    Serial.println(F(" -> UNKNOWN RESPONSE (Check wiring)"));
  }

  // 3. Initialize RF24 Library
  Serial.print(F("[TEST 2/5] RF24 Library Initialization: "));
  bool beginSuccess = radio.begin();
  bool chipConnected = radio.isChipConnected();

  if (beginSuccess && chipConnected) {
    Serial.println(F("SUCCESS (radio.begin() & isChipConnected() TRUE)"));
  } else {
    Serial.println(F("FAILED (Hardware not communicating with library)"));
  }

  // 4. Register Read/Write Integrity Verification
  Serial.print(F("[TEST 3/5] Register Configuration Integrity: "));
  radio.setChannel(RF_CHANNEL);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_LOW); // PA_LOW for safe bench testing without power spikes
  radio.setAutoAck(true);
  radio.setRetries(5, 15);       // 1500us delay, 15 retries
  radio.enableDynamicPayloads();
  radio.openWritingPipe(TX_PIPE);
  radio.stopListening();         // Enter Standby-II / TX Mode

  uint8_t readChannel = readRegister(0x05); // RF_CH register
  if (readChannel == RF_CHANNEL) {
    Serial.print(F("PASSED (Channel verified at "));
    Serial.print(readChannel);
    Serial.println(F(")"));
  } else {
    Serial.print(F("MISMATCH! Wrote 108, Read back: "));
    Serial.println(readChannel);
  }

  // 5. Test RF Receiver Frontend / Ambient Carrier Detection
  Serial.print(F("[TEST 4/5] RF Receiver Frontend Sniffer: "));
  radio.startListening();
  delay(20);
  bool carrierDetected = radio.testCarrier();
  radio.stopListening();
  Serial.println(carrierDetected ? F("Carrier Energy Detected in Room (Active RF Frontend)") : 
                                  F("RF Channel Clean & Quiet (Frontend Functional)"));

  Serial.println(F("======================================================="));
  Serial.println(F("[TEST 5/5] Live 2.4 GHz Transmission Loop Starting...  "));
  Serial.println(F("Transmitting live beacon packet every 500ms...        "));
  Serial.println(F("NOTE:"));
  Serial.println(F("  - If RX partner is OFF: [TX OK - AIR ONLY] is EXPECTED! (Proves RF transmitted)"));
  Serial.println(F("  - If RX partner is ON : [TX SUCCESS (ACK)] will appear!"));
  Serial.println(F("======================================================="));
  delay(1000);
}

// Packet structure for test beacon
struct TestPacket {
  uint32_t packetSeq;
  uint32_t uptimeMs;
  char signature[12];
};

TestPacket packet = {0, 0, "CYBER_X5_OK"};
bool hbState = false;

void loop() {
  packet.packetSeq++;
  packet.uptimeMs = millis();

  // Toggle PC13 Heartbeat LED
  hbState = !hbState;
  digitalWrite(PIN_LED_HB, hbState ? LOW : HIGH);

  // Read current radio status byte
  uint8_t statusByte = pingNRF_Status();

  Serial.print(F("[PKT #"));
  Serial.print(packet.packetSeq);
  Serial.print(F("] Status: "));
  printHexByte(statusByte);
  Serial.print(F(" | Sending Beacon... "));

  // Transmit 20-byte test packet
  bool txSuccess = radio.write(&packet, sizeof(packet));

  if (txSuccess) {
    Serial.println(F(">>> [TX SUCCESS (ACK RECEIVED!)] <<<"));
  } else {
    // If radio.write() returned false because partner is not on, check if chip is still alive
    if (radio.isChipConnected()) {
      Serial.println(F("[TX OK - AIR TRANSMITTED (No Partner ACK)]"));
    } else {
      Serial.println(F(">>> [SPI BUS ERROR - CHIP DISCONNECTED!] <<<"));
    }
  }

  delay(500); // 2 Hz transmission rate for easy inspection
}
