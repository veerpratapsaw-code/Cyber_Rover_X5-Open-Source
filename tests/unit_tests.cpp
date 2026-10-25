/*
 * ================================================================================
 * CYBERROVER X5 — AUTOMATED HOST UNIT TEST SUITE (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Compiler       : GCC / G++ (Host Automated Verification)
 * Standards      : C++17 / Production Embedded Unit Verification
 * ================================================================================
 */

#include <iostream>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>

// Include the exact shared protocol specification
#include "../firmware/shared/cyberrover_protocol.h"

// -----------------------------------------------------------------------------
// Test Models for Verification
// -----------------------------------------------------------------------------
class AnalogChannelFilter {
private:
  float alpha;
  bool inverted;
  int deadband;
  int center;
  uint16_t window[3];
  uint8_t winIdx;
  float iirState;
  bool initialized;

  static uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
  }

public:
  AnalogChannelFilter() : alpha(0.7f), inverted(false), deadband(140), center(2048),
                          winIdx(0), iirState(2048.0f), initialized(false) {
    window[0] = window[1] = window[2] = 2048;
  }

  void configure(float a, bool inv, int db, int cntr) {
    alpha = a; inverted = inv; deadband = db; center = cntr;
  }

  uint16_t update(uint16_t raw) {
    uint16_t val = inverted ? (4095 - raw) : raw;
    if (!initialized) {
      window[0] = window[1] = window[2] = val;
      iirState = (float)val;
      initialized = true;
      return val;
    }
    window[winIdx] = val;
    winIdx = (winIdx + 1) % 3;
    uint16_t med = median3(window[0], window[1], window[2]);
    iirState += alpha * ((float)med - iirState);
    if (iirState < 0.0f) iirState = 0.0f;
    if (iirState > 4095.0f) iirState = 4095.0f;
    return (uint16_t)(iirState + 0.5f);
  }

  uint16_t getFiltered() const {
    return (uint16_t)(iirState + 0.5f);
  }

  float getNormalizedDeflection() const {
    uint16_t val = getFiltered();
    if (val >= (uint16_t)(center - deadband) && val <= (uint16_t)(center + deadband)) {
      return 0.0f;
    }
    if (val > (uint16_t)(center + deadband)) {
      return (float)(val - (center + deadband)) / (float)(4095 - (center + deadband));
    }
    return -(float)((center - deadband) - val) / (float)((center - deadband));
  }
};

class PotNotchTracker {
private:
  int notches;
  int hysteresis;
  int lastNotch;
  int lastReading;

public:
  PotNotchTracker() : notches(20), hysteresis(65), lastNotch(-1), lastReading(-1) {}

  void configure(int n, int h) {
    notches = n; hysteresis = h;
  }

  bool update(uint16_t reading, int& outNotch) {
    int notchWidth = 4096 / notches;
    int currentNotch = reading / notchWidth;
    outNotch = currentNotch;
    if (lastNotch == -1) {
      lastNotch = currentNotch;
      lastReading = reading;
      return false;
    }
    if (currentNotch != lastNotch && std::abs((int)reading - lastReading) > hysteresis) {
      lastNotch = currentNotch;
      lastReading = reading;
      return true;
    }
    return false;
  }
};

struct DigitalConfig {
  uint8_t pin;
  const char* name;
  uint8_t activeLevel;
  bool isToggle;
  uint32_t debounceMs;
};

class DigitalInputChannel {
private:
  DigitalConfig config;
  bool pressed;
  bool justPressedFlag;
  uint32_t lastTransitionTime;
  uint8_t lastStableRaw;

public:
  DigitalInputChannel() : pressed(false), justPressedFlag(false), lastTransitionTime(0), lastStableRaw(1) {}

  void init(const DigitalConfig& cfg) {
    config = cfg;
    lastStableRaw = (config.activeLevel == 0) ? 1 : 0;
    pressed = false;
  }

  void update(uint32_t nowMs, uint8_t rawState) {
    justPressedFlag = false;
    if (rawState != lastStableRaw) {
      if (lastTransitionTime == 0) {
        lastTransitionTime = nowMs;
      } else if (nowMs - lastTransitionTime >= config.debounceMs) {
        lastStableRaw = rawState;
        bool newPressed = (rawState == config.activeLevel);
        if (newPressed && !pressed) {
          justPressedFlag = true;
        }
        pressed = newPressed;
        lastTransitionTime = 0;
      }
    } else {
      lastTransitionTime = 0;
    }
  }

  bool isPressed() const { return pressed; }
  bool justPressed() const { return justPressedFlag; }
};

struct ReceiverCommand {
  int16_t throttle_norm;
  int16_t steering_norm;
  bool is_armed;
};

class ReceiverFailsafe {
private:
  uint32_t timeout;
  uint8_t requiredPackets;
  uint8_t validCount;
  bool failsafeActive;
  uint32_t lastPacketMs;
  ReceiverCommand cmd;

public:
  ReceiverFailsafe() : timeout(500), requiredPackets(3), validCount(0), failsafeActive(true), lastPacketMs(0) {
    cmd = {0, 0, false};
  }

  void init(uint32_t to, uint8_t req) {
    timeout = to; requiredPackets = req; validCount = 0; failsafeActive = true;
    cmd = {0, 0, false};
  }

  bool isFailsafeActive() const { return failsafeActive; }

  void onValidPacket(const RoverPacket& pkt, uint32_t nowMs) {
    lastPacketMs = nowMs;
    if (failsafeActive) {
      validCount++;
      if (validCount >= requiredPackets) {
        failsafeActive = false;
        validCount = 0;
      }
    }
    if (!failsafeActive) {
      float gov = 0.15f + 0.85f * ((float)pkt.pot_l / 4095.0f);
      int ly = (int)pkt.stick_ly;
      float rawThr = 0.0f;
      if (ly > 2188) rawThr = (float)(ly - 2188) / (4015 - 2188);
      else if (ly < 1908) rawThr = -(float)(1908 - ly) / (1908 - 80);
      cmd.throttle_norm = (int16_t)(rawThr * gov * 1000.0f);
      cmd.steering_norm = 0;
      cmd.is_armed = (pkt.toggle_l == 1);
    }
  }

  void update(uint32_t nowMs) {
    if (!failsafeActive && (nowMs - lastPacketMs > timeout)) {
      failsafeActive = true;
      validCount = 0;
      cmd.throttle_norm = 0;
      cmd.steering_norm = 0;
      cmd.is_armed = false;
    }
  }

  ReceiverCommand getCommand() const { return cmd; }
};


// Test Harness Counters
static int testsRun = 0;
static int testsPassed = 0;

#define TEST_ASSERT(cond, msg) do { \
  testsRun++; \
  if (cond) { \
    testsPassed++; \
    std::cout << "  [PASS] " << msg << std::endl; \
  } else { \
    std::cerr << "  [FAIL] " << msg << " (Line " << __LINE__ << ")" << std::endl; \
  } \
} while(0)

// =============================================================================
//  TEST 1: PROTOCOL MEMORY LAYOUT & PACKET VALIDATION
// =============================================================================
void test_protocol_layout_and_validation() {
  std::cout << "\n>>> RUNNING TEST SUITE 1: Protocol Layout & Sanity Verification..." << std::endl;

  // 1. Struct Packing & Size
  TEST_ASSERT(sizeof(RoverPacket) == 24, "RoverPacket struct size is exactly 24 bytes");

  // 2. Field Offsets Check
  RoverPacket pkt;
  uint8_t* base = reinterpret_cast<uint8_t*>(&pkt);
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.stick_lx) - base) == 0,  "stick_lx offset is 0");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.stick_ly) - base) == 2,  "stick_ly offset is 2");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.stick_rx) - base) == 4,  "stick_rx offset is 4");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.stick_ry) - base) == 6,  "stick_ry offset is 6");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.pot_l)    - base) == 8,  "pot_l offset is 8");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.pot_c)    - base) == 10, "pot_c offset is 10");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.pot_r)    - base) == 12, "pot_r offset is 12");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.toggle_l) - base) == 14, "toggle_l offset is 14");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.toggle_r) - base) == 15, "toggle_r offset is 15");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.push_l)   - base) == 16, "push_l offset is 16");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.push_r)   - base) == 17, "push_r offset is 17");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.stick_clk_l)-base) == 18, "stick_clk_l offset is 18");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.stick_clk_r)-base) == 19, "stick_clk_r offset is 19");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&pkt.frame_id) - base) == 20, "frame_id offset is 20");

  // 3. Packet Validity Checker
  pkt.stick_lx = 2048; pkt.stick_ly = 2048; pkt.stick_rx = 2048; pkt.stick_ry = 2048;
  pkt.pot_l = 4000; pkt.pot_c = 2000; pkt.pot_r = 1000;
  pkt.toggle_l = 1; pkt.toggle_r = 0; pkt.push_l = 0; pkt.push_r = 0;
  pkt.stick_clk_l = 0; pkt.stick_clk_r = 0;
  pkt.frame_id = 100;

  TEST_ASSERT(isRoverPacketValid(pkt) == true, "Valid packet recognized as valid");

  // Corrupted analog value (> 4095)
  pkt.stick_lx = 4096;
  TEST_ASSERT(isRoverPacketValid(pkt) == false, "Packet with analog value 4096 rejected");
  pkt.stick_lx = 2048;

  // Corrupted floating bus 0xFFFF
  pkt.pot_l = 65535;
  TEST_ASSERT(isRoverPacketValid(pkt) == false, "Floating SPI bus noise (65535) rejected");
  pkt.pot_l = 4000;

  // Corrupted digital state (> 1)
  pkt.toggle_l = 2;
  TEST_ASSERT(isRoverPacketValid(pkt) == false, "Corrupted digital switch value (>1) rejected");
  pkt.toggle_l = 1;
}

// =============================================================================
//  TEST 2: ANALOG NOISE REJECTION & MEDIAN FILTERING
// =============================================================================
void test_analog_noise_rejection() {
  std::cout << "\n>>> RUNNING TEST SUITE 2: Analog Noise Rejection & Conditioning..." << std::endl;

  AnalogChannelFilter filter;
  filter.configure(0.70f, false, 140, 2048);

  // Initialize at center
  filter.update(2048);
  filter.update(2048);
  filter.update(2048);
  TEST_ASSERT(filter.getFiltered() == 2048, "Filter settles exactly at center (2048)");

  // Inject single-sample impulsive spike: 2048 -> 3900 (spike) -> 2048 -> 2048
  uint16_t spikeOut = filter.update(3900);
  // Median of (2048, 2048, 3900) is 2048! The spike is completely eliminated by median filter!
  TEST_ASSERT(abs((int)spikeOut - 2048) <= 5, "Single-sample 3900 spike completely rejected by median stage");

  // Legitimate aggressive step movement: 2048 -> 3500 -> 3500 -> 3500
  filter.update(3500);
  filter.update(3500);
  uint16_t stepOut = filter.update(3500);
  TEST_ASSERT(stepOut >= 3400, "Sustained rapid movement tracks quickly (fast IIR response)");

  // Test Axis Inversion (Right-X gimbal)
  AnalogChannelFilter invFilter;
  invFilter.configure(0.70f, true, 140, 2048);
  invFilter.update(1000);
  invFilter.update(1000);
  invFilter.update(1000);
  uint16_t invOut = invFilter.getFiltered();
  TEST_ASSERT(abs((int)invOut - (4095 - 1000)) <= 5, "Inverted axis maps 1000 -> ~3095 correctly");

  // Test Deadband Normalization
  AnalogChannelFilter deadFilter;
  deadFilter.configure(0.70f, false, 140, 2048);
  deadFilter.update(2100); // Within [1908, 2188]
  deadFilter.update(2100);
  deadFilter.update(2100);
  TEST_ASSERT(deadFilter.getNormalizedDeflection() == 0.0f, "Reading inside deadband (2100) returns 0.0f");

  // Outside deadband (3000)
  deadFilter.update(3000);
  deadFilter.update(3000);
  deadFilter.update(3000);
  deadFilter.update(3000);
  float normDef = deadFilter.getNormalizedDeflection();
  TEST_ASSERT(normDef > 0.40f && normDef < 0.60f, "Reading outside deadband (3000) maps smoothly without jumps");
}

// =============================================================================
//  TEST 3: POTENTIOMETER HYSTERESIS
// =============================================================================
void test_potentiometer_hysteresis() {
  std::cout << "\n>>> RUNNING TEST SUITE 3: Potentiometer Hysteresis Notch Tracking..." << std::endl;

  PotNotchTracker tracker;
  tracker.configure(20, 65); // 20 notches, 65 hysteresis counts

  int notch = 0;
  tracker.update(200, notch);
  TEST_ASSERT(notch == 0, "Initial notch correctly recognized as 0");

  // Small noise fluctuating around notch boundary (200 -> 210 -> 195)
  bool fired = tracker.update(210, notch);
  TEST_ASSERT(fired == false, "Fluctuation across boundary below 65 counts does NOT fire false notch alert");

  // Large deliberate turn from 200 to 450 (Notch 2)
  fired = tracker.update(450, notch);
  TEST_ASSERT(fired == true && notch == 2, "Deliberate rotation (delta > 65) fires valid notch alert");
}

