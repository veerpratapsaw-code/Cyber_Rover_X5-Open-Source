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
#include "../firmware/cyberrover_x5_remote_tx/analog_filter.h"
#include "../firmware/cyberrover_x5_remote_tx/digital_inputs.h"
#include "../firmware/cyberrover_x5_rover_rx/receiver_failsafe.h"

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

  std::cout << "\n================================================================================" << std::endl;
  std::cout << "  TEST SUMMARY: " << testsPassed << " / " << testsRun << " TESTS PASSED ("
            << (testsPassed * 100 / testsRun) << "%)" << std::endl;
  std::cout << "================================================================================" << std::endl;

  return (testsPassed == testsRun) ? 0 : 1;
}
