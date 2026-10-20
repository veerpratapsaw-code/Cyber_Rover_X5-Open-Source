/*
 * ================================================================================
 * CYBERROVER X5 — UNIFIED RF TELEMETRY & CONTROL PROTOCOL SPECIFICATION (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCUs    : STM32F103C8T6 (Remote TX & Rover Carrier Node)
 * Protocol Spec  : 24-Byte Little-Endian Packed Binary Struct over NRF24L01+
 *
 * SAFETY & COMPATIBILITY GUARANTEES:
 *   1. Zero Alignment Padding: sizeof(RoverPacket) is exactly 24 bytes.
 *   2. Endianness: Little-Endian (Standard ARM Cortex-M3 architecture).
 *   3. Range Constraints: All 7 analog channels are bounded to 12-bit ADC (0 - 4095).
 *   4. Digital Inputs: Bounded to binary states (0 or 1).
 *   5. Frame Integrity: Monotonically increasing 32-bit frame counter.
 * ================================================================================
 */

#ifndef CYBERROVER_PROTOCOL_H
#define CYBERROVER_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Protocol Constants
#define CYBERROVER_PROTOCOL_VERSION  1
#define CYBERROVER_PACKET_SIZE       24
#define CYBERROVER_RF_CHANNEL        108            // 2.508 GHz (Above standard Wi-Fi channels 1-13)
#define CYBERROVER_RF_PIPE           0xE8E8F0F0E1LL // 40-bit hardware RF pipe address

// Dynamic Control Thresholds
#define ADC_RESOLUTION_MAX           4095
#define ADC_NOMINAL_CENTER           2048
#define ADC_DEFAULT_DEADBAND         140            // 1908 to 2188 reads neutral
#define MAX_SPEED_CMPS               314            // 314 cm/s = 11.31 km/h top sprint
#define MAX_MOTOR_RPM                600            // 600 RPM gearhead output

#pragma pack(push, 1)
/**
 * @brief Unified 13-Channel Telemetry & Control Packet (24 Bytes)
 */
struct RoverPacket {
  uint16_t stick_lx;    // Left Stick X   (0 - 4095) [Steering / Turn Blinkers]
  uint16_t stick_ly;    // Left Stick Y   (0 - 4095) [Throttle / Speed & RPM]
  uint16_t stick_rx;    // Right Stick X  (0 - 4095, Inverted on TX)
  uint16_t stick_ry;    // Right Stick Y  (0 - 4095) [Camera Tilt]
  uint16_t pot_l;       // Pot Left       (0 - 4095, Speed Governor Factor)
  uint16_t pot_c;       // Pot Center     (0 - 4095, Camera Tilt Trim)
  uint16_t pot_r;       // Pot Right      (0 - 4095, Aux / Battery Sim)
  uint8_t  toggle_l;    // 1 = Armed ('D'), 0 = Disarmed / Park ('P')
  uint8_t  toggle_r;    // 1 = Mute ON, 0 = Sound Active [PB4 Switch]
  uint8_t  push_l;      // 1 = PRESSED, 0 = RELEASED [PA8]
  uint8_t  push_r;      // 1 = PRESSED, 0 = RELEASED [PA15]
  uint8_t  stick_clk_l; // 1 = PRESSED, 0 = UP [PA10]
  uint8_t  stick_clk_r; // 1 = PRESSED, 0 = UP [PA11]
  uint32_t frame_id;    // Rolling sequence counter (0 to 4,294,967,295)
};
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// Compile-time static assertion to strictly enforce 24-byte packet layout
static_assert(sizeof(RoverPacket) == CYBERROVER_PACKET_SIZE, 
              "CRITICAL DEFECT: RoverPacket memory layout must be exactly 24 bytes!");

/**
 * @brief Strict Packet Sanity Validator.
 * Rejects corrupt packets caused by SPI bus floating (0xFFFF), cable brownouts, or memory corruption.
 * @param pkt Packet to validate.
 * @return true if all fields conform to physical bounds, false otherwise.
 */
inline bool isRoverPacketValid(const RoverPacket& pkt) {
  if (pkt.stick_lx > ADC_RESOLUTION_MAX || pkt.stick_ly > ADC_RESOLUTION_MAX ||
      pkt.stick_rx > ADC_RESOLUTION_MAX || pkt.stick_ry > ADC_RESOLUTION_MAX ||
      pkt.pot_l    > ADC_RESOLUTION_MAX || pkt.pot_c    > ADC_RESOLUTION_MAX || pkt.pot_r > ADC_RESOLUTION_MAX) {
    return false;
  }
  if (pkt.toggle_l > 1 || pkt.toggle_r > 1 ||
      pkt.push_l   > 1 || pkt.push_r   > 1 ||
      pkt.stick_clk_l > 1 || pkt.stick_clk_r > 1) {
    return false;
  }
  return true;
}
#endif

#endif // CYBERROVER_PROTOCOL_H