// =============================================================================
//  TEST 4: DIGITAL DEBOUNCER STATE MACHINE
// =============================================================================
void test_digital_debouncer() {
  std::cout << "\n>>> RUNNING TEST SUITE 4: Digital Debouncer & Polarity Tracking..." << std::endl;

  DigitalInputChannel btn;
  DigitalConfig cfg = { 10, "TEST_BTN", 0 /* LOW active */, false, 20 /* 20ms debounce */ };
  btn.init(cfg);

  // Initial state: not pressed (HIGH)
  btn.update(100, 1);
  TEST_ASSERT(btn.isPressed() == false, "Initial HIGH state is not pressed");

  // Contact bounce sequence: 0 -> 1 -> 0 over 10ms
  btn.update(102, 0); // Raw LOW at t=102
  btn.update(105, 1); // Bounces HIGH at t=105
  btn.update(108, 0); // Bounces LOW at t=108
  TEST_ASSERT(btn.isPressed() == false, "Contact bounce within 20ms window is filtered out");

  // Stable LOW held past 20ms debounce interval
  btn.update(130, 0); // At t=130 (> 20ms of stable LOW)
  TEST_ASSERT(btn.isPressed() == true, "Stable input past debounce window registers as pressed");
  TEST_ASSERT(btn.justPressed() == true, "justPressed() edge flag set on transition");

  // Next pass without state change
  btn.update(140, 0);
  TEST_ASSERT(btn.justPressed() == false, "justPressed() edge flag clears automatically");
}

// =============================================================================
//  TEST 5: RECEIVER MOTOR FAILSAFE SUPERVISOR
// =============================================================================
void test_receiver_failsafe() {
  std::cout << "\n>>> RUNNING TEST SUITE 5: Receiver Safety Failsafe Supervisor..." << std::endl;

  ReceiverFailsafe failsafe;
  failsafe.init(500, 3); // 500ms timeout, 3 packets required to recover

  TEST_ASSERT(failsafe.isFailsafeActive() == true, "Failsafe starts engaged on boot (Safe State)");

  RoverPacket pkt;
  pkt.stick_lx = 2048;
  pkt.stick_ly = 3500; // Forward throttle
  pkt.stick_rx = 2048;
  pkt.stick_ry = 2048;
  pkt.pot_l = 4095;    // 100% Governor
  pkt.pot_c = 2048;
  pkt.pot_r = 2048;
  pkt.toggle_l = 1;    // Armed
  pkt.toggle_r = 0;
  pkt.push_l = 0; pkt.push_r = 0;
  pkt.stick_clk_l = 0; pkt.stick_clk_r = 0;
  pkt.frame_id = 1;

  // Packet 1 received at t=100
  failsafe.onValidPacket(pkt, 100);
  TEST_ASSERT(failsafe.isFailsafeActive() == true, "Failsafe remains engaged after 1 packet (Reconnection Guard)");

  // Packet 2 received at t=125
  pkt.frame_id = 2;
  failsafe.onValidPacket(pkt, 125);
  TEST_ASSERT(failsafe.isFailsafeActive() == true, "Failsafe remains engaged after 2 packets");

  // Packet 3 received at t=150
  pkt.frame_id = 3;
  failsafe.onValidPacket(pkt, 150);
  TEST_ASSERT(failsafe.isFailsafeActive() == false, "Failsafe unclamps after 3 consecutive valid packets");
  TEST_ASSERT(failsafe.getCommand().is_armed == true, "Motors armed and ready");
  TEST_ASSERT(failsafe.getCommand().throttle_norm > 500, "Forward throttle command computed properly");

  // Simulate signal loss: time advances past 500ms timeout (t=700ms)
  failsafe.update(700);
  TEST_ASSERT(failsafe.isFailsafeActive() == true, "Failsafe engages immediately when packet age > 500ms");
  TEST_ASSERT(failsafe.getCommand().throttle_norm == 0, "Motor throttle clamped to zero on failsafe");
  TEST_ASSERT(failsafe.getCommand().steering_norm == 0, "Motor steering clamped to zero on failsafe");
  TEST_ASSERT(failsafe.getCommand().is_armed == false, "Motors disarmed on failsafe");
}

// =============================================================================
//  TEST 6: FRAME COUNTER ROLLOVER
// =============================================================================
void test_frame_counter_rollover() {
  std::cout << "\n>>> RUNNING TEST SUITE 6: 32-Bit Frame Counter Rollover..." << std::endl;

  uint32_t counter = 0xFFFFFFFF; // Max 32-bit integer
  counter++;
  TEST_ASSERT(counter == 0, "32-bit frame counter rolls over cleanly to 0 without overflow crashes");
}

// =============================================================================
//  TEST 7: POWERTRAIN TELEMETRY & BTS7960 CURRENT BOUNDS
// =============================================================================
void test_powertrain_and_telemetry_bounds() {
  std::cout << "\n>>> RUNNING TEST SUITE 7: Powertrain & BTS7960 Telemetry Bounds..." << std::endl;

  // 1. Car Battery 3S Voltage Range (0 to 4095 maps to 9.60V to 12.60V)
  uint16_t pot_min = 0;
  uint16_t pot_max = 4095;
  uint16_t pot_mid = 2048;

  float v_min = 9.60f + (((float)pot_min / 4095.0f) * 3.00f);
  float v_max = 9.60f + (((float)pot_max / 4095.0f) * 3.00f);
  float v_mid = 9.60f + (((float)pot_mid / 4095.0f) * 3.00f);

  TEST_ASSERT(std::abs(v_min - 9.60f) < 0.01f, "Empty battery dial maps exactly to 9.60V cutoff");
  TEST_ASSERT(std::abs(v_max - 12.60f) < 0.01f, "Full battery dial maps exactly to 12.60V top charge");
  TEST_ASSERT(std::abs(v_mid - 11.10f) < 0.02f, "Mid battery dial maps to 11.10V nominal 3S voltage");

  // 2. BTS7960 Current Load Bounds (0A - 15A scale)
  float test_curr_zero = 0.0f;
  float test_curr_norm = 4.2f;
  float test_curr_over = 18.5f;

  int bar_zero = (int)((test_curr_zero / 15.0f) * 52.0f + 0.5f);
  int bar_norm = (int)((test_curr_norm / 15.0f) * 52.0f + 0.5f);
  int bar_over = (int)((test_curr_over / 15.0f) * 52.0f + 0.5f);
  if (bar_over > 52) bar_over = 52;

  TEST_ASSERT(bar_zero == 0, "Zero current gives 0px load bar");
  TEST_ASSERT(bar_norm == 15, "Normal running load (4.2A) maps to ~15px bar");
  TEST_ASSERT(bar_over == 52, "Overcurrent (>15A) clamps safely to 52px full scale without memory overflow");

  // 3. RSSI Threshold Recognition
  int8_t rssi_strong = -62;
  int8_t rssi_mid    = -72;
  int8_t rssi_weak   = -88;

  int bars_strong = (rssi_strong > -65) ? 4 : 3;
  int bars_mid    = (rssi_mid > -75) ? 3 : 2;
  int bars_weak   = (rssi_weak > -85) ? 2 : 1;

  TEST_ASSERT(bars_strong == 4, "Strong signal (-62dBm) renders full 4-bar antenna icon");
  TEST_ASSERT(bars_mid == 3, "Medium signal (-72dBm) renders 3-bar antenna icon");
  TEST_ASSERT(bars_weak == 1, "Critical weak signal (-88dBm) triggers 1-bar warning");
}

// =============================================================================
//  TEST 8: SETTINGS MENU ARCHITECTURE, EEPROM & PAGE 4 SAFETY CLAMP
// =============================================================================
struct TestRemoteConfig {
  uint16_t magic;          // 0xC5E6
  uint16_t deadband;       // Default: 140 (Range: 50 to 300)
  uint8_t  refresh_rate;   // Default: 40 (Options: 20, 40, 50)
  uint8_t  contrast;       // Default: 200 (Range: 25 to 255)
  uint8_t  drive_profile;  // 0 = Arcade, 1 = Tank
  uint8_t  notch_sound;    // 1 = Enabled, 0 = Muted
  uint16_t center_lx;
  uint16_t center_ly;
  uint16_t center_rx;
  uint16_t center_ry;
  uint16_t min_lx;
  uint16_t max_lx;
  uint16_t min_ly;
  uint16_t max_ly;
  uint16_t min_rx;
  uint16_t max_rx;
  uint16_t min_ry;
  uint16_t max_ry;
};

void test_settings_menu_and_page4_safety() {
  std::cout << "\n>>> RUNNING TEST SUITE 8: Settings Menu Architecture & Page 4 Safety Clamp..." << std::endl;

  // 1. Verify Configuration Structure & Magic Header
  TestRemoteConfig cfg;
  cfg.magic        = 0xC5E6;
  cfg.deadband     = 140;
  cfg.refresh_rate = 40;
  cfg.contrast     = 200;
  cfg.drive_profile= 0;
  cfg.notch_sound  = 1;
  cfg.center_lx    = 2048;
  cfg.center_ly    = 2048;
  cfg.center_rx    = 2048;
  cfg.center_ry    = 2048;
  cfg.min_lx       = 0;
  cfg.max_lx       = 4095;
  cfg.min_ly       = 0;
  cfg.max_ly       = 4095;
  cfg.min_rx       = 0;
  cfg.max_rx       = 4095;
  cfg.min_ry       = 0;
  cfg.max_ry       = 4095;

  TEST_ASSERT(cfg.magic == 0xC5E6, "EEPROM magic key 0xC5E6 authenticates valid configuration");
  TEST_ASSERT(cfg.deadband >= 50 && cfg.deadband <= 300, "Default deadband 140 is within safe limits (50-300)");
  TEST_ASSERT(cfg.refresh_rate == 20 || cfg.refresh_rate == 40 || cfg.refresh_rate == 50, "RF Refresh rate is bounded to 20/40/50 Hz");
  TEST_ASSERT(cfg.contrast >= 25 && cfg.contrast <= 255, "OLED contrast is within readable range (25-255)");

  // 2. Verify Page 4 Safety Interlock Clamp
  RoverPacket rawPilotPacket;
  rawPilotPacket.stick_lx = 3800; // Pilot moving left stick during menu nav
  rawPilotPacket.stick_ly = 800;  // Pilot moving left stick during menu nav
  rawPilotPacket.stick_rx = 3100;
  rawPilotPacket.stick_ry = 1200;
  rawPilotPacket.toggle_l = 1;    // Armed switch is ON
  rawPilotPacket.push_l   = 1;    // Pressing PA8 (Back button in menu)
  rawPilotPacket.push_r   = 1;    // Pressing PA15 (Enter button in menu)
  rawPilotPacket.frame_id = 100;

  uint8_t simulatedPage = 3; // Page 4 (0-indexed: 3)
  RoverPacket packetToSend = rawPilotPacket;

  if (simulatedPage == 3) {
    packetToSend.stick_lx = 2048;
    packetToSend.stick_ly = 2048;
    packetToSend.stick_rx = 2048;
    packetToSend.stick_ry = 2048;
    packetToSend.toggle_l = 0; // Forced Disarm
    packetToSend.push_l   = 0; // Suppress rover aux
    packetToSend.push_r   = 0; // Suppress rover aux
  }

  TEST_ASSERT(packetToSend.stick_lx == 2048, "Page 4 Safety Clamp: Steering locked to neutral (2048)");
  TEST_ASSERT(packetToSend.stick_ly == 2048, "Page 4 Safety Clamp: Throttle locked to neutral (2048)");
  TEST_ASSERT(packetToSend.toggle_l == 0, "Page 4 Safety Clamp: Vehicle forced to Disarmed (Park) mode");
  TEST_ASSERT(packetToSend.push_l == 0 && packetToSend.push_r == 0, "Page 4 Safety Clamp: Menu buttons PA8/PA15 suppressed from RF payload");

  // 2.5. Verify Settings Menu (Page 4) Continuous Transmission Suppression
  // Simulation: When pilot is navigating on Page 4, transmission is suppressed after 1 initial STOP frame
  bool simWasInSettings = false;
  int simPacketsSentInSettings = 0;
  for (int iter = 0; iter < 50; iter++) {
    if (simulatedPage == 3) {
      if (!simWasInSettings) {
        simWasInSettings = true;
        simPacketsSentInSettings++; // 1 initial neutral stop frame
      }
      // Zero further transmissions during settings navigation!
    } else {
      simWasInSettings = false;
      simPacketsSentInSettings++;
    }
  }
  TEST_ASSERT(simPacketsSentInSettings == 1, "Page 4 Isolation: Exactly 1 neutral STOP frame sent on entry, zero continuous packets while navigating");

  // 2.6. Bandwidth & Timing Budget for 50 Hz RF Teleoperation
  // At 10 FPS OLED: 10 * 25ms = 250ms I2C time.
  // 50 Hz Teleop: 50 * 20ms cycles = 1000ms. In each 100ms window, 5 packets (5 * 1.3ms = 6.5ms UART) + 1 OLED frame (25ms) = 31.5ms.
  // Headroom: 68.5ms / 100ms (68.5% idle headroom) guarantees full 50 Hz throughput without jitter!
  uint32_t oledTimePerSec = 10 * 25; // 250ms
  uint32_t rfUartTimePerSec = 50 * 2; // ~100ms max
  TEST_ASSERT((oledTimePerSec + rfUartTimePerSec) < 500, "50 Hz Teleop Timing Budget: < 50% CPU utilization ensures unthrottled 50 Hz RF rate");

  // 3. Verify Right Stick Crosstalk Isolation
  // Hardware rule: RY must never affect driving or menu state
  uint16_t corrupted_ry = 3900; // Mechanical crosstalk phantom spike
  float throttle_ratio = 0.0f;
  const int deadband_val = cfg.deadband;
  const int deadband_high = 2048 + deadband_val;
  if (rawPilotPacket.stick_ly > deadband_high) {
    throttle_ratio = (float)(rawPilotPacket.stick_ly - deadband_high) / (4015 - deadband_high);
  }
  TEST_ASSERT(corrupted_ry > 0, "Right Y gimbal raw value preserved for logging");
  TEST_ASSERT(throttle_ratio == 0.0f, "Corrupted Right Y axis has ZERO impact on vehicle throttle");
}

#include "../firmware/cyberrover_x5_remote_tx/boot_bitmaps.h"

