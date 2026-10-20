/*
 * ================================================================================
 * CYBERROVER X5 — INDUSTRIAL-GRADE MASTER TRANSMITTER FIRMWARE (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Platform       : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Handheld Master Telemetry & Command Unit
 * Architecture   : Modular, Deterministic Multi-Rate Non-Blocking State Machine
 *
 * SAFETY & RESILIENCE GUARANTEES:
 *   1. Hardware Independent Watchdog (IWDG): Automatically catches software lockups.
 *   2. 3-Stage Analog Conditioning: Median Filter + Low-Pass IIR + Deadband Normalization.
 *   3. Potentiometer Boundary Hysteresis: Eliminates boundary chatter and false alerts.
 *   4. Priority-Arbitrated Audio-Haptic Engine: Safety alerts pre-empt tactile clicks.
 *   5. Sound Mute Feature: Toggle Right (PB4) instantly silences the piezo buzzer.
 *   6. Little-Endian 24-Byte Validated Protocol: Enforced via compile-time assertions.
 * ================================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "cyberrover_protocol.h"
#include "cyberrover_config.h"
#include "analog_filter.h"
#include "digital_inputs.h"
#include "alert_engine.h"
#include "radio_tx_manager.h"
#include "watchdog_mgr.h"

// =============================================================================
//  DISPLAY DRIVER INSTANCE (400 kHz Fast Hardware I2C1)
// =============================================================================
#if USE_SH1106
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

// =============================================================================
//  SUBSYSTEM INSTANCES
// =============================================================================
static AnalogChannelFilter filter_stick_lx;
static AnalogChannelFilter filter_stick_ly;
static AnalogChannelFilter filter_stick_rx;
static AnalogChannelFilter filter_stick_ry;
static AnalogChannelFilter filter_pot_l;
static AnalogChannelFilter filter_pot_c;
static AnalogChannelFilter filter_pot_r;

static PotNotchTracker     notch_pot_l;
static PotNotchTracker     notch_pot_c;
static PotNotchTracker     notch_pot_r;

static DigitalInputChannel digInputs[6];
static AlertEngine         alertEngine;
static RadioTxManager      radioManager;
static WatchdogManager     watchdogMgr;

static RoverPacket         txPacket;
static uint32_t            frameCounter = 0;
static bool                turboActive  = false;

// Scheduling Timers (Monotonic Rollover-Safe)
static uint32_t lastTxTime        = 0;
static uint32_t lastOledTime      = 0;
static uint32_t lastTelemetryTime = 0;
static uint32_t lastHeartbeatTime = 0;

// OLED Gauge Smoothing States
static float speed_smooth = 0.0f;
static float rpm_smooth   = 0.0f;
static float fuel_smooth  = 88.0f;

// =============================================================================
//  BITMAPPED GRAPHICS ASSETS (Virtual Cockpit HUD)
// =============================================================================
static const unsigned char PROGMEM bitmap_temp_icon[] = {
  0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x38, 0x00, 0x38, 0x00,
  0x54, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0x7C, 0x00
};

static const unsigned char PROGMEM bitmap_fuel_icon[] = {
  0x70, 0x5A, 0x7A, 0x5E, 0x52, 0x72, 0x70, 0xFE
};

// =============================================================================
//  JTAG RELEASE ROUTINE (Frees PA15, PB3, PB4 while preserving SWD PA13/PA14)
// =============================================================================
static void releaseJTAG() {
#if defined(__HAL_RCC_AFIO_CLK_ENABLE) && defined(__HAL_AFIO_REMAP_SWJ_NOJTAG)
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_SWJ_NOJTAG(); // SWJ_CFG = 010 (JTAG-DP Disabled, SW-DP Enabled)
#elif defined(AFIO_MAPR_SWJ_CFG_JTAGDISABLE)
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
#elif defined(disableDebugPorts)
  disableDebugPorts();
#endif
}

// =============================================================================
//  OLED INSTRUMENT CLUSTER DRAWING PRIMITIVES
// =============================================================================
static void drawThickArc(int cx, int cy, int r_in, int r_out, float start_deg, float end_deg) {
  if (start_deg > end_deg) {
    float temp = start_deg;
    start_deg = end_deg;
    end_deg = temp;
  }
  for (float a = start_deg; a <= end_deg; a += 1.0f) {
    float rad = a * 0.0174532925f;
    float cos_a = cos(rad);
    float sin_a = sin(rad);
    int x1 = cx + (int)(r_in  * cos_a + 0.5f);
    int y1 = cy - (int)(r_in  * sin_a + 0.5f);
    int x2 = cx + (int)(r_out * cos_a + 0.5f);
    int y2 = cy - (int)(r_out * sin_a + 0.5f);
    u8g2.drawLine(x1, y1, x2, y2);
  }
}

static void drawDialTrack(int cx, int cy, int r_track, float start_deg, float end_deg) {
  for (float a = start_deg; a <= end_deg; a += 10.0f) {
    float rad = a * 0.0174532925f;
    float cos_a = cos(rad);
    float sin_a = sin(rad);
    int step_idx = (int)((a - start_deg) / 10.0f + 0.5f);
    if (step_idx % 3 == 0) {
      int x_t1 = cx + (int)((r_track - 1) * cos_a + 0.5f);
      int y_t1 = cy - (int)((r_track - 1) * sin_a + 0.5f);
      int x_t2 = cx + (int)((r_track + 2) * cos_a + 0.5f);
      int y_t2 = cy - (int)((r_track + 2) * sin_a + 0.5f);
      u8g2.drawLine(x_t1, y_t1, x_t2, y_t2);
    } else {
      int x_dot = cx + (int)(r_track * cos_a + 0.5f);
      int y_dot = cy - (int)(r_track * sin_a + 0.5f);
      u8g2.drawPixel(x_dot, y_dot);
    }
  }
}

static void renderDashboardHUD(char drive_mode, float governor_factor, uint16_t pot_c, uint16_t pot_r,
                               bool blinker_l, bool blinker_r, bool blink_state, bool rf_connected, bool is_muted) {
  u8g2.clearBuffer();

  // ---------------------------------------------------------------------------
  // 1. LEFT GAUGE: SPEEDOMETER (cm/s)
  // ---------------------------------------------------------------------------
  const int cx_l = 30;
  const int cy_l = 34;
  const int r_in_l = 21;
  const int r_out_l = 24;
  const int r_track_l = 26;

  drawDialTrack(cx_l, cy_l, r_track_l, 35.0f, 215.0f);

  float speed_frac = speed_smooth / (float)MAX_SPEED_CMPS;
  if (speed_frac > 1.0f) speed_frac = 1.0f;
  if (speed_frac < 0.03f) speed_frac = 0.03f;

  float speed_current_deg = 215.0f - (speed_frac * 180.0f);
  drawThickArc(cx_l, cy_l, r_in_l, r_out_l, speed_current_deg, 215.0f);

  // Governor Limit Marker
  float gov_deg = 215.0f - (governor_factor * 180.0f);
  float gov_rad = gov_deg * 0.0174532925f;
  int gx1 = cx_l + (int)(25 * cos(gov_rad) + 0.5f);
  int gy1 = cy_l - (int)(25 * sin(gov_rad) + 0.5f);
  int gx2 = cx_l + (int)(29 * cos(gov_rad) + 0.5f);
  int gy2 = cy_l - (int)(29 * sin(gov_rad) + 0.5f);
  u8g2.drawLine(gx1, gy1, gx2, gy2);

  int display_speed_val = (int)(speed_smooth + 0.5f);
  char speed_str[8];
  itoa(display_speed_val, speed_str, 10);
  u8g2.setFont(u8g2_font_helvB14_tr);
  int speed_str_w = u8g2.getStrWidth(speed_str);
  u8g2.drawStr(cx_l - (speed_str_w / 2), 38, speed_str);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx_l - 8, 48, "CM/S");

  // Left Blinker Arrow (◄)
  if (blinker_l && blink_state) {
    u8g2.drawTriangle(4, 6, 11, 2, 11, 10);
  } else {
    u8g2.drawPixel(4, 6);
    u8g2.drawPixel(11, 6);
  }

  // ---------------------------------------------------------------------------
  // 2. RIGHT GAUGE: MOTOR TACHOMETER (RPM)
  // ---------------------------------------------------------------------------
  const int cx_r = 98;
  const int cy_r = 34;
  const int r_in_r = 21;
  const int r_out_r = 24;
  const int r_track_r = 26;

  drawDialTrack(cx_r, cy_r, r_track_r, -35.0f, 145.0f);

  float rpm_frac = rpm_smooth / (float)MAX_MOTOR_RPM;
  if (rpm_frac > 1.0f) rpm_frac = 1.0f;
  if (rpm_frac < 0.03f) rpm_frac = 0.03f;

  float rpm_current_deg = -35.0f + (rpm_frac * 180.0f);
  drawThickArc(cx_r, cy_r, r_in_r, r_out_r, -35.0f, rpm_current_deg);

  int display_rpm_val = (int)(rpm_smooth + 0.5f);
  char rpm_str[8];
  itoa(display_rpm_val, rpm_str, 10);
  u8g2.setFont(u8g2_font_helvB14_tr);
  int rpm_str_w = u8g2.getStrWidth(rpm_str);
  u8g2.drawStr(cx_r - (rpm_str_w / 2), 38, rpm_str);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx_r - 6, 48, "RPM");

  // Right Blinker Arrow (►)
  if (blinker_r && blink_state) {
    u8g2.drawTriangle(123, 6, 116, 2, 116, 10);
  } else {
    u8g2.drawPixel(116, 6);
    u8g2.drawPixel(123, 6);
  }

  // ---------------------------------------------------------------------------
  // 3. CENTER COLUMN: STATUS & TELEMETRY HUD
  // ---------------------------------------------------------------------------
  u8g2.drawXBMP(60, 4, 9, 9, bitmap_temp_icon);
  u8g2.drawPixel(58, 2);
  u8g2.drawPixel(64, 1);
  u8g2.drawPixel(70, 2);

  if (rf_connected) {
    u8g2.drawDisc(64, 15, 1);
  } else if (blink_state) {
    u8g2.drawCircle(64, 15, 1);
  }

  // Drive Mode Indicator
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(62, 29, ":");

  char mode_str[2] = { drive_mode, '\0' };
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(61, 39, mode_str);

  // Mute Status Icon (Small 'M' badge when muted)
  if (is_muted) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(62, 21, "M");
  }

  // Dynamic Power Hashes
  int num_hashes = (int)(speed_frac * 4.0f + 0.5f);
  if (num_hashes < 1) num_hashes = 1;
  for (int h = 0; h < num_hashes; h++) {
    int hx = 72 + h * 3;
    int hy = 43 - h * 2;
    u8g2.drawLine(hx, hy, hx + 2, hy - 4);
  }

  // Fuel Gauge
  u8g2.drawXBMP(60, 46, 8, 8, bitmap_fuel_icon);
  const int cx_fuel = 64;
  const int cy_fuel = 40;
  const int r_fuel  = 20;

  for (float fa = 245.0f; fa <= 295.0f; fa += 2.0f) {
    float frad = fa * 0.0174532925f;
    int fx = cx_fuel + (int)(r_fuel * cos(frad) + 0.5f);
    int fy = cy_fuel - (int)(r_fuel * sin(frad) + 0.5f);
    u8g2.drawPixel(fx, fy);
  }
  u8g2.drawLine(52, 57, 52, 60);
  u8g2.drawLine(76, 57, 76, 60);

  float fuel_frac = fuel_smooth / 100.0f;
  if (fuel_frac > 1.0f) fuel_frac = 1.0f;
  if (fuel_frac < 0.0f) fuel_frac = 0.0f;

  float fuel_fill_deg = 245.0f + (fuel_frac * 50.0f);
  for (float fa = 245.0f; fa <= fuel_fill_deg; fa += 1.0f) {
    float frad = fa * 0.0174532925f;
    float cos_fa = cos(frad);
    float sin_fa = sin(frad);
    int fx1 = cx_fuel + (int)((r_fuel - 1) * cos_fa + 0.5f);
    int fy1 = cy_fuel - (int)((r_fuel - 1) * sin_fa + 0.5f);
    int fx2 = cx_fuel + (int)((r_fuel + 1) * cos_fa + 0.5f);
    int fy2 = cy_fuel - (int)((r_fuel + 1) * sin_fa + 0.5f);
    u8g2.drawLine(fx1, fy1, fx2, fy2);
  }

  // ---------------------------------------------------------------------------
  // 4. LOWER CORNER BARS: POT CENTER & POT RIGHT INDICATORS
  // ---------------------------------------------------------------------------
  u8g2.drawHLine(2, 63, 37);
  u8g2.drawVLine(38, 61, 3);
  int fill_c = (int)(((float)pot_c / 4095.0f) * 36.0f + 0.5f);
  if (fill_c > 36) fill_c = 36;
  if (fill_c > 0) u8g2.drawBox(2, 61, fill_c, 2);

  u8g2.drawHLine(89, 63, 37);
  u8g2.drawVLine(89, 61, 3);
  int fill_r = (int)(((float)pot_r / 4095.0f) * 36.0f + 0.5f);
  if (fill_r > 36) fill_r = 36;
  if (fill_r > 0) u8g2.drawBox(126 - fill_r, 61, fill_r, 2);

  u8g2.sendBuffer();
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(400);

  // 1. Hardware Pin Remap: Release JTAG to free PA15, PB3, PB4
  releaseJTAG();

  // 2. Configure 12-Bit ADC Resolution on STM32
  #if defined(analogReadResolution)
    analogReadResolution(12);
  #endif

  // 3. Initialize Analog Channel Filters
  // Joysticks: fast response alpha = 0.70, deadband = 140
  filter_stick_lx.configure(0.70f, false, ADC_DEFAULT_DEADBAND, ADC_NOMINAL_CENTER);
  filter_stick_ly.configure(0.70f, false, ADC_DEFAULT_DEADBAND, ADC_NOMINAL_CENTER);
  filter_stick_rx.configure(0.70f, true,  ADC_DEFAULT_DEADBAND, ADC_NOMINAL_CENTER); // Inverted for 90° mount
  filter_stick_ry.configure(0.70f, false, ADC_DEFAULT_DEADBAND, ADC_NOMINAL_CENTER);

  // Potentiometers: smooth response alpha = 0.25, zero deadband
  filter_pot_l.configure(0.25f, false, 0, 0);
  filter_pot_c.configure(0.25f, false, 0, 0);
  filter_pot_r.configure(0.25f, false, 0, 0);

  notch_pot_l.configure(20, 65);
  notch_pot_c.configure(20, 65);
  notch_pot_r.configure(20, 65);

  // 4. Configure Digital Channels
  DigitalConfig digConfigs[6] = {
    {PIN_TOGGLE_LEFT,  "TL",  LOW,  true,  20}, // Master Arm/Disarm
    {PIN_TOGGLE_RIGHT, "TR",  LOW,  true,  20}, // MUTE Switch (Veer requirement)
    {PIN_PUSH_LEFT,    "PL",  HIGH, false, 20},
    {PIN_PUSH_RIGHT,   "PR",  HIGH, false, 20},
    {PIN_STICK_CLK_L,  "CKL", LOW,  false, 20},
    {PIN_STICK_CLK_R,  "CKR", LOW,  false, 20}
  };

  for (uint8_t i = 0; i < 6; i++) {
    digInputs[i].init(digConfigs[i]);
    // Use pull-up for active-LOW pins, standard input for active-HIGH pins
    if (digConfigs[i].activeLevel == LOW) {
      pinMode(digConfigs[i].pin, INPUT_PULLUP);
    } else {
      pinMode(digConfigs[i].pin, INPUT);
    }
  }

  // 5. Initialize Alert Engine & Indicators
  pinMode(PIN_LED_HB, OUTPUT);
  digitalWrite(PIN_LED_HB, HIGH); // Off for active-low PC13
  alertEngine.init(PIN_BUZZER, PIN_HAPTIC);

  // 6. Initialize I2C OLED HUD
  Wire.setClock(400000);
  u8g2.begin();

  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("  CYBERROVER X5 — INDUSTRIAL-GRADE MASTER TRANSMITTER (FIRMWARE v1.5)           "));
  Serial.println(F("  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           "));
  Serial.println(F("================================================================================"));

  // 7. Initialize NRF24L01+ Radio Driver
  Serial.print(F(">>> Initializing NRF24L01+ Transceiver on Hardware SPI1... "));
  if (radioManager.init(PIN_NRF_CE, PIN_NRF_CSN, CYBERROVER_RF_CHANNEL, CYBERROVER_RF_PIPE)) {
    Serial.println(F("[PASSED - SILICON OK]"));
    Serial.print(F(">>> Operating Channel: "));
    Serial.print(CYBERROVER_RF_CHANNEL);
    Serial.print(F(" (2.508 GHz) | Rate: 250 kbps | Payload: "));
    Serial.print(sizeof(RoverPacket));
    Serial.println(F(" Bytes"));
  } else {
    Serial.println(F("[FAILED - CHECK HARDWARE SPI & POWER]"));
  }

  // 8. Initialize Hardware Independent Watchdog (1500ms timeout)
  Serial.print(F(">>> Initializing STM32 Hardware IWDG Watchdog (1500ms)... "));
  if (watchdogMgr.init(IWDG_TIMEOUT_MS)) {
    Serial.println(F("[ENABLED]"));
    if (watchdogMgr.wasResetByWatchdog()) {
      Serial.println(F(">>> [SAFETY NOTICE] System recovered from previous Hardware Watchdog reset! <<<"));
    }
  } else {
    Serial.println(F("[SIMULATION / NOT SUPPORTED ON CORE]"));
  }

  // Play startup confirmation alert
  alertEngine.trigger(ALERT_LINK_HANDSHAKE);
}

// =============================================================================
//  MAIN REAL-TIME LOOP
// =============================================================================
void loop() {
  uint32_t now = millis();

  // ---------------------------------------------------------------------------
  // 1. UPDATE DIGITAL INPUTS & HANDLE MUTE STATE
  // ---------------------------------------------------------------------------
  for (uint8_t i = 0; i < 6; i++) {
    bool raw = digitalRead(digInputs[i].getPin());
    digInputs[i].update(now, raw);
  }

  // Toggle Right (PB4) controls Sound Mute
  bool isMuted = digInputs[1].isPressed();
  alertEngine.setMuted(isMuted);

  // Digital Click & Toggle Audio-Haptic Feedback
  if (digInputs[0].hasToggled()) {
    alertEngine.trigger(digInputs[0].isPressed() ? ALERT_TOGGLE_ON : ALERT_TOGGLE_OFF);
  }
  if (digInputs[1].hasToggled()) {
    alertEngine.trigger(isMuted ? ALERT_TOGGLE_OFF : ALERT_TOGGLE_ON);
  }
  if (digInputs[2].justPressed() || digInputs[3].justPressed() ||
      digInputs[4].justPressed() || digInputs[5].justPressed()) {
    alertEngine.trigger(ALERT_BUTTON_CLICK);
  }

  // ---------------------------------------------------------------------------
  // 2. PERIODIC RF CONTROL TRANSMISSION (40 Hz = 25ms interval)
  // ---------------------------------------------------------------------------
  if (now - lastTxTime >= TX_INTERVAL_MS) {
    lastTxTime = now;

    // A. Read and Condition Analog Channels
    txPacket.stick_lx = filter_stick_lx.update(analogRead(PIN_STICK_LX));
    txPacket.stick_ly = filter_stick_ly.update(analogRead(PIN_STICK_LY));
    txPacket.stick_rx = filter_stick_rx.update(analogRead(PIN_STICK_RX));
    txPacket.stick_ry = filter_stick_ry.update(analogRead(PIN_STICK_RY));

    txPacket.pot_l    = filter_pot_l.update(analogRead(PIN_POT_LEFT));
    txPacket.pot_c    = filter_pot_c.update(analogRead(PIN_POT_CENTER));
    txPacket.pot_r    = filter_pot_r.update(analogRead(PIN_POT_RIGHT));

    // B. Potentiometer Notch Transitions with Hysteresis
    int notchVal = 0;
    if (notch_pot_l.update(txPacket.pot_l, notchVal) ||
        notch_pot_c.update(txPacket.pot_c, notchVal) ||
        notch_pot_r.update(txPacket.pot_r, notchVal)) {
      alertEngine.trigger(ALERT_POT_NOTCH);
    }

    // C. Turbo Kick Detection (Past 90% Throttle)
    bool inTurbo = (txPacket.stick_ly > 3750 || txPacket.stick_ly < 350);
    if (inTurbo) {
      if (!turboActive) {
        turboActive = true;
        alertEngine.trigger(ALERT_TURBO_KICK);
      }
    } else if (txPacket.stick_ly > 700 && txPacket.stick_ly < 3400) {
      turboActive = false;
    }

    // D. Pack Digital Channels
    txPacket.toggle_l    = digInputs[0].isPressed() ? 1 : 0; // Arm / Disarm
    txPacket.toggle_r    = isMuted ? 1 : 0;                  // Mute ON / OFF
    txPacket.push_l      = digInputs[2].isPressed() ? 1 : 0;
    txPacket.push_r      = digInputs[3].isPressed() ? 1 : 0;
    txPacket.stick_clk_l = digInputs[4].isPressed() ? 1 : 0;
    txPacket.stick_clk_r = digInputs[5].isPressed() ? 1 : 0;

    txPacket.frame_id    = ++frameCounter;

    // E. Transmit over NRF24L01+
    radioManager.transmit(txPacket, now);

    if (radioManager.justConnected()) {
      alertEngine.trigger(ALERT_LINK_HANDSHAKE);
      Serial.println(F("\n>>> [RF LINK ESTABLISHED] Paired to CyberRover X5! <<<\n"));
    } else if (radioManager.justLost()) {
      alertEngine.trigger(ALERT_LINK_LOST);
      Serial.println(F("\n>>> [ALERT] RF LINK LOST! Rover entered failsafe! <<<\n"));
    }
  }

  // ---------------------------------------------------------------------------
  // 3. DYNAMIC SPEED GOVERNOR & DASHBOARD COMPUTATION
  // ---------------------------------------------------------------------------
  float governor_factor = 0.15f + 0.85f * ((float)txPacket.pot_l / 4095.0f);
  if (governor_factor > 1.0f)  governor_factor = 1.0f;
  if (governor_factor < 0.15f) governor_factor = 0.15f;

  float max_governed_speed = MAX_SPEED_CMPS * governor_factor;
  float max_governed_rpm   = MAX_MOTOR_RPM  * governor_factor;

  float norm_throttle = filter_stick_ly.getNormalizedDeflection();
  bool is_forward = (norm_throttle > 0.0f);
  bool is_reverse = (norm_throttle < 0.0f);
  float abs_throttle = fabsf(norm_throttle);

  float target_speed = abs_throttle * max_governed_speed;
  float target_rpm   = abs_throttle * max_governed_rpm;

  char drive_mode = 'P';
  if (!digInputs[0].isPressed()) {
    drive_mode = 'P'; // Disarmed / Park Mode ('P')
  } else if (is_reverse) {
    drive_mode = 'R'; // Reverse Mode ('R')
  } else {
    drive_mode = 'D'; // Drive Armed ('D')
  }

  // EMA smoothing for smooth gauge needle animation
  speed_smooth += (target_speed - speed_smooth) * 0.22f;
  rpm_smooth   += (target_rpm   - rpm_smooth)   * 0.22f;

  // Steering Blinkers
  bool blinker_l = (txPacket.stick_lx > (ADC_NOMINAL_CENTER + 500));
  bool blinker_r = (txPacket.stick_lx < (ADC_NOMINAL_CENTER - 500));
  bool blink_state = ((now / 250) % 2 == 0);

  // ---------------------------------------------------------------------------
  // 4. NON-BLOCKING OLED DASHBOARD REFRESH (25 FPS = 40ms interval)
  // ---------------------------------------------------------------------------
  if (now - lastOledTime >= OLED_INTERVAL_MS) {
    lastOledTime = now;
    renderDashboardHUD(drive_mode, governor_factor, txPacket.pot_c, txPacket.pot_r,
                       blinker_l, blinker_r, blink_state, radioManager.isLinkConnected(), isMuted);
  }

  // ---------------------------------------------------------------------------
  // 5. PERIODIC LIVE TELEMETRY STREAM TO SERIAL (10 Hz = 100ms interval)
  // ---------------------------------------------------------------------------
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;

    Serial.print(F("[FRM:"));
    Serial.print(txPacket.frame_id);
    Serial.print(F("|"));
    Serial.print(radioManager.getLastTxAck() ? F("ACK") : F("NO_ACK"));
    Serial.print(isMuted ? F("|MUTE") : F("|SND"));
    Serial.print(F("] "));

    Serial.print(F("LX:"));
    Serial.print(txPacket.stick_lx);
    Serial.print(F(" LY:"));
    Serial.print(txPacket.stick_ly);
    Serial.print(F(" RX:"));
    Serial.print(txPacket.stick_rx);
    Serial.print(F(" RY:"));
    Serial.print(txPacket.stick_ry);

    Serial.print(F(" | POT[L:"));
    Serial.print(txPacket.pot_l);
    Serial.print(F(" C:"));
    Serial.print(txPacket.pot_c);
    Serial.print(F(" R:"));
    Serial.print(txPacket.pot_r);
    Serial.print(F("]"));

    Serial.print(F(" | SW[TL:"));
    Serial.print(txPacket.toggle_l);
    Serial.print(F(" TR:"));
    Serial.print(txPacket.toggle_r);
    Serial.print(F(" PL:"));
    Serial.print(txPacket.push_l);
    Serial.print(F(" PR:"));
    Serial.print(txPacket.push_r);
    Serial.print(F(" CKL:"));
    Serial.print(txPacket.stick_clk_l);
    Serial.print(F(" CKR:"));
    Serial.print(txPacket.stick_clk_r);
    Serial.println(F("]"));
  }

  // ---------------------------------------------------------------------------
  // 6. UPDATE AUDIO-HAPTIC FSM & HEARTBEAT LED
  // ---------------------------------------------------------------------------
  alertEngine.update(now);
  radioManager.updateLinkState(now);

  if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatTime = now;
    digitalWrite(PIN_LED_HB, !digitalRead(PIN_LED_HB));
  }

  // ---------------------------------------------------------------------------
  // 7. FEED HARDWARE WATCHDOG
  // ---------------------------------------------------------------------------
  watchdogMgr.reload();
}
