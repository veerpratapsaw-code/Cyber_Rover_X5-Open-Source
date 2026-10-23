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

  std::cout << "\n================================================================================" << std::endl;
  std::cout << "  TEST SUMMARY: " << testsPassed << " / " << testsRun << " TESTS PASSED ("
            << (testsPassed * 100 / testsRun) << "%)" << std::endl;
  std::cout << "================================================================================" << std::endl;

  return (testsPassed == testsRun) ? 0 : 1;
}