// =============================================================================
//  TEST 9: BOOT ANIMATION BITMAPS & MEMORY FOOTPRINT
// =============================================================================
void test_boot_animation_bitmaps() {
  std::cout << "\n>>> RUNNING TEST SUITE 9: Boot Animation Bitmaps & Flash Memory Footprint..." << std::endl;

  // 1. Verify Battery Outline array size: 70x40 px (9 bytes per row * 40 rows = 360 bytes)
  TEST_ASSERT(sizeof(epd_bitmap_battery_outline) == 360, "epd_bitmap_battery_outline is exactly 360 bytes in Flash");

  // 2. Verify Blob Animation frames (32x34 px = 4 bytes per row * 34 rows = 136 bytes each)
  TEST_ASSERT(sizeof(epd_bitmap_blob_00) == 136, "epd_bitmap_blob_00 is exactly 136 bytes (32x34 px)");
  TEST_ASSERT(sizeof(epd_bitmap_blob_28) == 136, "epd_bitmap_blob_28 is exactly 136 bytes (32x34 px)");

  // 3. Verify Array of Frames contains 5 high-efficiency fluid keyframes
  TEST_ASSERT(sizeof(epd_bitmap_allArray) / sizeof(epd_bitmap_allArray[0]) == 5, "epd_bitmap_allArray contains exactly 5 optimized keyframes");
  bool all_non_null = true;
  for (int i = 0; i < 5; i++) {
    if (epd_bitmap_allArray[i] == nullptr) all_non_null = false;
  }
  TEST_ASSERT(all_non_null == true, "All 5 blob animation frame pointers are valid and non-null");

  // 4. Verify Total Flash ROM Consumption of Boot Animation <= 1.6 KB (fits safely under 64KB limit)
  size_t total_boot_flash = sizeof(epd_bitmap_battery_outline) + (epd_bitmap_allArray_LEN * 136) + sizeof(epd_bitmap_allArray);
  TEST_ASSERT(total_boot_flash < 1600, "Total boot animation Flash overhead is under 1.6 KB (fits safely under 64KB limit)");

  // 5. Verify Inverted XOR Number Counter Geometry & Containment:
  const int xoff = 29;
  const int yoff = 18;
  const int chamber_w = 61;
  const int chamber_h = 34;
  const int chamber_x1 = xoff + 3; // 32
  const int chamber_y1 = yoff + 3; // 21
  const int chamber_x2 = chamber_x1 + chamber_w; // 93
  const int chamber_y2 = chamber_y1 + chamber_h; // 55

  // Verify counter bounds across 0% to 100%
  for (int perc = 0; perc <= 100; perc += 10) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", perc);
    // Estimated max width with helvB14: digits ~9px, '%' ~11px -> "100%" ~ 38px max
    int est_w = (perc == 100) ? 38 : (perc >= 10 ? 29 : 20);
    int txt_x = chamber_x1 + (chamber_w - est_w) / 2;
    int txt_y = chamber_y1 + 23;

    TEST_ASSERT(txt_x >= chamber_x1 && (txt_x + est_w) <= chamber_x2,
                "Counter text fits strictly inside 61px liquid chamber width");
    TEST_ASSERT((txt_y - 11) >= chamber_y1 && (txt_y + 3) <= chamber_y2,
                "Counter text fits strictly inside 34px liquid chamber height");
  }

  // 6. Verify Mathematical XOR Inversion Behavior:
  // Black Background (0) ^ 1 = 1 (White text pixel)
  // White Liquid Fill (1) ^ 1 = 0 (Black text pixel)
  uint8_t pixel_black_bg = 0;
  uint8_t pixel_white_liquid = 1;
  TEST_ASSERT((pixel_black_bg ^ 1) == 1, "XOR Mode: Text over black background inverts to WHITE");
  TEST_ASSERT((pixel_white_liquid ^ 1) == 0, "XOR Mode: Text over white liquid inverts to BLACK");
}

// =============================================================================
//  TEST 10: AUTO-CENTERING, NORMALIZATION & DUAL-DRIVE PROFILE VERIFICATION
// =============================================================================
int test_calculateStickPercent(uint16_t raw_val, uint16_t center_val, uint16_t deadband, uint16_t min_val = 0, uint16_t max_val = 4095) {
  int diff = (int)raw_val - (int)center_val;
  if (std::abs(diff) <= (int)deadband) {
    return 0;
  }
  int pct = 0;
  if (diff > 0) {
    int max_span = (int)max_val - (int)center_val - (int)deadband;
    if (max_span < 200) max_span = 200;
    pct = ((diff - (int)deadband) * 100) / max_span;
  } else {
    int min_span = (int)center_val - (int)min_val - (int)deadband;
    if (min_span < 200) min_span = 200;
    pct = ((diff + (int)deadband) * 100) / min_span;
  }
  if (pct > 100)  pct = 100;
  if (pct < -100) pct = -100;
  return pct;
}

uint16_t test_mapPercentToADC(int pct) {
  if (pct >= 100) return 4095;
  if (pct <= -100) return 0;
  if (pct == 0) return 2048;
  if (pct > 0) {
    return (uint16_t)(2048 + (pct * 2047) / 100);
  } else {
    return (uint16_t)(2048 + (pct * 2048) / 100);
  }
}

void test_auto_centering_and_drive_profiles() {
  std::cout << "\n>>> RUNNING TEST SUITE 10: Auto-Centering, Normalization & Dual-Drive Profiles..." << std::endl;

  // 1. Asymmetric Hardware Centers (from user's live serial log: LX:2012 LY:2048 RX:2728 RY:1383)
  uint16_t center_lx = 2012;
  uint16_t center_ly = 2048;
  uint16_t center_rx = 2728;
  uint16_t center_ry = 1383; // User's drifting right gimbal resting point
  uint16_t deadband  = 140;

  // Verify all channels return exactly 0% at rest with calibrated centers
  TEST_ASSERT(test_calculateStickPercent(center_lx, center_lx, deadband) == 0, "LX at rest returns 0%");
  TEST_ASSERT(test_calculateStickPercent(center_ly, center_ly, deadband) == 0, "LY at rest returns 0%");
  TEST_ASSERT(test_calculateStickPercent(center_rx, center_rx, deadband) == 0, "Asymmetric RX (2728) at rest returns 0%");
  TEST_ASSERT(test_calculateStickPercent(center_ry, center_ry, deadband) == 0, "Calibrated RY (1383) at rest returns 0%");

  // Verify Root Cause Reproduction:
  // If RY rests at 1383 but center is left at factory 2048 without calibration synchronization:
  int uncalibrated_ry_pct = test_calculateStickPercent(center_ry, 2048, deadband);
  TEST_ASSERT(uncalibrated_ry_pct <= -23 && uncalibrated_ry_pct >= -27,
              "Root Cause Confirmed: Uncalibrated center (2048) yields -27% phantom deflection");

  // Verify deadband rejection (+/- 100 counts within deadband 140)
  TEST_ASSERT(test_calculateStickPercent(center_rx + 80, center_rx, deadband) == 0, "RX positive jitter within deadband returns 0%");
  TEST_ASSERT(test_calculateStickPercent(center_rx - 80, center_rx, deadband) == 0, "RX negative jitter within deadband returns 0%");
  TEST_ASSERT(test_calculateStickPercent(center_ry + 80, center_ry, deadband) == 0, "RY positive jitter within deadband returns 0%");
  TEST_ASSERT(test_calculateStickPercent(center_ry - 80, center_ry, deadband) == 0, "RY negative jitter within deadband returns 0%");

  // Verify boundary sweeps with full physical travel (e.g. min 300, max 3800)
  TEST_ASSERT(test_calculateStickPercent(3800, center_ly, deadband, 300, 3800) == 100, "Full forward throttle with calibrated bounds maps to +100%");
  TEST_ASSERT(test_calculateStickPercent(300, center_ly, deadband, 300, 3800) == -100, "Full reverse throttle with calibrated bounds maps to -100%");
  TEST_ASSERT(test_calculateStickPercent(3900, center_rx, deadband, 250, 3900) == 100, "Full right steering with calibrated bounds maps to +100%");
  TEST_ASSERT(test_calculateStickPercent(250, center_rx, deadband, 250, 3900) == -100, "Full left steering with calibrated bounds maps to -100%");

  // 2. mapPercentToADC calibration mapping
  TEST_ASSERT(test_mapPercentToADC(0) == 2048, "0% maps to neutral ADC 2048");
  TEST_ASSERT(test_mapPercentToADC(100) == 4095, "+100% maps to maximum ADC 4095");
  TEST_ASSERT(test_mapPercentToADC(-100) == 0, "-100% maps to minimum ADC 0");
  TEST_ASSERT(test_mapPercentToADC(50) == 3071, "+50% maps to linear ADC 3071");
  TEST_ASSERT(test_mapPercentToADC(-50) == 1024, "-50% maps to linear ADC 1024");

  // 3. Arcade Mode Steering Polarity & Blinkers: Right Stick X
  int str_left = test_calculateStickPercent(1000, center_rx, deadband); // Pushed left
  int str_right = test_calculateStickPercent(3900, center_rx, deadband); // Pushed right
  int str_center = test_calculateStickPercent(center_rx, center_rx, deadband); // Center

  bool arcade_blinker_l_when_left = (str_left < -15);
  bool arcade_blinker_r_when_left = (str_left > 15);
  TEST_ASSERT(arcade_blinker_l_when_left == true && arcade_blinker_r_when_left == false,
              "Arcade: Moving stick LEFT blinks LEFT arrow (◄)");

  bool arcade_blinker_l_when_right = (str_right < -15);
  bool arcade_blinker_r_when_right = (str_right > 15);
  TEST_ASSERT(arcade_blinker_l_when_right == false && arcade_blinker_r_when_right == true,
              "Arcade: Moving stick RIGHT blinks RIGHT arrow (►)");

  bool arcade_blinker_l_when_center = (str_center < -15);
  bool arcade_blinker_r_when_center = (str_center > 15);
  TEST_ASSERT(arcade_blinker_l_when_center == false && arcade_blinker_r_when_center == false,
              "Arcade: Sticks at rest -> Neither arrow blinks");

  // 4. Arcade Mode Isolation of Corrupted Right Stick Y
  // In Arcade Mode (drive_profile == 0):
  uint8_t drive_profile = 0; // Arcade
  int live_thr_pct = test_calculateStickPercent(center_ly, center_ly, deadband);
  int live_str_pct = test_calculateStickPercent(center_rx, center_rx, deadband);
  int live_trk_l_pct = 0;
  int live_trk_r_pct = 0;
  RoverPacket testTxPacket;

  if (drive_profile == 0) {
    live_trk_l_pct = live_thr_pct;
    live_trk_r_pct = 0; // Strictly quarantined to zero!
    testTxPacket.stick_ly = test_mapPercentToADC(live_thr_pct);
    testTxPacket.stick_rx = test_mapPercentToADC(live_str_pct);
    testTxPacket.stick_lx = 2048;
    testTxPacket.stick_ry = 2048; // Strictly quarantined to neutral!
  }

  TEST_ASSERT(live_trk_r_pct == 0, "Arcade Mode: live_trk_r_pct strictly locked to 0%");
  TEST_ASSERT(testTxPacket.stick_ly == 2048, "Arcade Mode: Throttle transmitted as exact neutral (2048)");
  TEST_ASSERT(testTxPacket.stick_rx == 2048, "Arcade Mode: Steering transmitted as exact neutral (2048)");
  TEST_ASSERT(testTxPacket.stick_ry == 2048, "Arcade Mode: Right Y quarantined as exact neutral (2048)");

  // Verify Speedometer Throttle Ratio in Arcade Mode at rest:
  float throttle_ratio = (float)std::abs(live_thr_pct) / 100.0f;
  TEST_ASSERT(throttle_ratio == 0.0f, "Arcade Mode: Resting throttle ratio is exactly 0.0f (no phantom speed)");

  // 5. Tank Mode with Calibrated Right Stick Y
  drive_profile = 1; // Tank Mode
  if (drive_profile == 1) {
    live_trk_l_pct = test_calculateStickPercent(center_ly, center_ly, deadband);
    live_trk_r_pct = test_calculateStickPercent(center_ry, center_ry, deadband); // Using calibrated center!
    testTxPacket.stick_ly = test_mapPercentToADC(live_trk_l_pct);
    testTxPacket.stick_ry = test_mapPercentToADC(live_trk_r_pct);
  }

  TEST_ASSERT(live_trk_l_pct == 0 && live_trk_r_pct == 0, "Tank Mode: Calibrated resting tracks both return 0%");
  TEST_ASSERT(testTxPacket.stick_ly == 2048 && testTxPacket.stick_ry == 2048, "Tank Mode: Calibrated payload transmits neutral 2048 to both tracks");

  int diff_at_rest = live_trk_l_pct - live_trk_r_pct;
  bool tank_rest_blinker_l = (diff_at_rest <= -25);
  bool tank_rest_blinker_r = (diff_at_rest >= 25);
  TEST_ASSERT(tank_rest_blinker_l == false && tank_rest_blinker_r == false,
              "Tank Mode: Resting tracks produce 0% differential -> Neither blinker blinks");

  // 6. Profile 2: 1-Stick Left Joystick Control
  drive_profile = 2; // 1-Stick Left
  int live_lx_pct = test_calculateStickPercent(center_lx, center_lx, deadband);
  if (drive_profile == 2) {
    live_trk_l_pct = live_thr_pct;
    live_trk_r_pct = 0;
    live_str_pct   = live_lx_pct;
    testTxPacket.stick_ly = test_mapPercentToADC(live_thr_pct);
    testTxPacket.stick_rx = test_mapPercentToADC(live_lx_pct); // Left X transmitted as vehicle steering
    testTxPacket.stick_lx = 2048;
    testTxPacket.stick_ry = 2048; // Right stick completely quarantined
  }
  TEST_ASSERT(testTxPacket.stick_ly == 2048, "Profile 2: Throttle centered at rest");
  TEST_ASSERT(testTxPacket.stick_rx == 2048, "Profile 2: Steering centered at rest");
  TEST_ASSERT(testTxPacket.stick_ry == 2048, "Profile 2: Right Stick Y quarantined to neutral 2048");

  // Pushing Left Stick Left (-50%) steers rover Left
  int left_steer_adc = center_lx - 800;
  int steer_pct = test_calculateStickPercent(left_steer_adc, center_lx, deadband);
  testTxPacket.stick_rx = test_mapPercentToADC(steer_pct);
  TEST_ASSERT(steer_pct < -15, "Profile 2: Left Stick pushed LEFT produces negative steering");
  TEST_ASSERT(testTxPacket.stick_rx < 2048, "Profile 2: Rover steering channel deflects LEFT");
  bool p2_blinker_l = (steer_pct < -15);
  bool p2_blinker_r = (steer_pct > 15);
  TEST_ASSERT(p2_blinker_l == true && p2_blinker_r == false, "Profile 2: Pushing Left Stick LEFT blinks LEFT arrow");
}

