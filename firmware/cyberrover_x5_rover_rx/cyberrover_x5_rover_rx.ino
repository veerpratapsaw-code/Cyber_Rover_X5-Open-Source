/*
 * ================================================================================
 * CYBERROVER X5 — INDUSTRIAL-GRADE RECEIVER & FAILSAFE SUPERVISOR (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Platform       : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Carrier Node Receiver & Vehicle Drivetrain Supervisor
 *
 * SAFETY & RESILIENCE GUARANTEES:
 *   1. Zero SPI Register Contention: Radio is read directly without busy-polling.
 *   2. 12-Bit ADC Packet Sanity Check: Floating bus noise (0xFFFF) is rejected instantly.
 *   3. 500ms Deterministic Motor Failsafe: Clamps throttle & steering to neutral on loss.
 *   4. Reconnection Qualification Guard: Requires 3 consecutive valid packets to unclamp.
 *   5. Background Silicon Auto-Recovery: Controlled, bounded re-init during prolonged drop.
 * ================================================================================
 */

#include <Arduino.h>
#include <SPI.h>

#include "cyberrover_protocol.h"
#include "cyberrover_rx_config.h"
#include "receiver_failsafe.h"
#include "radio_rx_manager.h"

// =============================================================================
//  SUBSYSTEM INSTANCES
// =============================================================================
static RadioRxManager   radioRx;
static ReceiverFailsafe failsafe;
static RoverPacket      rxPacket;

static uint32_t lastTelemetryTime = 0;

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_HB, OUTPUT);
  digitalWrite(PIN_LED_HB, HIGH); // Off for active-low PC13

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — INDUSTRIAL-GRADE RECEIVER & FAILSAFE BENCH MONITOR v1.5       "));
  Serial.println(F("  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           "));
  Serial.println(F("================================================================================"));

  // 1. Initialize NRF24L01+ Radio Driver on Hardware SPI1 (PA5-7, CE=PA3, CSN=PA4)
  Serial.print(F(">>> Initializing NRF24L01+ Radio on Native SPI1 (PA5-7, CE=PA3, CSN=PA4)... "));
  if (radioRx.init(PIN_NRF_CE, PIN_NRF_CSN, CYBERROVER_RF_CHANNEL, CYBERROVER_RF_PIPE)) {
    Serial.println(F("[PASSED - SILICON OK]"));
    Serial.print(F(">>> Operating Channel: "));
    Serial.print(CYBERROVER_RF_CHANNEL);
    Serial.print(F(" (2.508 GHz) | Rate: 250 kbps | Payload: "));
    Serial.print(sizeof(RoverPacket));
    Serial.println(F(" Bytes"));
    Serial.println(F(">>> Waiting for RF packets from Remote Transmitter..."));
  } else {
    Serial.println(F("[FAILED!] Check 5V power, capacitor, and SPI1 wiring!"));
  }

  // 2. Initialize Safety Failsafe Engine (500ms timeout, 3 packets to recover)
  failsafe.init(FAILSAFE_TIMEOUT_MS, RECONNECT_REQUIRED_PACKETS);
  Serial.println(F(">>> Safety Failsafe Supervisor: 500ms Timeout Active (Neutral Motor Clamp)"));
  Serial.println(F("================================================================================"));
  Serial.println();
}

// =============================================================================
//  MAIN REAL-TIME LOOP
// =============================================================================
void loop() {
  uint32_t now = millis();

  // ---------------------------------------------------------------------------
  // 1. PROCESS INCOMING RADIO PACKETS WITH ZERO SPI REGISTER COLLISION
  // ---------------------------------------------------------------------------
  RoverPacket tempPacket;
  if (radioRx.processIncoming(tempPacket, now)) {
    rxPacket = tempPacket;
    failsafe.onValidPacket(rxPacket, now);

    // Heartbeat LED toggles on valid packet receipt
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
  }

  // ---------------------------------------------------------------------------
  // 2. UPDATE FAILSAFE SUPERVISOR & DETECT TRANSITIONS
  // ---------------------------------------------------------------------------
  failsafe.update(now);

  if (failsafe.justRecovered()) {
    Serial.println();
    Serial.println(F("****************************************************************"));
    Serial.println(F("  >>> RF LINK CONNECTED! RECEIVING LIVE DATA FROM REMOTE! <<<   "));
    Serial.println(F("****************************************************************"));
  } else if (failsafe.justEnteredFailsafe()) {
    digitalWrite(PIN_LED_HB, HIGH); // Turn LED OFF when disconnected
    Serial.println();
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
    Serial.println(F("  [ALERT] RF LINK LOST! NO PACKET FOR >500ms -> FAILSAFE ENGAGED"));
    Serial.println(F("  [ACTION] All motor outputs clamped to neutral PWM (0, 0)!     "));
    Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
  }

  // ---------------------------------------------------------------------------
  // 3. SILICON AUTO-RECOVERY (Only active during prolonged link loss > 2000ms)
  // ---------------------------------------------------------------------------
  radioRx.checkBackgroundRecovery(now, now - failsafe.getPacketAgeMs(now));

  if (radioRx.justRecovered()) {
    Serial.println(F(">>> [AUTO-RECOVERED] NRF24 Silicon Re-Established! Listening for Remote... <<<"));
  }

  // ---------------------------------------------------------------------------
  // 4. PERIODIC LIVE TELEMETRY STREAM TO SERIAL (10 Hz = 100ms interval)
  // ---------------------------------------------------------------------------
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;

    if (failsafe.isFailsafeActive()) {
      Serial.print(F("[LINK: DISCONNECTED / FAILSAFE] Waiting for Remote... (Age: "));
      Serial.print(failsafe.getPacketAgeMs(now));
      Serial.println(F(" ms)"));
    } else {
      const RoverMotorCommand& cmd = failsafe.getCommand();

      // Print Frame Sequence & Packet Rate
      Serial.print(F("[FRM #"));
      Serial.print(rxPacket.frame_id);
      Serial.print(F(" | "));
      Serial.print(radioRx.getPacketRateHz());
      Serial.print(F(" Hz] "));

      // Print Raw Analog Inputs
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
      Serial.print(rxPacket.toggle_l);
      Serial.print(F(" TR:"));
      Serial.print(rxPacket.toggle_r); // Mute flag
      Serial.print(F(" PL:"));
      Serial.print(rxPacket.push_l);
      Serial.print(F(" PR:"));
      Serial.print(rxPacket.push_r);
      Serial.print(F(" CKL:"));
      Serial.print(rxPacket.stick_clk_l);
      Serial.print(F(" CKR:"));
      Serial.print(rxPacket.stick_clk_r);
      Serial.print(F("]"));

      // Print Computed Vehicle Motor Command
      Serial.print(F(" || MOTOR: [THR:"));
      Serial.print(cmd.throttle_norm);
      Serial.print(F(" STR:"));
      Serial.print(cmd.steering_norm);
      Serial.print(F(" ARM:"));
      Serial.print(cmd.is_armed ? F("1") : F("0"));
      Serial.println(F("]"));
    }
  }
}
