/*
 * ================================================================================
 * CYBERROVER X5 — 13-CHANNEL REMOTE TRANSMITTER FIRMWARE v1.2 (OPTIMIZED)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras, Jharkhand
 * Platform       : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Handheld Remote Controller Transmitter Node
 *
 * OPTIMIZATIONS & FEATURES:
 *   - Universal Potentiometer Haptic Feedback: Tactile notches active on ALL 3 pots
 *     (Left Pot - Speed, Center Pot - Cam Tilt, Right Pot - Aux).
 *   - Phase-Transition Alert Engine: Zero redundant GPIO writes during active pulses,
 *     drastically reducing CPU cycles and bus traffic.
 *   - Streamlined 40 Hz RF transmission loop (25ms) on Channel 108 @ 250 kbps.
 *   - In-memory hysteresis deadbands preventing ADC noise jitter on stationary controls.
 *   - JTAG release routine (frees PB3, PB4, PA15 for standard GPIO).
 *   - Native 12-bit ADC (0 - 4095) with hardware inverted Right Stick X.
 *
 * HARDWARE PIN WIRING:
 *   ANALOG (7 Channels):
 *     1. PB1  ---> Left Stick X    (ADC9)
 *     2. PB0  ---> Left Stick Y    (ADC8)
 *     3. PA4  ---> Right Stick X   (ADC4) [Inverted: 4095 - raw_rx for 90° mount]
 *     4. PA3  ---> Right Stick Y   (ADC3)
 *     5. PA0  ---> Pot Left        (ADC0) [Speed Governor]
 *     6. PA1  ---> Pot Center      (ADC1) [Camera Tilt]
 *     7. PA2  ---> Pot Right       (ADC2) [Auxiliary]
 *
 *   DIGITAL (6 Active Channels):
 *     1. PB4  ---> Toggle Left     (Active-LOW  : LOW = ON,  HIGH = OFF)
 *     2. PB5  ---> Toggle Right    (Active-LOW  : LOW = ON,  HIGH = OFF)
 *     3. PA15 ---> Push Left       (Active-HIGH : HIGH = PRESSED, LOW = RELEASED) [NC]
 *     4. PB3  ---> Push Right      (Active-HIGH : HIGH = PRESSED, LOW = RELEASED) [NC]
 *     5. PA11 ---> Left Stick Click(Active-LOW  : LOW = PRESSED, HIGH = UP)
 *     6. PA12 ---> Right Stick Click(Active-LOW : LOW = PRESSED, HIGH = UP)
 *
 *   ACTUATORS & INDICATORS:
 *     - PB8   ---> Piezo Buzzer
 *     - PB9   ---> Haptic Vibration Motor (via Transistor/MOSFET)
 *     - PC13  ---> Onboard Heartbeat LED (Active-LOW)
 *
 *   NRF24L01+ RF TRANSCEIVER (Native SPI1):
 *     - PA5   ---> SCK   (Hardware SPI1 Clock)
 *     - PA6   ---> MISO  (Hardware SPI1 Master-In-Slave-Out)
 *     - PA7   ---> MOSI  (Hardware SPI1 Master-Out-Slave-In)
 *     - PB10  ---> CE    (Chip Enable)
 *     - PB11  ---> CSN   (Chip Select Not)
 *     - 3.3V  ---> VCC   (with 10uF - 100uF capacitor to GND)
 *     - GND   ---> GND
 * ================================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// =============================================================================
//  PIN DEFINITIONS
// =============================================================================

// 7 Analog Pins
#define PIN_STICK_LX    PB1
#define PIN_STICK_LY    PB0
#define PIN_STICK_RX    PA4
#define PIN_STICK_RY    PA3
#define PIN_POT_LEFT    PA0
#define PIN_POT_CENTER  PA1
#define PIN_POT_RIGHT   PA2

// 6 Active Digital Pins
#define PIN_TOGGLE_LEFT  PB4
#define PIN_TOGGLE_RIGHT PB5
#define PIN_PUSH_LEFT    PA15
#define PIN_PUSH_RIGHT   PB3
#define PIN_STICK_CLK_L  PA11
#define PIN_STICK_CLK_R  PA12

// Actuator Pins
#define PIN_BUZZER       PB8
#define PIN_HAPTIC       PB9
#define PIN_LED_HB       PC13

// NRF24L01 Control Pins
#define PIN_NRF_CE       PB10
#define PIN_NRF_CSN      PB11

// =============================================================================
//  RADIO & DATA PACKET DEFINITIONS
// =============================================================================
const uint8_t  RF_CHANNEL = 108;            // 2.508 GHz (Above Wi-Fi channels 1-13)
const uint64_t RF_PIPE    = 0xE8E8F0F0E1LL; // CyberRover X5 Link Address

struct __attribute__((packed)) RoverPacket {
  uint16_t stick_lx;    // Left Stick X   (0 - 4095)
  uint16_t stick_ly;    // Left Stick Y   (0 - 4095)
  uint16_t stick_rx;    // Right Stick X  (0 - 4095, Inverted)
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

RoverPacket txPacket;
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
bool radioReady = false;

// =============================================================================
//  DIGITAL CONTROL TRACKING
// =============================================================================
struct DigitalInput {
  uint8_t pin;
  const char* name;
  const char* tag;
  bool isToggle;
  bool activeLevel;
  bool state;
};

DigitalInput digInputs[] = {
  {PIN_TOGGLE_LEFT,  "Toggle Left  (PB4) ", "TL",  true,  LOW,  false},
  {PIN_TOGGLE_RIGHT, "Toggle Right (PB5) ", "TR",  true,  LOW,  false},
  {PIN_PUSH_LEFT,    "Push Left    (PA15)", "PL",  false, HIGH, false},
  {PIN_PUSH_RIGHT,   "Push Right   (PB3) ", "PR",  false, HIGH, false},
  {PIN_STICK_CLK_L,  "Stick Click L(PA11)", "CKL", false, LOW,  false},
  {PIN_STICK_CLK_R,  "Stick Click R(PA12)", "CKR", false, LOW,  false}
};

const uint8_t NUM_DIG_INPUTS = sizeof(digInputs) / sizeof(digInputs[0]);

// =============================================================================
//  POTENTIOMETER TRACKING (Left, Center, Right)
// =============================================================================
struct PotTracker {
  uint8_t pin;
  int lastReading;
  int lastNotch;
};

PotTracker potTrackers[3] = {
  {PIN_POT_LEFT,   -1, -1}, // Speed Governor
  {PIN_POT_CENTER, -1, -1}, // Camera Tilt
  {PIN_POT_RIGHT,  -1, -1}  // Auxiliary
};

// =============================================================================
//  HIGH-EFFICIENCY PHASE-DRIVEN AUDIO-HAPTIC ENGINE
// =============================================================================
enum AlertEvent {
  ALERT_NONE,
  ALERT_LINK_HANDSHAKE,  // 1. Sharp Double-Thud (60ms/70ms) + Victory Double-Chirp
  ALERT_TOGGLE_ON,       // 2. Solid 75ms Punch + High crisp pip (30ms)
  ALERT_TOGGLE_OFF,      // 3. Smooth 50ms Tap + Lower descending pip (20ms)
  ALERT_BUTTON_CLICK,    // 4. Punchy 70ms Thud + Sharp click tone (30ms)
  ALERT_POT_NOTCH,       // 5. Crisp 40ms Tick + Micro audio click (12ms)
  ALERT_TURBO_KICK,      // 6. 120ms Power Surge + Silent (pure vibration)
  ALERT_LINK_LOST        // 7. Heartbeat rumble + Double warning beep
};

AlertEvent currentAlert   = ALERT_NONE;
uint32_t   alertStartTime = 0;
uint8_t    alertPhase     = 0; // State-machine phase tracker (eliminates redundant GPIO writes)

void setOutputs(bool buzzer, bool haptic) {
  digitalWrite(PIN_BUZZER, buzzer ? HIGH : LOW);
  digitalWrite(PIN_HAPTIC, haptic ? HIGH : LOW);
}

// 1. Clean Professional Startup Beep & Haptic Confirmation (80ms)
void playStartupBeep() {
  Serial.println(F(">>> [SOUND] System Ready Beep..."));
  digitalWrite(PIN_BUZZER, HIGH);
  digitalWrite(PIN_HAPTIC, HIGH);
  delay(80); // Crisp 80ms industrial ready chirp
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_HAPTIC, LOW);
}

void triggerAlert(AlertEvent evt) {
  // Prevent low-priority notifications from interrupting Link Handshake
  if (currentAlert == ALERT_LINK_HANDSHAKE && (evt == ALERT_POT_NOTCH || evt == ALERT_TURBO_KICK)) {
    return;
  }

  currentAlert   = evt;
  alertStartTime = millis();
  alertPhase     = 0;

  // Immediate Phase 0 Setup
  switch (evt) {
    case ALERT_LINK_HANDSHAKE:
    case ALERT_TOGGLE_ON:
    case ALERT_TOGGLE_OFF:
    case ALERT_BUTTON_CLICK:
    case ALERT_POT_NOTCH:
    case ALERT_LINK_LOST:
      setOutputs(true, true);
      break;

    case ALERT_TURBO_KICK:
      setOutputs(false, true); // Silent pure vibration
      break;

    default:
      setOutputs(false, false);
      break;
  }
}

// Background Phase-Driven State Machine (Executes only on phase boundaries)
void updateAlertEngine() {
  if (currentAlert == ALERT_NONE) return;
  uint32_t elapsed = millis() - alertStartTime;

  switch (currentAlert) {
    // 2. RF Link Handshake: Sharp Double-Thud (60ms/70ms) + Victory Double-Chirp
    case ALERT_LINK_HANDSHAKE:
      if (alertPhase == 0 && elapsed >= 60) {
        setOutputs(false, false); // Gap (60 - 95ms)
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 95) {
        setOutputs(true, true);   // Thud 2 (95 - 165ms = 70ms)
        alertPhase = 2;
      } else if (alertPhase == 2 && elapsed >= 165) {
        setOutputs(false, false); // Complete
        currentAlert = ALERT_NONE;
      }
      break;

    // 3. Toggle Switch ON: Solid 75ms Punch + High crisp pip (30ms)
    case ALERT_TOGGLE_ON:
      if (alertPhase == 0 && elapsed >= 30) {
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 75) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    // 4. Toggle Switch OFF: Smooth 50ms Tap + Lower pip (20ms)
    case ALERT_TOGGLE_OFF:
      if (alertPhase == 0 && elapsed >= 20) {
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 50) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    // 5. Push Button / Click: Punchy 70ms Thud + Sharp click tone (30ms)
    case ALERT_BUTTON_CLICK:
      if (alertPhase == 0 && elapsed >= 30) {
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 70) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    // 6. Speed Governor / Potentiometer "Notches": Crisp 40ms Tick + Micro click (12ms)
    case ALERT_POT_NOTCH:
      if (alertPhase == 0 && elapsed >= 12) {
        digitalWrite(PIN_BUZZER, LOW);
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 40) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    // 7. Full Throttle Turbo Kick: 120ms Power Surge + Silent (pure vibration)
    case ALERT_TURBO_KICK:
      if (elapsed >= 120) {
        digitalWrite(PIN_HAPTIC, LOW);
        currentAlert = ALERT_NONE;
      }
      break;

    // 8. Signal Loss Warning: Heartbeat rumble + Double warning beep
    case ALERT_LINK_LOST:
      if (alertPhase == 0 && elapsed >= 50) {
        setOutputs(false, false); // Beep/Rumble 1 ends
        alertPhase = 1;
      } else if (alertPhase == 1 && elapsed >= 85) {
        setOutputs(true, true);   // Beep/Rumble 2 starts
        alertPhase = 2;
      } else if (alertPhase == 2 && elapsed >= 135) {
        setOutputs(false, false); // Complete
        currentAlert = ALERT_NONE;
      }
      break;

    default:
      setOutputs(false, false);
      currentAlert = ALERT_NONE;
      break;
  }
}

// =============================================================================
//  JTAG RELEASE ROUTINE (Frees PA15, PB3, PB4 as normal GPIO)
// =============================================================================
void releaseJTAG() {
  #if defined(__HAL_RCC_AFIO_CLK_ENABLE) && defined(__HAL_AFIO_REMAP_SWJ_NOJTAG)
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); // Keeps SWD (PA13/PA14) alive, frees PA15, PB3, PB4
  #elif defined(AFIO_MAPR_SWJ_CFG_JTAGDISABLE)
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
  #elif defined(disableDebugPorts)
    disableDebugPorts();
  #endif
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Release JTAG to allow PB4, PB3, PA15 to act as standard digital GPIO
  releaseJTAG();

  // 2. Configure 12-bit ADC resolution (0 - 4095)
  #if defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__)
    analogReadResolution(12);
  #endif

  // 3. Configure Digital Inputs with internal pull-ups
  for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++) {
    pinMode(digInputs[i].pin, INPUT_PULLUP);
    bool raw = digitalRead(digInputs[i].pin);
    digInputs[i].state = (raw == digInputs[i].activeLevel);
  }

  // 4. Configure Actuator & Heartbeat Pins
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_HAPTIC, OUTPUT);
  pinMode(PIN_LED_HB, OUTPUT);

  setOutputs(false, false);
  digitalWrite(PIN_LED_HB, HIGH); // Off for active-low PC13

  // 5. Initialize NRF24L01 Module
  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — 13-CHANNEL REMOTE TRANSMITTER (OPTIMIZED OS v1.2)             "));
  Serial.println(F("  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           "));
  Serial.println(F("================================================================================"));
  Serial.print(F(">>> Initializing NRF24L01+ Radio on SPI1... "));

  // Explicitly initialize SPI1 bus
  SPI.begin();

  if (radio.begin()) {
    radio.setPALevel(RF24_PA_LOW);           // Bench power (safe from brownout)
    radio.setDataRate(RF24_250KBPS);         // 250 kbps for max range & noise rejection
    radio.setChannel(RF_CHANNEL);            // Channel 108 (2.508 GHz)
    radio.setRetries(5, 15);                 // 1500us delay, 15 retries (maximum RF reliability)
    radio.setPayloadSize(sizeof(RoverPacket));
    radio.openWritingPipe(RF_PIPE);
    radio.stopListening();                   // Transmitter mode
    radioReady = true;
    Serial.println(F("[PASSED - SILICON ACTIVE]"));
    Serial.print(F(">>> Broadcasting on Channel "));
    Serial.print(RF_CHANNEL);
    Serial.print(F(" | Speed: 250 kbps | Loop: 40 Hz (25ms) | Payload: "));
    Serial.print(sizeof(RoverPacket));
    Serial.println(F(" Bytes"));
  } else {
    Serial.println(F("[FAILED!] Check 3.3V power, 10uF cap, and SPI1 wiring!"));
    radioReady = false;
  }

  Serial.println(F("================================================================================"));
  Serial.println(F("System Ready! Live transmission & stream running below (115200 baud)...         "));
  Serial.println();

  // Trigger Clean Startup Beep (80ms industrial confirmation)
  playStartupBeep();
}

// =============================================================================
//  MAIN LOOP
// =============================================================================
uint32_t lastTxTime        = 0;
uint32_t lastPrintTime     = 0;
uint32_t lastHeartbeat     = 0;
uint32_t frameCounter      = 0;
uint32_t lastLinkLossAlert = 0;
uint32_t lastAckSuccess    = 0;

bool lastTxAck             = false;
bool wasLinkConnected      = false;
bool turboActive           = false;

void loop() {
  uint32_t now = millis();

  // Update non-blocking Audio-Haptic Engine on every pass
  updateAlertEngine();

  // ====================================================================
  //  1. CHECK DIGITAL EVENTS (Toggles, Buttons, Gimbal Clicks)
  // ====================================================================
  for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++) {
    bool raw = digitalRead(digInputs[i].pin);
    bool isCurrentActive = (raw == digInputs[i].activeLevel);

    if (isCurrentActive != digInputs[i].state) {
      digInputs[i].state = isCurrentActive;

      // Event 3 & 4: Toggle Switch ON / OFF
      if (digInputs[i].isToggle) {
        if (isCurrentActive) {
          triggerAlert(ALERT_TOGGLE_ON);   // Solid 75ms Punch + High crisp pip (30ms)
        } else {
          triggerAlert(ALERT_TOGGLE_OFF);  // Smooth 50ms Tap + Lower pip (20ms)
        }
      } 
      // Event 5: Push Button / Stick Click
      else {
        if (isCurrentActive) {
          triggerAlert(ALERT_BUTTON_CLICK);// Punchy 70ms Thud + Sharp click tone
        }
      }

      Serial.print(F(">>> [EVENT] "));
      Serial.print(digInputs[i].name);
      Serial.print(F(" -> "));
      Serial.println(isCurrentActive ? F("[ ACTIVE / ON  ]") : F("[ IDLE / OFF ]"));
    }
  }

  // ====================================================================
  //  2. READ INPUTS & TRANSMIT RF PACKET (Every 25ms = 40 Hz)
  // ====================================================================
  if (now - lastTxTime >= 25) {
    lastTxTime = now;

    // Read Joysticks (12-bit: 0 - 4095)
    txPacket.stick_lx = analogRead(PIN_STICK_LX);
    txPacket.stick_ly = analogRead(PIN_STICK_LY);
    int raw_rx        = analogRead(PIN_STICK_RX);
    txPacket.stick_rx = (raw_rx <= 4095) ? (4095 - raw_rx) : 0; // Inverted for 90° gimbal mount
    txPacket.stick_ry = analogRead(PIN_STICK_RY);

    // Read 3 Potentiometers
    txPacket.pot_l    = analogRead(PIN_POT_LEFT);
    txPacket.pot_c    = analogRead(PIN_POT_CENTER);
    txPacket.pot_r    = analogRead(PIN_POT_RIGHT);

    // ------------------------------------------------------------------
    // Event 6: Haptic Notches for ALL 3 Potentiometers (Left, Center, Right)
    // ------------------------------------------------------------------
    uint16_t potValues[3] = { txPacket.pot_l, txPacket.pot_c, txPacket.pot_r };
    for (uint8_t p = 0; p < 3; p++) {
      int notch = potValues[p] / 205; // 20 discrete notches (Exactly 5% per notch: 5%, 10%, 15%... 100%)
      
      if (potTrackers[p].lastNotch == -1) {
        potTrackers[p].lastNotch   = notch;
        potTrackers[p].lastReading = potValues[p];
      } else {
        // Hysteresis deadband filter (>65 ADC counts prevents hand jitter while dialing 5% steps)
        if (notch != potTrackers[p].lastNotch && abs((int)potValues[p] - potTrackers[p].lastReading) > 65) {
          potTrackers[p].lastNotch   = notch;
          potTrackers[p].lastReading = potValues[p];
          triggerAlert(ALERT_POT_NOTCH); // Crisp 40ms Tick + Micro audio click
        }
      }
    }

    // ------------------------------------------------------------------
    // Event 7: Full Throttle Turbo Kick (120ms Power Surge past 90% throttle)
    // ------------------------------------------------------------------
    bool isInTurboZone = (txPacket.stick_ly > 3750 || txPacket.stick_ly < 350);
    if (isInTurboZone) {
      if (!turboActive) {
        turboActive = true;
        triggerAlert(ALERT_TURBO_KICK); // 120ms Power Surge (Silent pure vibration)
      }
    } else if (txPacket.stick_ly > 700 && txPacket.stick_ly < 3400) {
      turboActive = false; // Reset latch when stick returns closer to center
    }

    // Read 6 Digital Channels (Mapped from calibrated digInputs array)
    txPacket.toggle_l    = digInputs[0].state ? 1 : 0;
    txPacket.toggle_r    = digInputs[1].state ? 1 : 0;
    txPacket.push_l      = digInputs[2].state ? 1 : 0;
    txPacket.push_r      = digInputs[3].state ? 1 : 0;
    txPacket.stick_clk_l = digInputs[4].state ? 1 : 0;
    txPacket.stick_clk_r = digInputs[5].state ? 1 : 0;

    txPacket.frame_id    = ++frameCounter;

    // Transmit over NRF24L01+
    if (radioReady) {
      lastTxAck = radio.write(&txPacket, sizeof(RoverPacket));

      if (lastTxAck) {
        lastAckSuccess = now;

        // Event 2: RF Link Handshake (First connect or re-connect)
        if (!wasLinkConnected) {
          wasLinkConnected = true;
          triggerAlert(ALERT_LINK_HANDSHAKE); // Sharp Double-Thud + Victory Double-Chirp
          Serial.println(F("\n>>> [RF LINK ESTABLISHED] Paired to CyberRover X5! <<<\n"));
        }
      } else {
        // Event 8: Signal Loss Warning (Repeats every 2.5s if no ACK for >1000ms)
        if (wasLinkConnected && (now - lastAckSuccess > 1000)) {
          if (now - lastLinkLossAlert >= 2500) {
            lastLinkLossAlert = now;
            triggerAlert(ALERT_LINK_LOST); // Heartbeat rumble + Double warning beep
          }
        }
      }
    }
  }

  // ====================================================================
  //  3. PERIODIC LIVE TELEMETRY STREAM (Every 100ms = 10 Hz)
  // ====================================================================
  if (now - lastPrintTime >= 100) {
    lastPrintTime = now;

    // Frame & RF status
    Serial.print(F("[FRM #"));
    Serial.print(txPacket.frame_id);
    Serial.print(F(" | RF:"));
    if (radioReady) {
      Serial.print(lastTxAck ? F("ACK_OK") : F("TX_NO_ACK"));
    } else {
      Serial.print(F("OFFLINE"));
    }
    Serial.print(F("] "));

    // Joysticks
    Serial.print(F("LX:"));
    Serial.print(txPacket.stick_lx);
    Serial.print(F(" LY:"));
    Serial.print(txPacket.stick_ly);
    Serial.print(F(" | RX:"));
    Serial.print(txPacket.stick_rx);
    Serial.print(F(" RY:"));
    Serial.print(txPacket.stick_ry);

    // Potentiometers
    Serial.print(F(" || POTS: [L:"));
    Serial.print(txPacket.pot_l);
    Serial.print(F(" C:"));
    Serial.print(txPacket.pot_c);
    Serial.print(F(" R:"));
    Serial.print(txPacket.pot_r);
    Serial.print(F("]"));

    // Digital Inputs
    Serial.print(F(" || DIG: "));
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++) {
      Serial.print(digInputs[i].tag);
      Serial.print(F(":"));
      Serial.print(digInputs[i].state ? F("[ON] ") : F("[--] "));
    }

    Serial.println();
  }

  // ====================================================================
  //  4. HEARTBEAT LED (1 Hz blink)
  // ====================================================================
  if (now - lastHeartbeat >= 500) {
    lastHeartbeat = now;
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
  }
}