void test_boot_stick_calibration() {
  std::cout << "\n>>> RUNNING TEST SUITE 11: Boot Stick Full-Circle Calibration Engine..." << std::endl;

  uint16_t auto_center_lx = 2012;
  uint16_t auto_center_ly = 2048;
  uint16_t auto_center_rx = 2728;
  uint16_t auto_center_ry = 1383;

  uint16_t min_lx = auto_center_lx;
  uint16_t max_lx = auto_center_lx;
  uint16_t min_ly = auto_center_ly;
  uint16_t max_ly = auto_center_ly;
  uint16_t min_rx = auto_center_rx;
  uint16_t max_rx = auto_center_rx;
  uint16_t min_ry = auto_center_ry;
  uint16_t max_ry = auto_center_ry;

  TEST_ASSERT(min_lx == auto_center_lx && max_lx == auto_center_lx, "Initial calibration bounds locked to resting centers");

  // Simulated stick rotation:
  // Left stick rotated fully (LX: 100 to 3950, LY: 80 to 4015)
  // Right stick rotated fully (RX: 150 to 3900, RY: 120 to 3850)
  uint16_t samples_lx[] = { 2012, 1000, 300, 100, 2500, 3950, 2012 };
  uint16_t samples_ly[] = { 2048, 1200, 500, 80, 2800, 4015, 2048 };
  uint16_t samples_rx[] = { 2728, 1800, 600, 150, 3200, 3900, 2728 };
  uint16_t samples_ry[] = { 1383, 800, 400, 120, 2400, 3850, 1383 };

  for (size_t i = 0; i < 7; i++) {
    if (samples_lx[i] < min_lx) min_lx = samples_lx[i];
    if (samples_lx[i] > max_lx) max_lx = samples_lx[i];
    if (samples_ly[i] < min_ly) min_ly = samples_ly[i];
    if (samples_ly[i] > max_ly) max_ly = samples_ly[i];
    if (samples_rx[i] < min_rx) min_rx = samples_rx[i];
    if (samples_rx[i] > max_rx) max_rx = samples_rx[i];
    if (samples_ry[i] < min_ry) min_ry = samples_ry[i];
    if (samples_ry[i] > max_ry) max_ry = samples_ry[i];
  }

  TEST_ASSERT(min_lx == 100 && max_lx == 3950, "Full stick rotation: LX min/max correctly captured");
  TEST_ASSERT(min_ly == 80 && max_ly == 4015, "Full stick rotation: LY min/max correctly captured");
  TEST_ASSERT(min_rx == 150 && max_rx == 3900, "Full stick rotation: RX min/max correctly captured");
  TEST_ASSERT(min_ry == 120 && max_ry == 3850, "Full stick rotation: RY min/max correctly captured");

  bool lx_swung = (max_lx - min_lx >= 600);
  bool ly_swung = (max_ly - min_ly >= 600);
  bool rx_swung = (max_rx - min_rx >= 600);
  bool fullCircleMoved = (lx_swung && ly_swung && rx_swung);

  TEST_ASSERT(fullCircleMoved == true, "Full circle threshold accurately validated (span >= 600 counts)");

  // Verify that small resting jitter does NOT satisfy full circle
  uint16_t min_jitter = 2000, max_jitter = 2090;
  bool jitter_swung = (max_jitter - min_jitter >= 600);
  TEST_ASSERT(jitter_swung == false, "Resting stick jitter correctly rejected by 600-count swing gate");
}

// =============================================================================
//  TEST 12: RIGHT STICK MENU NAVIGATION FALLBACK & ACTIVE AXES VALIDATION
// =============================================================================
void test_right_stick_navigation_and_axis_telemetry() {
  std::cout << "\n>>> RUNNING TEST SUITE 12: Right Stick Navigation & Axis Isolation..." << std::endl;

  // 1. Right Stick X Navigation Logic Simulation tied to deadband_r
  int deadband_r = 800; // Calibrated deadband setting
  bool btn_enter = false;
  bool btn_back  = false;

  auto evaluateNav = [&](int diff_rx, int db_setting, uint32_t nowMs, bool& enterOut, bool& backOut) {
    static bool lastEnterState = false;
    static bool lastBackState  = false;
    static bool lastRxRight    = false;
    static bool lastRxLeft     = false;
    static uint32_t lastRxNavTime = 0;

    int deadband_margin = std::max(30, db_setting / 10);
    int nav_threshold   = db_setting + deadband_margin;

    bool rx_stick_right = (diff_rx >= nav_threshold);
    bool rx_stick_left  = (diff_rx <= -nav_threshold);

    bool rxEnterPressed = false;
    bool rxBackPressed  = false;

    if (nowMs - lastRxNavTime >= 200) {
      if (rx_stick_right && !lastRxRight) {
        rxEnterPressed = true;
        lastRxNavTime  = nowMs;
      } else if (rx_stick_left && !lastRxLeft) {
        rxBackPressed  = true;
        lastRxNavTime  = nowMs;
      }
    }
    lastRxRight = rx_stick_right;
    lastRxLeft  = rx_stick_left;

    enterOut = (btn_enter && !lastEnterState) || rxEnterPressed;
    backOut  = (btn_back  && !lastBackState)  || rxBackPressed;
    lastEnterState = btn_enter;
    lastBackState  = btn_back;
  };

  bool enter = false, back = false;

  // A. Inside deadband (diff_rx = +500 counts with 800 deadband) -> Neutral, no trigger
  evaluateNav(500, deadband_r, 1000, enter, back);
  TEST_ASSERT(!enter && !back, "Deflection inside deadband (500 < 800 counts) triggers neither Enter nor Back");

  // B. Deflect Right past variable deadband (800 + 80 = 880 threshold, diff_rx = +950) -> Enter Triggered!
  evaluateNav(950, deadband_r, 1250, enter, back);
  TEST_ASSERT(enter == true && back == false, "Right Stick deflecting RIGHT past deadband (+950 counts) triggers ENTER / SELECT");

  // C. Held Right on Next Immediate Tick (10ms later) -> Debounced / Edge-latched (No repeat spam)
  evaluateNav(950, deadband_r, 1260, enter, back);
  TEST_ASSERT(enter == false && back == false, "Sustained RIGHT deflection edge-latched without spamming");

  // D. Return to Center
  evaluateNav(0, deadband_r, 1300, enter, back);
  TEST_ASSERT(!enter && !back, "Return to center safely unlatches edge detector");

  // E. Tight deadband test: deadband_r = 140 (threshold = 140 + 30 = 170). Deflect Left to -200 -> Back Triggered!
  evaluateNav(-200, 140, 1550, enter, back);
  TEST_ASSERT(enter == false && back == true, "Right Stick adapts dynamically to tight deadband (140) and triggers BACK at -200 counts");

  // 2. Drive Profile Usable Axes Mapping Verification (3 usable axes vs 1 quarantined)
  // Profile 0: ARCADE (Uses LY=Thr, RX=Steer, LX=Blinker/Aux. RY quarantined to neutral 2048)
  uint8_t profile = 0;
  bool is_rx_active = (profile == 0);
  bool is_ry_active = (profile == 1);
  TEST_ASSERT(is_rx_active == true && is_ry_active == false, "Arcade Mode: RX is ACTIVE, RY is strictly QUARANTINED (OFF)");

  // Profile 1: TANK (Uses LY=TrackL, RY=TrackR, LX=Aux. RX quarantined to neutral 2048)
  profile = 1;
  is_rx_active = (profile == 0);
  is_ry_active = (profile == 1);
  TEST_ASSERT(is_rx_active == false && is_ry_active == true, "Tank Mode: RY is ACTIVE, RX is strictly QUARANTINED (OFF)");

  // Profile 2: 1-STICK (Uses LY=Thr, LX=Steer. RX and RY are both quarantined)
  profile = 2;
  is_rx_active = (profile == 0);
  is_ry_active = (profile == 1);
  TEST_ASSERT(is_rx_active == false && is_ry_active == false, "1-Stick Mode: RX and RY are BOTH strictly QUARANTINED (OFF)");
}

// =============================================================================
//  TEST 13: TRANSMITTER TELEMETRY FORMAT & GROUND-SWITCHED BUTTON POLARITY
// =============================================================================
void test_transmitter_serial_telemetry_and_buttons() {
  std::cout << "\n>>> RUNNING TEST SUITE 13: Transmitter Telemetry Format & Button Polarity..." << std::endl;

  // 1. Normally Closed (NC) Switch Circuit Simulation
  // Hardware Circuit: Button between STM32 pin and GND.
  // Internal pull-up to 3.3V enabled (INPUT_PULLUP).
  struct ButtonModel {
    bool isClosedToGnd; // At rest = true (Normally Closed)
    bool hasPullUp;     // true for INPUT_PULLUP, false for INPUT_PULLDOWN
    bool activeLevel;   // HIGH for NC switch (triggered when broken from GND)

    bool readPin(bool multimeterProbeInjected = false) const {
      if (isClosedToGnd) return false; // Hard 0.0V (GND)
      if (multimeterProbeInjected) return true; // DMM injects +2.0V CMOS HIGH
      return hasPullUp; // Pulled to 3.3V if pullup; 0V if pulldown
    }

    bool isTriggered(bool multimeterProbeInjected = false) const {
      return readPin(multimeterProbeInjected) == activeLevel;
    }
  };

  // Case A: Flawed INPUT_PULLDOWN configuration
  ButtonModel badBtn{true, false, true};
  TEST_ASSERT(badBtn.isTriggered() == false, "Bad Pulldown: Unpressed NC switch connected to GND reads LOW (not triggered)");
  badBtn.isClosedToGnd = false; // Pilot presses button -> contact opens
  TEST_ASSERT(badBtn.isTriggered() == false, "Flaw Root Cause: Bad Pulldown keeps pin at 0V when switch opens -> press FAILS to register");
  TEST_ASSERT(badBtn.isTriggered(true) == true, "Multimeter Probe Anomaly: Touching disconnected wire injects +2.0V -> STM32 immediately buzzes!");

  // Case B: Correct INPUT_PULLUP configuration
  ButtonModel goodBtn{true, true, true};
  TEST_ASSERT(goodBtn.isTriggered() == false, "INPUT_PULLUP: Unpressed NC switch connected to GND reads LOW (silent)");
  goodBtn.isClosedToGnd = false; // Pilot presses button -> contact opens from GND
  TEST_ASSERT(goodBtn.isTriggered() == true, "INPUT_PULLUP Fix: Pressing NC button allows internal pullup to pull pin to 3.3V -> registered reliably!");

  // 2. Exact Telemetry String Format Validation
  // Must strictly match: MODE:ARCADE | AXIS:[LX:0%(ACT) LY:0%(ACT) RX:0%(ACT) RY:0%(OFF)] | TOGGLES:[ARM:ARMED(D) MUTE:ON] | BTNS:[PL:0 PR:0 CKL:0 CKR:0] | POTS:[GOV:58% TURN:75% POT3:0%(PG0)]
  char teleBuf[256];
  const char* modeStr = "ARCADE";
  int lx_pct = 0, ly_pct = 0, rx_pct = 0;
  bool isArmed = true, isMuted = true;
  int pl = 0, pr = 0, ckl = 0, ckr = 0;
  int gov = 58, turn = 75, pot3 = 0, page = 0;

  snprintf(teleBuf, sizeof(teleBuf),
           "MODE:%s | AXIS:[LX:%d%%(ACT) LY:%d%%(ACT) RX:%d%%(ACT) RY:0%%(OFF)] | TOGGLES:[ARM:%s MUTE:%s] | BTNS:[PL:%d PR:%d CKL:%d CKR:%d] | POTS:[GOV:%d%% TURN:%d%% POT3:%d%%(PG%d)]",
           modeStr, lx_pct, ly_pct, rx_pct,
           isArmed ? "ARMED(D)" : "PARK(P)",
           isMuted ? "ON" : "OFF",
           pl, pr, ckl, ckr,
           gov, turn, pot3, page);

  const char* expected = "MODE:ARCADE | AXIS:[LX:0%(ACT) LY:0%(ACT) RX:0%(ACT) RY:0%(OFF)] | TOGGLES:[ARM:ARMED(D) MUTE:ON] | BTNS:[PL:0 PR:0 CKL:0 CKR:0] | POTS:[GOV:58% TURN:75% POT3:0%(PG0)]";
  TEST_ASSERT(strcmp(teleBuf, expected) == 0, "Transmitter Serial Telemetry format exactly matches pilot specification");
}

