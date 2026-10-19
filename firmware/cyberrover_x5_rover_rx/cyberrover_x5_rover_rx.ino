/*
 * ================================================================================
 * CYBERROVER X5 — VEHICLE RECEIVER LINK VERIFICATION BENCH FIRMWARE v1.1
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Platform       : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Carrier Perfboard Receiver Node (Wireless Input Verification)
 *
 * SCOPE:
 *   - Receives all 13 remote control channels over NRF24L01+ 2.4 GHz RF link.
 *   - Continuous hardware silicon watchdog: instantly flags loose jumpers!
 *   - Live Serial Monitor telemetry display (115200 baud).
 *   - Automatic 500ms Signal Loss Failsafe monitor.
 *   - Onboard Heartbeat / Packet Activity LED indicator on PC13.
 *
 * EXACT HARDWARE WIRING (Receiver STM32 Blue Pill <---> NRF24L01+ Module):
 *   - Pin 1 (GND) ---> STM32 GND
 *   - Pin 2 (VCC) ---> STM32 3.3V (MUST have 10uF - 100uF capacitor to GND!)
 *   - Pin 3 (CE)  ---> PB10 (Chip Enable: Activates RX listening)
 *   - Pin 4 (CSN) ---> PB11 (Chip Select Not: SPI bus select)
 *   - Pin 5 (SCK) ---> PA5  (Hardware SPI1 Clock)
 *   - Pin 6 (MOSI)---> PA7  (Hardware SPI1 Master-Out-Slave-In)
 *   - Pin 7 (MISO)---> PA6  (Hardware SPI1 Master-In-Slave-Out)
 *   - Pin 8 (IRQ) ---> Not Connected (NC)
 *
 * USB-TTL PROGRAMMING & SERIAL TELEMETRY:
 *   - PA9  (TX1)  ---> USB-TTL RXD
 *   - PA10 (RX1)  ---> USB-TTL TXD
 *   - GND         ---> USB-TTL GND
 *   - 5V / 3.3V   ---> USB-TTL 5V / 3.3V
 * ================================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// =============================================================================
//  PIN DEFINITIONS & CONFIGURATION
// =============================================================================
#define PIN_NRF_CE    PB10
#define PIN_NRF_CSN   PB11
#define PIN_LED_HB    PC13    // Onboard Blue Pill LED (Active-LOW)

// RF Link Constants (Identical to Transmitter)
const uint8_t  RF_CHANNEL = 108;            // 2.508 GHz (Above Wi-Fi channels 1-13)
const uint64_t RF_PIPE    = 0xE8E8F0F0E1LL; // CyberRover X5 Link Address

// =============================================================================
//  UNIFIED 13-CHANNEL DATA PACKET STRUCTURE (24 Bytes)
// =============================================================================
struct __attribute__((packed)) RoverPacket {
  uint16_t stick_lx;    // Left Stick X   (0 - 4095)
  uint16_t stick_ly;    // Left Stick Y   (0 - 4095)
  uint16_t stick_rx;    // Right Stick X  (0 - 4095, Inverted on TX)
  uint16_t stick_ry;    // Right Stick Y  (0 - 4095)
  uint16_t pot_l;       // Pot Left       (0 - 4095, Speed Governor)
  uint16_t pot_c;       // Pot Center     (0 - 4095, Camera Tilt)
  uint16_t pot_r;       // Pot Right      (0 - 4095, Aux)
  uint8_t  toggle_l;    // 1 = ON, 0 = OFF
  uint8_t  toggle_r;    // 1 = ON, 0 = OFF
  uint8_t  push_l;      // 1 = PRESSED, 0 = RELEASED
  uint8_t  push_r;      // 1 = PRESSED, 0 = RELEASED
  uint8_t  stick_clk_l; // 1 = PRESSED, 0 = UP
  uint8_t  stick_clk_r; // 1 = PRESSED, 0 = UP
  uint32_t frame_id;    // Rolling frame sequence counter
};

RoverPacket rxPacket;
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);

// =============================================================================
//  DIAGNOSTIC & FAILSAFE TRACKING VARIABLES
// =============================================================================
uint32_t lastPacketTime   = 0;
uint32_t packetsReceived  = 0;
uint32_t packetsPerSec    = 0;
uint32_t lastRateCheck    = 0;
uint32_t lastPrintTime    = 0;
bool     failsafeActive   = true;

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_LED_HB, OUTPUT);
  digitalWrite(PIN_LED_HB, HIGH); // Off for active-low PC13

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — RECEIVER RF LINK VERIFICATION BENCH MONITOR v1.1              "));
  Serial.println(F("  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           "));
  Serial.println(F("================================================================================"));

  // 1. Initialize Hardware SPI1 bus explicitly
  SPI.begin();

  // 2. Initialize NRF24L01 Module
  Serial.print(F(">>> Initializing NRF24L01+ Radio on Native SPI1 (PA5-7, PB10, PB11)... "));
  if (!radio.begin()) {
    Serial.println(F("[FAILED!]"));
    Serial.println(F("CRITICAL ERROR: NRF24 silicon not responding on SPI1!"));
    Serial.println(F("Troubleshooting Checklist:"));
    Serial.println(F("  1. Verify 3.3V & GND wiring to NRF module (Do NOT use 5V!)."));
    Serial.println(F("  2. Solder a 10uF - 100uF capacitor across NRF24 VCC and GND pins."));
    Serial.println(F("  3. Check SPI1 pin connections: SCK=PA5, MISO=PA6, MOSI=PA7, CE=PB10, CSN=PB11."));
    while (1) {
      digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
      delay(100); // Rapid alert flash
    }
  }
  Serial.println(F("[PASSED - SILICON OK]"));

  // 3. Configure RF Parameters (Identical match to Transmitter)
  radio.setPALevel(RF24_PA_LOW);           // Bench test power (prevents brownouts)
  radio.setDataRate(RF24_250KBPS);         // 250 kbps for maximum sensitivity and range
  radio.setChannel(RF_CHANNEL);            // Channel 108 (2.508 GHz)
  radio.setPayloadSize(sizeof(RoverPacket));
  radio.openReadingPipe(1, RF_PIPE);       // Listen on primary address
  radio.startListening();

  Serial.print(F(">>> Radio Listening on Channel: "));
  Serial.print(RF_CHANNEL);
  Serial.print(F(" (2.508 GHz) | Speed: 250 kbps | Payload: "));
  Serial.print(sizeof(RoverPacket));
  Serial.println(F(" Bytes"));
  Serial.println(F(">>> Waiting for RF packets from Remote Transmitter..."));
  Serial.println(F("================================================================================"));
  Serial.println();
}

// =============================================================================
//  MAIN LOOP
// =============================================================================
void loop() {
  uint32_t now = millis();

  // ---------------------------------------------------------------------------
  // 1. CHECK FOR INCOMING RADIO PACKETS
  // ---------------------------------------------------------------------------
  if (radio.available()) {
    radio.read(&rxPacket, sizeof(RoverPacket));

    lastPacketTime = now;
    packetsReceived++;
    packetsPerSec++;

    // Toggle onboard LED on every packet received (Visual Heartbeat)
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));

    if (failsafeActive) {
      failsafeActive = false;
      Serial.println();
      Serial.println(F("****************************************************************"));
      Serial.println(F("  >>> RF LINK CONNECTED! RECEIVING LIVE DATA FROM REMOTE! <<<   "));
      Serial.println(F("****************************************************************"));
    }
  }

  // ---------------------------------------------------------------------------
  // 2. FAILSAFE TIMEOUT MONITOR (500 ms threshold)
  // ---------------------------------------------------------------------------
  if (!failsafeActive && (now - lastPacketTime > 500)) {
    failsafeActive = true;
    digitalWrite(PIN_LED_HB, HIGH); // Turn LED off when link lost
    Serial.println();
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
    Serial.println(F("  [ALERT] RF LINK LOST! NO PACKET FOR >500ms -> FAILSAFE ENGAGED"));
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
  }

  // ---------------------------------------------------------------------------
  // 3. PACKET RATE CALCULATOR (Every 1000ms)
  // ---------------------------------------------------------------------------
  static uint32_t displayRate = 0;
  if (now - lastRateCheck >= 1000) {
    lastRateCheck = now;
    displayRate = packetsPerSec;
    packetsPerSec = 0;
  }

  // ---------------------------------------------------------------------------
  // 4. PERIODIC LIVE TELEMETRY STREAM TO SERIAL (Every 100ms = 10 Hz)
  // ---------------------------------------------------------------------------
  if (now - lastPrintTime >= 100) {
    lastPrintTime = now;

    // Silicon Watchdog: Live probe to detect if jumper wire disconnected
    if (!radio.isChipConnected()) {
      Serial.println(F(">>> [HARDWARE ERROR] NRF24 SILICON DISCONNECTED! Check SPI & 3.3V wiring! <<<"));
      return;
    }

    if (failsafeActive) {
      Serial.println(F("[LINK: DISCONNECTED / FAILSAFE] Waiting for Remote Controller..."));
    } else {
      // Print Frame & Packet Rate
      Serial.print(F("[FRM #"));
      Serial.print(rxPacket.frame_id);
      Serial.print(F(" | "));
      Serial.print(displayRate);
      Serial.print(F(" Hz] "));

      // Print Joysticks (12-bit: 0 - 4095)
      Serial.print(F("LX:"));
      Serial.print(rxPacket.stick_lx);
      Serial.print(F(" LY:"));
      Serial.print(rxPacket.stick_ly);
      Serial.print(F(" | RX:"));
      Serial.print(rxPacket.stick_rx);
      Serial.print(F(" RY:"));
      Serial.print(rxPacket.stick_ry);

      // Print Potentiometers
      Serial.print(F(" || POTS: [L:"));
      Serial.print(rxPacket.pot_l);
      Serial.print(F(" C:"));
      Serial.print(rxPacket.pot_c);
      Serial.print(F(" R:"));
      Serial.print(rxPacket.pot_r);
      Serial.print(F("]"));

      // Print Digital Controls
      Serial.print(F(" || SW: [TL:"));
      Serial.print(rxPacket.toggle_l ? F("1") : F("0"));
      Serial.print(F(" TR:"));
      Serial.print(rxPacket.toggle_r ? F("1") : F("0"));
      Serial.print(F(" PL:"));
      Serial.print(rxPacket.push_l ? F("1") : F("0"));
      Serial.print(F(" PR:"));
      Serial.print(rxPacket.push_r ? F("1") : F("0"));
      Serial.print(F(" CKL:"));
      Serial.print(rxPacket.stick_clk_l ? F("1") : F("0"));
      Serial.print(F(" CKR:"));
      Serial.print(rxPacket.stick_clk_r ? F("1") : F("0"));
      Serial.println(F("]"));
    }
  }
}