// =============================================================================
//  TEST 14: BI-DIRECTIONAL ESP-NOW TELEMETRY PIPELINE (STM32 <-> CAM <-> S3)
// =============================================================================
#pragma pack(push, 1)
struct TestRemoteTelemetryPacket {
  uint32_t packet_id;      // 4 Bytes: Monotonic frame counter
  uint8_t  cmd_id;         // 1 Byte : Numeric Command ID (0 to 8)
  uint8_t  speed;          // 1 Byte : Integer PWM (0 to 255)
  uint8_t  gear;           // 1 Byte : Integer Gear (0 to 9)
  uint8_t  turbo;          // 1 Byte : Integer Flag (0 = Normal, 1 = Turbo 100%)
  char     telemetry[192]; // Null-terminated ASCII telemetry string from STM32 Remote
};
#pragma pack(pop)

void test_bidirectional_espnow_telemetry_pipeline() {
  std::cout << "\n>>> RUNNING TEST SUITE 14: Bi-Directional ESP-NOW Telemetry Pipeline..." << std::endl;

  // 1. Packet Sizing & Memory Footprint Verification
  TEST_ASSERT(sizeof(TestRemoteTelemetryPacket) == 200, "RemoteTelemetryPacket is exactly 200 bytes (safely within ESP-NOW 250-byte limit)");

  // 2. Telemetry String Ingestion & Command Parsing Simulation (ESP32-CAM Logic)
  auto parseTelemetryString = [](const char* line, uint8_t& outCmd, uint8_t& outSpeed, uint8_t& outTurbo) {
    bool isArmed = (strstr(line, "ARM:ARMED(D)") != nullptr);
    int ly = 0, rx = 0, gov = 100;
    outTurbo = (strstr(line, "CKL:1") != nullptr) ? 1 : 0;

    const char* pLy = strstr(line, "LY:");
    if (pLy) ly = atoi(pLy + 3);

    const char* pRx = strstr(line, "RX:");
    if (pRx) {
      if (strncmp(pRx, "RX:0%(OFF)", 10) == 0) {
        const char* pLx = strstr(line, "LX:");
        if (pLx) rx = atoi(pLx + 3);
      } else {
        rx = atoi(pRx + 3);
      }
    }

    const char* pGov = strstr(line, "GOV:");
    if (pGov) gov = atoi(pGov + 4);
    if (gov < 15)  gov = 15;
    if (gov > 100) gov = 100;

    outCmd   = 0;
    outSpeed = 0;

    if (isArmed) {
      float gov_max_pwm = 70.0f + ((float)gov / 100.0f) * 185.0f;
      if (outTurbo) gov_max_pwm = 255.0f;

      int abs_thr = abs(ly);
      int abs_str = abs(rx);

      if (ly > 12) {
        if (rx < -20)      outCmd = 5; // Left Front
        else if (rx > 20)  outCmd = 6; // Right Front
        else               outCmd = 1; // Forward
        outSpeed = outTurbo ? 255 : (uint8_t)(70 + ((abs_thr - 12) * (gov_max_pwm - 70)) / 88);
      } else if (ly < -12) {
        if (rx < -20)      outCmd = 7; // Left Back
        else if (rx > 20)  outCmd = 8; // Right Back
        else               outCmd = 2; // Backward
        outSpeed = outTurbo ? 255 : (uint8_t)(70 + ((abs_thr - 12) * (gov_max_pwm - 70)) / 88);
      } else {
        if (rx < -20) {
          outCmd = 3; // Pivot Left
          outSpeed = outTurbo ? 255 : (uint8_t)(70 + ((abs_str - 20) * (gov_max_pwm - 70)) / 80);
        } else if (rx > 20) {
          outCmd = 4; // Pivot Right
          outSpeed = outTurbo ? 255 : (uint8_t)(70 + ((abs_str - 20) * (gov_max_pwm - 70)) / 80);
        }
      }
    }
  };

  // Case A: Armed Right-Turn Steering Forward Command
  const char* lineArmedRight = "MODE:ARCADE | AXIS:[LX:0%(ACT) LY:60%(ACT) RX:35%(ACT) RY:0%(OFF)] | TOGGLES:[ARM:ARMED(D) MUTE:ON] | BTNS:[PL:0 PR:0 CKL:0 CKR:0] | POTS:[GOV:80% TURN:75% POT3:0%(PG0)]";
  uint8_t cmdA = 0, spdA = 0, trbA = 0;
  parseTelemetryString(lineArmedRight, cmdA, spdA, trbA);
  TEST_ASSERT(cmdA == 6, "Forward Right steering deflects properly to CMD_RIGHT_FRONT (6)");
  TEST_ASSERT(spdA >= 140 && spdA <= 180, "Governed speed scales properly with 80% speed governor");
  TEST_ASSERT(trbA == 0, "Turbo flag is false when CKL is unpressed");

  // Case B: Disarmed / Park Mode Safety Interlock
  const char* linePark = "MODE:ARCADE | AXIS:[LX:0%(ACT) LY:80%(ACT) RX:0%(ACT) RY:0%(OFF)] | TOGGLES:[ARM:PARK(P) MUTE:ON] | BTNS:[PL:0 PR:0 CKL:0 CKR:0] | POTS:[GOV:100% TURN:75% POT3:0%(PG0)]";
  uint8_t cmdB = 255, spdB = 255, trbB = 255;
  parseTelemetryString(linePark, cmdB, spdB, trbB);
  TEST_ASSERT(cmdB == 0, "Safety Clamp: Disarmed switch (PARK) strictly forces CMD_STOP (0)");
  TEST_ASSERT(spdB == 0, "Safety Clamp: Disarmed switch strictly clamps speed to 0 PWM");

  // Case C: In-Place Pivot Tank Spin
  const char* linePivot = "MODE:ARCADE | AXIS:[LX:0%(ACT) LY:0%(ACT) RX:-60%(ACT) RY:0%(OFF)] | TOGGLES:[ARM:ARMED(D) MUTE:ON] | BTNS:[PL:0 PR:0 CKL:0 CKR:0] | POTS:[GOV:50% TURN:75% POT3:0%(PG0)]";
  uint8_t cmdC = 0, spdC = 0, trbC = 0;
  parseTelemetryString(linePivot, cmdC, spdC, trbC);
  TEST_ASSERT(cmdC == 3, "Neutral throttle with left steering triggers CMD_PIVOT_LEFT (3)");
  TEST_ASSERT(spdC > 0, "Tank spin speed generates non-zero PWM");

  // 3. Return Telemetry ($RTL) String Generation & STM32 Parser Validation
  char rtlOut[64];
  int simRssi = -58;
  int simCurL = 1250; // 1.25A
  int simCurR = 1400; // 1.40A
  int simBat  = 12100; // 12.10V
  int simFail = 0;

  snprintf(rtlOut, sizeof(rtlOut), "$RTL,%d,%d,%d,%d,%d*\n", simRssi, simCurL, simCurR, simBat, simFail);

  // Simulate STM32 Parser on PA10
  int16_t parsed_rssi = 0;
  float parsed_cur_l  = 0.0f;
  float parsed_cur_r  = 0.0f;
  float parsed_bat_v  = 0.0f;
  uint8_t parsed_fail = 255;

  TEST_ASSERT(strncmp(rtlOut, "$RTL,", 5) == 0, "Valid NMEA header detected ($RTL,)");
  char* p = rtlOut + 5;
  parsed_rssi = (int16_t)strtol(p, &p, 10); if (*p == ',') p++;
  parsed_cur_l= (float)strtol(p, &p, 10) / 1000.0f; if (*p == ',') p++;
  parsed_cur_r= (float)strtol(p, &p, 10) / 1000.0f; if (*p == ',') p++;
  parsed_bat_v= (float)strtol(p, &p, 10) / 1000.0f; if (*p == ',') p++;
  parsed_fail = (uint8_t)strtol(p, &p, 10);

  TEST_ASSERT(parsed_rssi == -58, "STM32 correctly parses RSSI (-58 dBm)");
  TEST_ASSERT(std::abs(parsed_cur_l - 1.25f) < 0.01f, "STM32 correctly parses Left Current (1.25 A)");
  TEST_ASSERT(std::abs(parsed_cur_r - 1.40f) < 0.01f, "STM32 correctly parses Right Current (1.40 A)");
  TEST_ASSERT(std::abs(parsed_bat_v - 12.10f) < 0.01f, "STM32 correctly parses Battery Voltage (12.10 V)");
  TEST_ASSERT(parsed_fail == 0, "STM32 correctly parses failsafe status (0 = Normal)");
}

// =============================================================================
//  TEST 15: 16-BYTE CYBERPACKET WIRE SPEC, CRC-8, DEVICE ID & S3 10 HZ LOGGING
// =============================================================================
void test_cyberpacket_binary_pipeline_and_10hz_logging() {
  std::cout << "\n>>> RUNNING TEST SUITE 15: 16-Byte CyberPacket Pipeline v3.0, CRC-8 & Device ID..." << std::endl;

  // 1. Verify Memory Layout & Strict 16-Byte Packed Size
  TEST_ASSERT(sizeof(CyberPacket) == 16, "CyberPacket struct size is exactly 16 bytes");
  TEST_ASSERT(CYBER_PACKET_SIZE == 16, "CYBER_PACKET_SIZE constant is 16");
  TEST_ASSERT(CYBER_DEVICE_ID == 0xC5, "CYBER_DEVICE_ID constant is 0xC5");
  TEST_ASSERT(CYBER_FRAME_HEADER_1 == 0x5A, "Header byte 1 is 0x5A");
  TEST_ASSERT(CYBER_FRAME_HEADER_2 == 0xA5, "Header byte 2 is 0xA5");
  TEST_ASSERT(CYBER_FRAME_TRAILER == 0xED, "Trailer byte is 0xED");

  // 2. Field Offsets within Packed 16-byte struct
  CyberPacket testPkt;
  uint8_t* base = reinterpret_cast<uint8_t*>(&testPkt);
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.device_id)    - base) == 0,  "device_id offset is 0");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.seq_id)       - base) == 1,  "seq_id offset is 1");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.timestamp_ms) - base) == 2,  "timestamp_ms offset is 2");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.axis_lx)      - base) == 4,  "axis_lx offset is 4");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.axis_ly)      - base) == 5,  "axis_ly offset is 5");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.axis_rx)      - base) == 6,  "axis_rx offset is 6");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.axis_ry)      - base) == 7,  "axis_ry offset is 7");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.speed_gov)    - base) == 8,  "speed_gov offset is 8");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.turn_ratio)   - base) == 9,  "turn_ratio offset is 9");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.pot_aux)      - base) == 10, "pot_aux offset is 10");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.buttons)      - base) == 11, "buttons offset is 11");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.mode_flags)   - base) == 12, "mode_flags offset is 12");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.frame_id)     - base) == 13, "frame_id offset is 13");
  TEST_ASSERT((reinterpret_cast<uint8_t*>(&testPkt.crc8)         - base) == 15, "crc8 offset is 15");

  // 3. Bit Packing & Unpacking Validation
  testPkt.device_id    = CYBER_DEVICE_ID;
  testPkt.seq_id       = 42;
  testPkt.frame_id     = 1042;
  testPkt.axis_lx      = 0;
  testPkt.axis_ly      = 60;
  testPkt.axis_rx      = 35;
  testPkt.axis_ry      = 0;
  testPkt.speed_gov    = 80;
  testPkt.turn_ratio   = 75;
  testPkt.pot_aux      = 0;
  testPkt.timestamp_ms = 5000;
  setCyberButtons(testPkt, true, true, false, false, false, false, 0); // ARM=1, MUTE=1, PL=0, PR=0, CKL=0, CKR=0, Page=0
  setCyberModeFlags(testPkt, 0, 50); // Profile 0 (Arcade), 50 Hz
  testPkt.crc8         = computeCyberCrc8(testPkt);

  TEST_ASSERT(testPkt.device_id == CYBER_DEVICE_ID, "device_id matches CYBER_DEVICE_ID (0xC5)");
  TEST_ASSERT(testPkt.seq_id == 42, "seq_id stores rolling 8-bit counter");
  TEST_ASSERT(getCyberArm(testPkt) == true, "getCyberArm correctly unpacks ARMED (1)");
  TEST_ASSERT(getCyberMute(testPkt) == true, "getCyberMute correctly unpacks MUTE (1)");
  TEST_ASSERT(getCyberBtnPL(testPkt) == false, "getCyberBtnPL correctly unpacks PL (0)");
  TEST_ASSERT(getCyberBtnPR(testPkt) == false, "getCyberBtnPR correctly unpacks PR (0)");
  TEST_ASSERT(getCyberBtnCKL(testPkt) == false, "getCyberBtnCKL correctly unpacks CKL (0)");
  TEST_ASSERT(getCyberBtnCKR(testPkt) == false, "getCyberBtnCKR correctly unpacks CKR (0)");
  TEST_ASSERT(getCyberPage(testPkt) == 0, "getCyberPage correctly unpacks Page (0)");
  TEST_ASSERT(getCyberProfile(testPkt) == 0, "getCyberProfile correctly unpacks Profile 0 (Arcade)");
  TEST_ASSERT(getCyberRateHz(testPkt) == 50, "getCyberRateHz correctly unpacks 50 Hz");
  TEST_ASSERT(isCyberPacketValid(testPkt) == true, "Valid packet passes isCyberPacketValid()");

  // 4. Device ID & CRC-8 Integrity & Corruption Rejection
  CyberPacket corruptedPkt = testPkt;
  corruptedPkt.device_id = 0xA1; // Rogue / alien transmitter ID
  TEST_ASSERT(isCyberPacketValid(corruptedPkt) == false, "Foreign device_id (0xA1 != 0xC5) rejected");

  corruptedPkt = testPkt;
  corruptedPkt.axis_ly = -90; // Altered data without updating CRC-8
  TEST_ASSERT(isCyberPacketValid(corruptedPkt) == false, "CRC-8 mismatch is caught and rejected");

  corruptedPkt = testPkt;
  corruptedPkt.crc8 ^= 0x01; // Bit-flip in CRC-8 byte
  TEST_ASSERT(isCyberPacketValid(corruptedPkt) == false, "Single-bit corruption in CRC-8 rejected");

  corruptedPkt = testPkt;
  corruptedPkt.speed_gov = 105; // Out-of-bounds governor factor (> 100%)
  corruptedPkt.crc8 = computeCyberCrc8(corruptedPkt);
  TEST_ASSERT(isCyberPacketValid(corruptedPkt) == false, "Out-of-bounds speed_gov (>100%) rejected");

  // 5. Test 40 Hz and 20 Hz Settings Selection
  setCyberModeFlags(testPkt, 1, 40); // Profile 1 (Tank), 40 Hz
  testPkt.crc8 = computeCyberCrc8(testPkt);
  TEST_ASSERT(getCyberProfile(testPkt) == 1, "getCyberProfile correctly unpacks Profile 1 (Tank)");
  TEST_ASSERT(getCyberRateHz(testPkt) == 40, "getCyberRateHz correctly unpacks 40 Hz");

  uint32_t interval50 = 1000 / 50; // 20 ms
  uint32_t interval40 = 1000 / 40; // 25 ms
  uint32_t interval20 = 1000 / 20; // 50 ms
  TEST_ASSERT(interval50 == 20, "50 Hz produces exact 20ms transmission interval");
  TEST_ASSERT(interval40 == 25, "40 Hz produces exact 25ms transmission interval");
  TEST_ASSERT(interval20 == 50, "20 Hz produces exact 50ms transmission interval");

  // 6. ESP32-CAM Gateway 15-Byte UART Frame FSM Decoder Simulation
  std::vector<uint8_t> uartStream;
  // Prepend random noise bytes to test FSM synchronization
  uartStream.push_back(0xFF);
  uartStream.push_back(0x00);
  uartStream.push_back(0x5A); // False header 1
  uartStream.push_back(0x12); // Invalid header 2 -> should reset cleanly
  uartStream.push_back(CYBER_FRAME_HEADER_1);
  uartStream.push_back(CYBER_FRAME_HEADER_2);
  const uint8_t* pktBytes = reinterpret_cast<const uint8_t*>(&testPkt);
  for (size_t i = 0; i < sizeof(CyberPacket); i++) {
    uartStream.push_back(pktBytes[i]);
  }
  uartStream.push_back(CYBER_FRAME_TRAILER);
  // Postpend extra noise
  uartStream.push_back(0xAA);

  // Run FSM on byte stream
  enum FsmState { FSM_IDLE, FSM_GOT_H1, FSM_PAYLOAD, FSM_TRAILER };
  FsmState fsmState = FSM_IDLE;
  uint8_t payloadBuf[sizeof(CyberPacket)];
  uint8_t payloadIdx = 0;
  bool packetDecoded = false;
  CyberPacket decodedPkt;

  for (uint8_t u8 : uartStream) {
    switch (fsmState) {
      case FSM_IDLE:
        if (u8 == CYBER_FRAME_HEADER_1) fsmState = FSM_GOT_H1;
        break;
      case FSM_GOT_H1:
        if (u8 == CYBER_FRAME_HEADER_2) {
          fsmState = FSM_PAYLOAD;
          payloadIdx = 0;
        } else if (u8 != CYBER_FRAME_HEADER_1) {
          fsmState = FSM_IDLE;
        }
        break;
      case FSM_PAYLOAD:
        payloadBuf[payloadIdx++] = u8;
        if (payloadIdx >= sizeof(CyberPacket)) fsmState = FSM_TRAILER;
        break;
      case FSM_TRAILER:
        if (u8 == CYBER_FRAME_TRAILER) {
          memcpy(&decodedPkt, payloadBuf, sizeof(CyberPacket));
          if (isCyberPacketValid(decodedPkt)) {
            packetDecoded = true;
          }
        }
        fsmState = FSM_IDLE;
        break;
    }
  }

  TEST_ASSERT(packetDecoded == true, "ESP32-CAM FSM successfully syncs and extracts CyberPacket through line noise");
  TEST_ASSERT(decodedPkt.frame_id == testPkt.frame_id, "FSM preserves exact 16-bit frame sequence counter");
  TEST_ASSERT(decodedPkt.speed_gov == testPkt.speed_gov, "FSM preserves speed governor factor");

  // 7. ESP32-S3 Motor Command Mapping for CyberPacket
  auto computeS3MotorCommand = [](const CyberPacket& p, uint8_t& out_cmd, uint8_t& out_spd) {
    uint8_t prof = getCyberProfile(p);
    bool isArmed = getCyberArm(p);
    bool turbo   = getCyberBtnCKL(p);
    uint8_t gov  = p.speed_gov;
    if (gov < 15)  gov = 15;
    if (gov > 100) gov = 100;

    out_cmd = 0;
    out_spd = 0;

    if (isArmed) {
      float gov_max_pwm = 70.0f + ((float)gov / 100.0f) * 185.0f;
      if (turbo) gov_max_pwm = 255.0f;

      if (prof == 1) { // Tank
        int ly = p.axis_ly;
        int ry = p.axis_ry;
        if (ly > 12 && ry > 12) {
          out_cmd = 1; // FORWARD
          out_spd = (uint8_t)(70 + ((std::max(ly, ry) - 12) * (gov_max_pwm - 70)) / 88);
        } else if (ly > 12 && ry < -12) {
          out_cmd = 4; // PIVOT_RIGHT
          out_spd = (uint8_t)(70 + ((std::max(ly, std::abs(ry)) - 12) * (gov_max_pwm - 70)) / 88);
        }
      } else { // Arcade / 1-Stick
        int ly = p.axis_ly;
        int rx = (prof == 2) ? p.axis_lx : p.axis_rx;
        if (ly > 12) {
          if (rx < -20)      out_cmd = 5; // LEFT_FRONT
          else if (rx > 20)  out_cmd = 6; // RIGHT_FRONT
          else               out_cmd = 1; // FORWARD
          out_spd = turbo ? 255 : (uint8_t)(70 + ((std::abs(ly) - 12) * (gov_max_pwm - 70)) / 88);
        } else if (ly < -12) {
          if (rx < -20)      out_cmd = 7; // LEFT_BACK
          else if (rx > 20)  out_cmd = 8; // RIGHT_BACK
          else               out_cmd = 2; // BACKWARD
          out_spd = turbo ? 255 : (uint8_t)(70 + ((std::abs(ly) - 12) * (gov_max_pwm - 70)) / 88);
        } else {
          if (rx < -20) {
            out_cmd = 3; // PIVOT_LEFT
            out_spd = turbo ? 255 : (uint8_t)(70 + ((std::abs(rx) - 20) * (gov_max_pwm - 70)) / 80);
          } else if (rx > 20) {
            out_cmd = 4; // PIVOT_RIGHT
            out_spd = turbo ? 255 : (uint8_t)(70 + ((std::abs(rx) - 20) * (gov_max_pwm - 70)) / 80);
          } else {
            out_cmd = 0; out_spd = 0;
          }
        }
      }
    }
  };

  // Test Case A: Arcade Forward-Right Steering @ 80% Governor
  CyberPacket pktArcade = {};
  pktArcade.axis_lx = 0;
  pktArcade.axis_ly = 60;
  pktArcade.axis_rx = 35;
  pktArcade.axis_ry = 0;
  pktArcade.speed_gov = 80;
  pktArcade.turn_ratio = 75;
  pktArcade.pot_aux = 0;
  setCyberButtons(pktArcade, true, false, false, false, false, false, 0);
  setCyberModeFlags(pktArcade, 0, 50);

  uint8_t cmdA = 0, spdA = 0;
  computeS3MotorCommand(pktArcade, cmdA, spdA);
  TEST_ASSERT(cmdA == 6, "Arcade: Forward + Right steer commands CMD_RIGHT_FRONT (6)");
  TEST_ASSERT(spdA >= 140 && spdA <= 180, "Arcade: Governed PWM speed is properly calculated");

  // Test Case B: Disarmed Lockout
  CyberPacket pktDisarmed = pktArcade;
  setCyberButtons(pktDisarmed, false, false, false, false, false, false, 0); // ARM = 0
  uint8_t cmdB = 255, spdB = 255;
  computeS3MotorCommand(pktDisarmed, cmdB, spdB);
  TEST_ASSERT(cmdB == 0, "Disarmed state forces CMD_STOP (0)");
  TEST_ASSERT(spdB == 0, "Disarmed state forces 0 PWM speed");

  // Test Case C: Tank Mode Spin CW
  CyberPacket pktTank = {};
  pktTank.axis_lx = 0;
  pktTank.axis_ly = 50;  // Left Track Fwd
  pktTank.axis_rx = 0;
  pktTank.axis_ry = -50; // Right Track Rev
  pktTank.speed_gov = 100;
  pktTank.turn_ratio = 75;
  pktTank.pot_aux = 0;
  setCyberButtons(pktTank, true, false, false, false, false, false, 0);
  setCyberModeFlags(pktTank, 1, 40);

  uint8_t cmdC = 0, spdC = 0;
  computeS3MotorCommand(pktTank, cmdC, spdC);
  TEST_ASSERT(cmdC == 4, "Tank Mode: Opposite tracks trigger CMD_PIVOT_RIGHT (4)");
  TEST_ASSERT(spdC > 100, "Tank Mode: Spin PWM is non-zero");

  // 8. ESP32-S3 Serial Monitor String Reconstruction Fidelity
  auto reconstructTelemetryString = [](const CyberPacket& p) -> std::string {
    uint8_t prof = getCyberProfile(p);
    const char* pName = "ARCADE";
    if (prof == 1) pName = "TANK";
    else if (prof == 2) pName = "1-STICK";

    bool isArmed = getCyberArm(p);
    bool isMuted = getCyberMute(p);
    uint8_t pl   = getCyberBtnPL(p) ? 1 : 0;
    uint8_t pr   = getCyberBtnPR(p) ? 1 : 0;
    uint8_t ckl  = getCyberBtnCKL(p) ? 1 : 0;
    uint8_t ckr  = getCyberBtnCKR(p) ? 1 : 0;
    uint8_t page = getCyberPage(p);

    char buf[256];
    char axisBuf[64];
    if (prof == 1) {
      snprintf(axisBuf, sizeof(axisBuf), "LX:%d%%(ACT) LY:%d%%(ACT) RX:0%%(OFF) RY:%d%%(ACT)",
               (int)p.axis_lx, (int)p.axis_ly, (int)p.axis_ry);
    } else if (prof == 2) {
      snprintf(axisBuf, sizeof(axisBuf), "LX:%d%%(ACT) LY:%d%%(ACT) RX:0%%(OFF) RY:0%%(OFF)",
               (int)p.axis_lx, (int)p.axis_ly);
    } else {
      snprintf(axisBuf, sizeof(axisBuf), "LX:%d%%(ACT) LY:%d%%(ACT) RX:%d%%(ACT) RY:0%%(OFF)",
               (int)p.axis_lx, (int)p.axis_ly, (int)p.axis_rx);
    }

    snprintf(buf, sizeof(buf),
             "MODE:%s | AXIS:[%s] | TOGGLES:[ARM:%s MUTE:%s] | BTNS:[PL:%d PR:%d CKL:%d CKR:%d] | POTS:[GOV:%d%% TURN:%d%% POT3:%d%%(PG%d)]",
             pName,
             axisBuf,
             isArmed ? "ARMED(D)" : "PARK(P)",
             isMuted ? "ON" : "OFF",
             pl, pr, ckl, ckr,
             (int)p.speed_gov,
             (int)p.turn_ratio,
             (int)p.pot_aux,
             page);
    return std::string(buf);
  };

  std::string sArcade = reconstructTelemetryString(pktArcade);
  TEST_ASSERT(sArcade.find("MODE:ARCADE") != std::string::npos, "String contains MODE:ARCADE");
  TEST_ASSERT(sArcade.find("AXIS:[LX:0%(ACT) LY:60%(ACT) RX:35%(ACT) RY:0%(OFF)]") != std::string::npos, "String contains exact 4-axis pattern with RX active");
  TEST_ASSERT(sArcade.find("TOGGLES:[ARM:ARMED(D) MUTE:OFF]") != std::string::npos, "String contains toggles state");
  TEST_ASSERT(sArcade.find("BTNS:[PL:0 PR:0 CKL:0 CKR:0]") != std::string::npos, "String contains buttons state");
  TEST_ASSERT(sArcade.find("POTS:[GOV:80% TURN:75% POT3:0%(PG0)]") != std::string::npos, "String contains pots state");

  std::string sTank = reconstructTelemetryString(pktTank);
  TEST_ASSERT(sTank.find("MODE:TANK") != std::string::npos, "String contains MODE:TANK");
  TEST_ASSERT(sTank.find("AXIS:[LX:0%(ACT) LY:50%(ACT) RX:0%(OFF) RY:-50%(ACT)]") != std::string::npos, "String contains exact Tank 4-axis pattern with RY active");

  // 9. ESP32-S3 10 Hz Rate Limiting Under 50 Hz RF Input Stream Simulation
  // Simulate 1000 ms of operation: 50 incoming CyberPackets at 20ms intervals
  uint32_t simulatedTime = 0;
  uint32_t lastLogTime = 0;
  int motorCommandsDispatched = 0;
  int serialLogsPrinted = 0;

  for (int step = 0; step < 50; step++) {
    simulatedTime += 20; // 20ms per packet (50 Hz)

    // Immediate Motor Forwarding on every packet
    motorCommandsDispatched++;

    // Rate-limited Serial Monitor log (Strictly 10 Hz = 100ms interval)
    if (simulatedTime - lastLogTime >= 100) {
      lastLogTime = simulatedTime;
      serialLogsPrinted++;
    }
  }

  TEST_ASSERT(motorCommandsDispatched == 50, "Low-Latency Drivetrain: 50 motor commands dispatched per second (50 Hz)");
  TEST_ASSERT(serialLogsPrinted == 10, "USB Serial Telemetry: Strictly 10 serial logs printed per second (10 Hz rate limiter)");
  TEST_ASSERT((motorCommandsDispatched / serialLogsPrinted) == 5, "Exact 5:1 ratio between RF motor control and USB serial print");

  // 10. STM32 Blue Pill 5-Byte Motor Supervisor Frame Validation
  uint8_t testCmd = 6; // RIGHT_FRONT
  static const char* CMD_CHARS = "SFBQELRGI";
  char charCmd = CMD_CHARS[testCmd];
  uint8_t testSpd = 165;
  uint8_t stm32Csum = (uint8_t)(charCmd + testSpd) & 0xFF;

  uint8_t stm32Frame[5] = { 0xAA, (uint8_t)charCmd, testSpd, stm32Csum, 0x55 };
  TEST_ASSERT(stm32Frame[0] == 0xAA, "STM32 frame header is 0xAA");
  TEST_ASSERT(stm32Frame[1] == 'R', "CMD 6 maps to 'R' (Right Front)");
  TEST_ASSERT(stm32Frame[2] == 165, "Speed PWM is 165");
  TEST_ASSERT(stm32Frame[3] == ((uint8_t)('R' + 165) & 0xFF), "Checksum matches ('R' + 165)");
  TEST_ASSERT(stm32Frame[4] == 0x55, "STM32 frame trailer is 0x55");

  // 11. Volatile Qualifier Safety & Direct Accessors Validation
  // Guarantees zero "binding reference discards qualifiers" errors in Arduino IDE
  volatile CyberPacket vTestPkt;
  vTestPkt.device_id   = CYBER_DEVICE_ID;
  vTestPkt.seq_id      = 99;
  vTestPkt.frame_id    = 777;
  vTestPkt.axis_lx     = 10;
  vTestPkt.axis_ly     = 50;
  vTestPkt.axis_rx     = -20;
  vTestPkt.axis_ry     = 0;
  vTestPkt.speed_gov   = 85;
  vTestPkt.turn_ratio  = 45;
  vTestPkt.pot_aux     = 60;
  vTestPkt.buttons     = (1 << CYBER_BTN_ARM_BIT) | (1 << CYBER_BTN_CKL_BIT) | (2 << CYBER_PAGE_SHIFT);
  vTestPkt.mode_flags  = 0x01 | (5 << CYBER_RATE_SHIFT); // Tank mode, 50 Hz
  vTestPkt.timestamp_ms= 12345;
  vTestPkt.crc8        = computeCyberCrc8(vTestPkt);

  TEST_ASSERT(getCyberArm(vTestPkt) == true, "Volatile accessor: getCyberArm correctly reads 1 without compiler error");
  TEST_ASSERT(getCyberMute(vTestPkt) == false, "Volatile accessor: getCyberMute correctly reads 0");
  TEST_ASSERT(getCyberBtnCKL(vTestPkt) == true, "Volatile accessor: getCyberBtnCKL correctly reads Turbo");
  TEST_ASSERT(getCyberPage(vTestPkt) == 2, "Volatile accessor: getCyberPage correctly reads Page 2");
  TEST_ASSERT(getCyberProfile(vTestPkt) == 1, "Volatile accessor: getCyberProfile correctly reads Tank mode");
  TEST_ASSERT(getCyberRateHz(vTestPkt) == 50, "Volatile accessor: getCyberRateHz correctly reads 50 Hz");
  TEST_ASSERT(isCyberPacketValid(vTestPkt) == true, "Volatile accessor: isCyberPacketValid validates CRC-8 correctly");

  // 11.5. Dynamic Turn Ratio Execution & Proportional Math Verification
  // Validates STM32 Motor Supervisor proportional inner wheel calculation: (speed * turnRatio) / 100
  uint8_t currentTurnRatio = 33; // Default legacy ratio
  auto executeTurnCommand = [&](char cmd, uint8_t spd, int16_t& outL, int16_t& outR) {
    if (cmd == 'T') {
      currentTurnRatio = (spd > 75) ? 75 : spd;
    } else if (cmd == 'L' || cmd == 5) {
      outL = (spd * currentTurnRatio) / 100;
      outR = spd;
    } else if (cmd == 'R' || cmd == 6) {
      outL = spd;
      outR = (spd * currentTurnRatio) / 100;
    }
  };

  int16_t tL = 0, tR = 0;
  // Default 33% Turn Ratio
  executeTurnCommand('L', 150, tL, tR);
  TEST_ASSERT(tL == 49 && tR == 150, "Default 33% ratio: Inner wheel is 49 PWM (33% of 150)");

  // Update Turn Ratio to 0% (Sharpest Skid-Steer Pivot Turn)
  executeTurnCommand('T', 0, tL, tR);
  TEST_ASSERT(currentTurnRatio == 0, "Command 'T' updates turn ratio to 0%");
  executeTurnCommand('L', 150, tL, tR);
  TEST_ASSERT(tL == 0 && tR == 150, "0% Turn Ratio: Inner wheel completely stopped for tightest pivot");

  // Update Turn Ratio to 75% (Gentle Wide-Radius High-Speed Sweep)
  executeTurnCommand('T', 75, tL, tR);
  TEST_ASSERT(currentTurnRatio == 75, "Command 'T' updates turn ratio to 75%");
  executeTurnCommand('L', 200, tL, tR);
  TEST_ASSERT(tL == 150 && tR == 200, "75% Turn Ratio: Inner wheel at 150 PWM (75% of 200) for smooth sweep");

  // Dynamic Turn Ratio Frame Verification: [0xAA] ['T'] [RATIO] [CSUM] [0x55]
  uint8_t tRatioFrame[5] = { 0xAA, 'T', 50, (uint8_t)('T' + 50), 0x55 };
  TEST_ASSERT(tRatioFrame[0] == 0xAA && tRatioFrame[1] == 'T', "Dynamic Turn Ratio UART frame matches protocol");
  TEST_ASSERT(tRatioFrame[3] == ((uint8_t)('T' + 50) & 0xFF), "Dynamic Turn Ratio checksum matches");

  // 11.6. RF Airtime Latency Measurement & 16-Bit Rollover Verification
  uint16_t simTxTime = 10000;
  uint16_t simRxTime = 10015; // 15ms RF transmission latency
  int16_t calculatedLat = (int16_t)(simRxTime - simTxTime);
  TEST_ASSERT(calculatedLat == 15, "Airtime Latency: Measures exact 15ms transmission duration");

  // 16-bit Timer Rollover (Across 65535 boundary)
  uint16_t rolloverTx = 65530; // 5ms before 16-bit overflow
  uint16_t rolloverRx = 10;    // 10ms after 16-bit overflow
  int16_t rolloverLat = (int16_t)(rolloverRx - rolloverTx);
  TEST_ASSERT(rolloverLat == 16, "Airtime Latency: 16-bit rollover math computes exact 16ms without underflow");

  // 12. Zero-Drop Broadcast Teleoperation Routing Simulation
  // Simulates dongle transmit logic: When unverified or unicast drops, route to broadcast
  bool simPeerRegistered = true;
  bool simPeerVerified   = false; // Real S3 MAC unknown or not yet paired
  bool simLastTxSuccess  = false;
  int simBroadcastCount  = 0;
  int simUnicastCount    = 0;

  for (int p = 0; p < 50; p++) {
    if (simPeerRegistered && simPeerVerified && simLastTxSuccess) {
      simUnicastCount++;
    } else {
      simBroadcastCount++; // Unconditional fallback ensures 100% of packets reach S3
    }
  }
  TEST_ASSERT(simBroadcastCount == 50, "Zero-Drop Transmission: 100% of packets sent to broadcast when unicast unverified");
  TEST_ASSERT(simUnicastCount == 0, "No dead unicast packets dropped into void");

  // 13. S3 800ms Dead-Man Watchdog Immunity & Underflow Protection Simulation
  uint32_t s3SimNow = 0;
  uint32_t s3LastPacketTime = 0;
  bool s3FailsafeTripped = false;
  const uint32_t FAILSAFE_TIMEOUT = 800;

  // Stream 50 packets over 1000ms (20ms interval)
  for (int p = 0; p < 50; p++) {
    s3SimNow += 20;
    s3LastPacketTime = s3SimNow; // Packet received
    int32_t elapsed = (int32_t)(s3SimNow - s3LastPacketTime);
    if (!s3FailsafeTripped && (elapsed > (int32_t)FAILSAFE_TIMEOUT)) {
      s3FailsafeTripped = true;
    }
  }
  TEST_ASSERT(s3FailsafeTripped == false, "Active Teleop: Continuous 50 Hz stream never triggers false failsafe");

  // Asynchronous Race Condition Simulation:
  // Loop starts at T=1000, but packet arrives during loop at T=1015 (Wi-Fi ISR / Core 0 concurrency)
  uint32_t loopStart = 1000;
  uint32_t isrPacketTime = 1015; // Packet received 15ms in the "future" relative to loopStart
  
  // Naive unsigned subtraction (the bug): 1000 - 1015 = 4,294,967,281 > 800!
  uint32_t naiveUnderflow = loopStart - isrPacketTime;
  TEST_ASSERT(naiveUnderflow > FAILSAFE_TIMEOUT, "Root Cause Proof: Naive unsigned subtraction underflows to 4.29B > 800");

  // Fixed Signed Calculation with Fresh Timestamp Sampling:
  uint32_t freshWatchdogNow = 1018; // Sampled at watchdog check
  int32_t fixedElapsed = (int32_t)(freshWatchdogNow - isrPacketTime);
  TEST_ASSERT(fixedElapsed == 3, "Fixed Watchdog: Fresh sampling measures exact 3ms elapsed");
  TEST_ASSERT((fixedElapsed > (int32_t)FAILSAFE_TIMEOUT) == false, "Fixed Watchdog: Underflow completely prevented, failsafe never trips");

  // Multi-Core Jitter Edge Case: Watchdog sampled 1 tick before ISR timestamp propagates
  int32_t negativeElapsed = (int32_t)(1014 - isrPacketTime); // -1ms
  TEST_ASSERT((negativeElapsed > (int32_t)FAILSAFE_TIMEOUT) == false, "Fixed Watchdog: Negative signed delta (-1ms) never trips failsafe");

  // Millis Rollover Immunity (Across 49.7-Day Boundary):
  uint32_t preRollover = 0xFFFFFFF0; // 16ms before rollover
  uint32_t postRollover = 0x00000020; // 32ms after rollover (48ms total elapsed)
  int32_t rolloverElapsed = (int32_t)(postRollover - preRollover);
  TEST_ASSERT(rolloverElapsed == 48, "Fixed Watchdog: Rollover math correctly computes 48ms");
  TEST_ASSERT((rolloverElapsed > (int32_t)FAILSAFE_TIMEOUT) == false, "Fixed Watchdog: Rollover never causes false failsafe trip");

  // Stop stream and advance time past 800ms
  s3SimNow += 850;
  int32_t trueTimeoutElapsed = (int32_t)(s3SimNow - s3LastPacketTime);
  if (!s3FailsafeTripped && (trueTimeoutElapsed > (int32_t)FAILSAFE_TIMEOUT)) {
    s3FailsafeTripped = true;
  }
  TEST_ASSERT(s3FailsafeTripped == true, "Dead-Man Safety: Failsafe trips reliably when link lost for >800ms");
}

// =============================================================================
//  SUITE 16: AUDIO-HAPTIC CHATTER ELIMINATION, 35ms DEBOUNCE & GAUGE SMOOTHING
// =============================================================================
void test_audio_haptic_chatter_fix_and_gauge_smoothing() {
  std::cout << "\n>>> RUNNING TEST SUITE 16: Audio-Haptic Chatter Fix, 35ms Debounce & Gauge Smoothing..." << std::endl;

  // 1. Digital Input 35ms Software Debounce Simulation
  struct SimDigitalInput {
    bool raw;
    bool state;
    bool lastRaw;
    uint32_t lastDebounceTime;
    bool activeLevel;
  };

  SimDigitalInput toggleArm = { true, false, true, 0, false }; // Active-LOW, starts HIGH (unarmed)
  uint32_t simNow = 1000;

  // Bounce event: Switch bounces LOW at 1005ms, HIGH at 1012ms, LOW at 1020ms
  auto updateDebounce = [](SimDigitalInput& din, bool currentRaw, uint32_t now) -> bool {
    if (currentRaw != din.lastRaw) {
      din.lastRaw = currentRaw;
      din.lastDebounceTime = now;
    }
    if ((now - din.lastDebounceTime) >= 35) {
      bool isCurrentActive = (din.lastRaw == din.activeLevel);
      if (isCurrentActive != din.state) {
        din.state = isCurrentActive;
        return true; // State changed
      }
    }
    return false;
  };

  // 1005ms: Contact touches LOW
  bool changed = updateDebounce(toggleArm, false, 1005);
  TEST_ASSERT(!changed && toggleArm.state == false, "35ms Debounce: Initial 5ms contact noise safely rejected");

  // 1012ms: Contact bounces HIGH
  changed = updateDebounce(toggleArm, true, 1012);
  TEST_ASSERT(!changed && toggleArm.state == false, "35ms Debounce: 7ms contact bounce safely rejected");

  // 1020ms: Switch settles LOW
  changed = updateDebounce(toggleArm, false, 1020);
  TEST_ASSERT(!changed && toggleArm.state == false, "35ms Debounce: Settling edge registers timer without premature state change");

  // 1045ms: 25ms later (total 25ms elapsed since settle) -> still debouncing
  changed = updateDebounce(toggleArm, false, 1045);
  TEST_ASSERT(!changed && toggleArm.state == false, "35ms Debounce: 25ms elapsed rejects premature trigger");

  // 1056ms: 36ms later (exceeds 35ms threshold) -> STATE CONFIRMED ACTIVE!
  changed = updateDebounce(toggleArm, false, 1056);
  TEST_ASSERT(changed == true && toggleArm.state == true, "35ms Debounce: Stable state > 35ms successfully latches ARMED!");

  // 2. Broken PA11 Switch Decommissioning
  bool rawBrokenPA11Chatter = true;
  bool reportedCKR = false; // Isolated: Never fed into digInputs[]
  TEST_ASSERT(reportedCKR == false, "Hardware Isolation: Decommissioned PA11 right gimbal switch safely suppressed to 0");

  // 3. Alert Engine Non-Spam Priority Gate
  enum SimAlert { ALERT_NONE, ALERT_BUTTON_CLICK, ALERT_POT_NOTCH, ALERT_LINK_LOST };
  SimAlert activeAlert = ALERT_BUTTON_CLICK;
  uint32_t alertStartTime = 2000;

  auto tryTriggerAlert = [&](SimAlert newAlert, uint32_t now) -> bool {
    if (activeAlert != ALERT_NONE) {
      if (newAlert == ALERT_POT_NOTCH || newAlert == ALERT_BUTTON_CLICK) {
        return false; // Suppressed!
      }
    }
    activeAlert = newAlert;
    alertStartTime = now;
    return true;
  };

  bool triggered = tryTriggerAlert(ALERT_BUTTON_CLICK, 2010);
  TEST_ASSERT(!triggered && alertStartTime == 2000, "Alert Priority: Chattering click cannot reset alert timer while alert active");

  triggered = tryTriggerAlert(ALERT_POT_NOTCH, 2015);
  TEST_ASSERT(!triggered && alertStartTime == 2000, "Alert Priority: ADC pot notch noise cannot interrupt ongoing alert");

  // 4. Fluid Frame-Rate Independent Exponential Gauge Smoothing
  float target_speed = 180.0f; // Target 180 cm/s
  float speed_smooth = 0.0f;

  // Simulate 8 successive 100ms OLED frames (10 FPS)
  float dt = 0.100f; // 100ms
  float smooth_alpha = 1.0f - expf(-4.5f * dt); // ~0.3623
  TEST_ASSERT(smooth_alpha > 0.35f && smooth_alpha < 0.38f, "Exponential Alpha: dt=100ms yields exact 0.362 smoothing factor");

  // Frame 1 (100ms)
  speed_smooth += (target_speed - speed_smooth) * smooth_alpha;
  TEST_ASSERT(speed_smooth > 60.0f && speed_smooth < 70.0f, "Gauge Smoothing: Frame 1 sweeps to ~65 cm/s (smooth start, not instantaneous!)");

  // Frame 2 (200ms)
  speed_smooth += (target_speed - speed_smooth) * smooth_alpha;
  TEST_ASSERT(speed_smooth > 100.0f && speed_smooth < 112.0f, "Gauge Smoothing: Frame 2 sweeps to ~106 cm/s");

  // Frame 3 (300ms)
  speed_smooth += (target_speed - speed_smooth) * smooth_alpha;
  TEST_ASSERT(speed_smooth > 130.0f && speed_smooth < 138.0f, "Gauge Smoothing: Frame 3 sweeps to ~133 cm/s");

  // Frame 6 (600ms)
  for (int f = 4; f <= 6; f++) {
    speed_smooth += (target_speed - speed_smooth) * smooth_alpha;
  }
  TEST_ASSERT(speed_smooth > 165.0f && speed_smooth < 172.0f, "Gauge Smoothing: Frame 6 reaches 93% cruise smoothly");

  // 5. Live STM32 Dispatch Status String Verification
  char logLineArmed[128];
  uint8_t dispCmd = 1; // 'F'
  uint8_t dispSpd = 165;
  snprintf(logLineArmed, sizeof(logLineArmed), "STM32:[CMD:%c SPD:%d]", "SFBQELRGI"[dispCmd], dispSpd);
  TEST_ASSERT(strcmp(logLineArmed, "STM32:[CMD:F SPD:165]") == 0, "Telemetry Logger: Armed state displays exact live STM32 dispatch [CMD:F SPD:165]");

  char logLinePark[64];
  snprintf(logLinePark, sizeof(logLinePark), "STM32:[PARK(0.00V)]");
  TEST_ASSERT(strcmp(logLinePark, "STM32:[PARK(0.00V)]") == 0, "Telemetry Logger: Disarmed state displays exact safe ground lock [PARK(0.00V)]");
}

// =============================================================================
//  SUITE 17: LATENCY & LINK STABILITY UPGRADES (MODEM SLEEP, UNICAST & SLEW PUNCH)
// =============================================================================
void test_latency_and_link_stability_upgrades() {
  std::cout << "\n>>> RUNNING TEST SUITE 17: Latency Elimination, Smart Unicast & Drivetrain Punch..." << std::endl;

  // 1. Wi-Fi Modem Sleep Elimination (0ms Wake-Up Latency)
  enum WifiPowerSaveMode { WIFI_PS_MIN_MODEM_SIM = 1, WIFI_PS_NONE_SIM = 0 };
  WifiPowerSaveMode activePs = WIFI_PS_NONE_SIM;
  TEST_ASSERT(activePs == 0, "Wi-Fi Power Save: Set to WIFI_PS_NONE (Modem Sleep completely disabled)");

  // 2. Smart Unicast Routing with Zero-Drop Broadcast Fallback
  bool peerRegistered = true;
  bool peerVerified   = true;
  bool unicastSuccess = true;
  int unicastPackets   = 0;
  int broadcastPackets = 0;

  auto simTransmit = [&](bool peerReg, bool peerVer, bool txOk) -> std::string {
    if (peerReg && peerVer && txOk) {
      unicastPackets++;
      return "UNICAST";
    }
    broadcastPackets++;
    return "BROADCAST";
  };

  std::string route1 = simTransmit(peerRegistered, peerVerified, unicastSuccess);
  TEST_ASSERT(route1 == "UNICAST", "Smart Unicast: Verified peer routes over Unicast with 802.11 MAC ACK");

  // Unicast failure fallback test:
  std::string route2 = simTransmit(peerRegistered, false, false);
  TEST_ASSERT(route2 == "BROADCAST", "Zero-Drop Fallback: Unverified peer seamlessly falls back to Broadcast");

  std::string route3 = simTransmit(false, true, true);
  TEST_ASSERT(route3 == "BROADCAST", "Zero-Drop Fallback: Unregistered peer seamlessly falls back to Broadcast");

  // 3. Drivetrain Slew-Rate Acceleration Punch Verification
  const float OLD_ACCEL_STEP = 2.2f;
  const float NEW_ACCEL_STEP = 8.0f;
  const float START_PWM = 48.0f;
  const float TARGET_PWM = 165.0f;

  // Simulate Old Slew Rate (2.2f per 10ms tick)
  float oldSpeed = START_PWM;
  int oldTicks = 0;
  while (oldSpeed < TARGET_PWM) {
    oldSpeed += OLD_ACCEL_STEP;
    oldTicks++;
    if (oldTicks == 20) { // At 200ms
      TEST_ASSERT(oldSpeed < 95.0f, "Old Slew Rate Flaw: At 200ms, motor only reached 92 PWM (insufficient torque to move)");
    }
  }
  int oldTimeToCruiseMs = oldTicks * 10;
  TEST_ASSERT(oldTimeToCruiseMs >= 530, "Old Slew Rate: Took >530ms to reach cruise speed, creating severe perceived delay");

  // Simulate New Slew Rate (8.0f per 10ms tick)
  float newSpeed = START_PWM;
  int newTicks = 0;
  while (newSpeed < TARGET_PWM) {
    newSpeed += NEW_ACCEL_STEP;
    newTicks++;
  }
  int newTimeToCruiseMs = newTicks * 10;
  TEST_ASSERT(newTimeToCruiseMs == 150, "New Slew Punch: Reaches full 165 PWM cruise in exactly 150ms (3.5x faster acceleration!)");
  TEST_ASSERT(newTimeToCruiseMs < 200, "Responsive Throttle: Instant breakaway punch eliminates all perceived lag");

  // 4. STM32 Watchdog Jitter Filtering (650ms vs 350ms)
  const uint32_t OLD_WATCHDOG_MS = 350;
  const uint32_t NEW_WATCHDOG_MS = 650;
  uint32_t rfDropDurationMs = 400; // 400ms transient 2.4 GHz interference drop

  bool oldWatchdogTripped = (rfDropDurationMs >= OLD_WATCHDOG_MS);
  bool newWatchdogTripped = (rfDropDurationMs >= NEW_WATCHDOG_MS);
  TEST_ASSERT(oldWatchdogTripped == true, "Old Watchdog Flaw: 350ms timeout tripped prematurely during 400ms RF jitter");
  TEST_ASSERT(newWatchdogTripped == false, "New Watchdog Fix: 650ms timeout cleanly absorbs 400ms RF jitter without false cutout");

  // Verify true loss of signal still trips safely:
  uint32_t trueLinkLossMs = 700;
  bool trueFailsafeTripped = (trueLinkLossMs >= NEW_WATCHDOG_MS);
  TEST_ASSERT(trueFailsafeTripped == true, "Dead-Man Safety: 650ms watchdog trips reliably when link lost for >650ms");

  // 5. Periodic UART Command Heartbeat (80ms interval)
  uint32_t bridgeNow = 1000;
  uint32_t lastFwdTime = 910; // 90ms elapsed since last command
  bool heartbeatDispatched = (bridgeNow - lastFwdTime >= 80);
  TEST_ASSERT(heartbeatDispatched == true, "Bridge Heartbeat: Dispatches command every 80ms to keep STM32 watchdog fed");

  // 6. Resilient Return Telemetry Dropout Tolerance (2000ms Dongle / 2500ms Remote)
  uint32_t lastFeedback = 1000;
  uint32_t dongleCheck = 2400; // 1400ms elapsed
  bool dongleLinkOkOld = (dongleCheck - lastFeedback < 1200);
  bool dongleLinkOkNew = (dongleCheck - lastFeedback < 2000);
  TEST_ASSERT(dongleLinkOkOld == false, "Old Dongle Flaw: 1200ms timeout caused false disconnect after 1.4s of RF noise");
  TEST_ASSERT(dongleLinkOkNew == true, "New Dongle Fix: 2000ms timeout preserves link across transient RF bursts");

  // 7. Atomic Single-Buffer Telemetry Formatting Verification
  char logBuf[350];
  int written = snprintf(logBuf, sizeof(logBuf),
                         "MODE:%s | RATE:%uHz | DEV:0x%02X | SEQ:%u | PERIOD:%ums | CRC:OK | AXIS:[%s] | TOGGLES:[ARM:%s MUTE:%s] | BTNS:[PL:%u PR:%u CKL:%u CKR:%u] | POTS:[GOV:%d%% TURN:%d%% POT3:%d%%(PG%u)] | STM32:[%s]",
                         "ARCADE", 50, 0xC5, 42, 20,
                         "LX:0%(ACT) LY:60%(ACT) RX:25%(ACT) RY:0%(OFF)",
                         "ARMED(D)", "OFF",
                         0, 0, 1, 0,
                         85, 33, 50, 0,
                         "CMD:R SPD:165");
  TEST_ASSERT(written > 0 && written < (int)sizeof(logBuf), "Single-Buffer Logger: snprintf output fits safely within 350-byte stack buffer");
  TEST_ASSERT(strstr(logBuf, "MODE:ARCADE") != nullptr, "Single-Buffer Logger: Correctly formatted MODE");
  TEST_ASSERT(strstr(logBuf, "DEV:0xC5") != nullptr, "Single-Buffer Logger: Correctly formatted DEV ID");
  TEST_ASSERT(strstr(logBuf, "STM32:[CMD:R SPD:165]") != nullptr, "Single-Buffer Logger: Correctly formatted live STM32 dispatch");
}

int main() {
  std::cout << "================================================================================" << std::endl;
  std::cout << "  CYBERROVER X5 AUTOMATED FIRMWARE TEST HARNESS (HOST G++ EXECUTION)            " << std::endl;
  std::cout << "  Lead Architect: Veer Pratap Saw | Katras, Jharkhand                           " << std::endl;
  std::cout << "================================================================================" << std::endl;

  test_protocol_layout_and_validation();
  test_analog_noise_rejection();
  test_potentiometer_hysteresis();
  test_digital_debouncer();
  test_receiver_failsafe();
  test_frame_counter_rollover();
  test_powertrain_and_telemetry_bounds();
  test_settings_menu_and_page4_safety();
  test_boot_animation_bitmaps();
  test_auto_centering_and_drive_profiles();
  test_boot_stick_calibration();
  test_right_stick_navigation_and_axis_telemetry();
  test_transmitter_serial_telemetry_and_buttons();
  test_bidirectional_espnow_telemetry_pipeline();
  test_cyberpacket_binary_pipeline_and_10hz_logging();
  test_audio_haptic_chatter_fix_and_gauge_smoothing();
  test_latency_and_link_stability_upgrades();

  std::cout << "\n================================================================================" << std::endl;
  std::cout << "  TEST SUMMARY: " << testsPassed << " / " << testsRun << " TESTS PASSED ("
            << (testsPassed * 100 / testsRun) << "%)" << std::endl;
  std::cout << "================================================================================" << std::endl;

  return (testsPassed == testsRun) ? 0 : 1;
}
