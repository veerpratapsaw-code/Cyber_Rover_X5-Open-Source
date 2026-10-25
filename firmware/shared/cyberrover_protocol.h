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
  uint8_t  stick_clk_l; // 1 = PRESSED, 0 = UP [PA12]
  uint8_t  stick_clk_r; // 1 = PRESSED, 0 = UP [PA11]
  uint32_t frame_id;    // Rolling sequence counter (0 to 4,294,967,295)
};
#pragma pack(pop)

// =============================================================================
//  CYBERPACKET (16-BYTE INDUSTRIAL TELEOPERATION SPECIFICATION v3.0)
// =============================================================================
#define CYBER_FRAME_HEADER_1  0x5A
#define CYBER_FRAME_HEADER_2  0xA5
#define CYBER_FRAME_TRAILER   0xED
#define CYBER_DEVICE_ID       0xC5  // Authentic CyberRover X5 Hardware Signature
#define CYBER_PACKET_SIZE     16    // 16-Byte Standard Struct

#define CYBER_BTN_ARM_BIT     0 // Bit 0: 1 = ARMED ('D'), 0 = PARK ('P')
#define CYBER_BTN_MUTE_BIT    1 // Bit 1: 1 = MUTE ON, 0 = SOUND ON
#define CYBER_BTN_PL_BIT      2 // Bit 2: 1 = Left Button PRESSED
#define CYBER_BTN_PR_BIT      3 // Bit 3: 1 = Right Button PRESSED
#define CYBER_BTN_CKL_BIT     4 // Bit 4: 1 = Left Click PRESSED (Turbo)
#define CYBER_BTN_CKR_BIT     5 // Bit 5: 1 = Right Click PRESSED
#define CYBER_PAGE_MASK       0xC0 // Bits 6-7: Display Page (0 to 3)
#define CYBER_PAGE_SHIFT      6

#define CYBER_PROFILE_MASK    0x0F // Bits 0-3: Profile (0=Arcade, 1=Tank, 2=1-Stick)
#define CYBER_RATE_MASK       0xF0 // Bits 4-7: Rate / 10 (2=20Hz, 4=40Hz, 5=50Hz)
#define CYBER_RATE_SHIFT      4

#pragma pack(push, 1)
/**
 * @brief High-Throughput 16-Byte Packed Binary Teleoperation Packet
 */
struct CyberPacket {
  uint8_t  device_id;    // [0]     0xC5: Device Signature & Anti-Hijack Filter
  uint8_t  seq_id;       // [1]     Rolling frame sequence counter (0 - 255)
  uint16_t timestamp_ms; // [2-3]   Transmitter hardware clock (0 - 65535 ms)
  int8_t   axis_lx;      // [4]     Left Stick X (-100% to +100%)
  int8_t   axis_ly;      // [5]     Left Stick Y (-100% to +100%) [Throttle]
  int8_t   axis_rx;      // [6]     Right Stick X (-100% to +100%) [Steering in Arcade]
  int8_t   axis_ry;      // [7]     Right Stick Y (-100% to +100%) [Track R in Tank]
  uint8_t  speed_gov;    // [8]     Speed Governor Factor (15 to 100%) [Pot Left]
  uint8_t  turn_ratio;   // [9]     Dynamic Turn Radius / Trim (0 to 75%) [Pot Center]
  uint8_t  pot_aux;      // [10]    Auxiliary Knob 3 / Page (0 to 100%) [Pot Right]
  uint8_t  buttons;      // [11]    bit0=ARM, bit1=MUTE, bit2=PL, bit3=PR, bit4=CKL, bit5=CKR, bit6-7=Page (0-3)
  uint8_t  mode_flags;   // [12]    bits 0-3 = Profile (0=Arcade, 1=Tank, 2=1-Stick), bits 4-7 = Rate / 10 (2, 4, 5)
  uint16_t frame_id;     // [13-14] Monotonic 16-bit Master Frame Counter (0 - 65535)
  uint8_t  crc8;         // [15]    CRC-8-ATM (Polynomial 0x07) over bytes [0..14]
};
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// Compile-time static assertions to strictly enforce packet layouts
static_assert(sizeof(RoverPacket) == CYBERROVER_PACKET_SIZE, 
              "CRITICAL DEFECT: RoverPacket memory layout must be exactly 24 bytes!");
static_assert(sizeof(CyberPacket) == CYBER_PACKET_SIZE, 
              "CRITICAL DEFECT: CyberPacket memory layout must be exactly 16 bytes!");

/**
 * @brief Strict RoverPacket Sanity Validator.
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

/**
 * @brief Helper Accessors for CyberPacket.
 */
inline bool getCyberArm(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_ARM_BIT)) != 0; }
inline bool getCyberMute(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_MUTE_BIT)) != 0; }
inline bool getCyberBtnPL(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PL_BIT)) != 0; }
inline bool getCyberBtnPR(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PR_BIT)) != 0; }
inline bool getCyberBtnCKL(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKL_BIT)) != 0; }
inline bool getCyberBtnCKR(const CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKR_BIT)) != 0; }
inline uint8_t getCyberPage(const CyberPacket& p) { return (p.buttons >> CYBER_PAGE_SHIFT) & 0x03; }
inline uint8_t getCyberProfile(const CyberPacket& p) { return (p.mode_flags & CYBER_PROFILE_MASK); }
inline uint8_t getCyberRateHz(const CyberPacket& p) { return ((p.mode_flags >> CYBER_RATE_SHIFT) & 0x0F) * 10; }

inline bool getCyberArm(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_ARM_BIT)) != 0; }
inline bool getCyberMute(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_MUTE_BIT)) != 0; }
inline bool getCyberBtnPL(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PL_BIT)) != 0; }
inline bool getCyberBtnPR(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_PR_BIT)) != 0; }
inline bool getCyberBtnCKL(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKL_BIT)) != 0; }
inline bool getCyberBtnCKR(const volatile CyberPacket& p) { return (p.buttons & (1 << CYBER_BTN_CKR_BIT)) != 0; }
inline uint8_t getCyberPage(const volatile CyberPacket& p) { return (p.buttons >> CYBER_PAGE_SHIFT) & 0x03; }
inline uint8_t getCyberProfile(const volatile CyberPacket& p) { return (p.mode_flags & CYBER_PROFILE_MASK); }
inline uint8_t getCyberRateHz(const volatile CyberPacket& p) { return ((p.mode_flags >> CYBER_RATE_SHIFT) & 0x0F) * 10; }

inline void setCyberButtons(CyberPacket& p, bool arm, bool mute, bool pl, bool pr, bool ckl, bool ckr, uint8_t page) {
  p.buttons = (arm ? 1 : 0) |
              ((mute ? 1 : 0) << 1) |
              ((pl ? 1 : 0) << 2) |
              ((pr ? 1 : 0) << 3) |
              ((ckl ? 1 : 0) << 4) |
              ((ckr ? 1 : 0) << 5) |
              ((page & 0x03) << 6);
}

inline void setCyberModeFlags(CyberPacket& p, uint8_t profile, uint8_t rate_hz) {
  uint8_t rate_code = rate_hz / 10;
  p.mode_flags = (profile & CYBER_PROFILE_MASK) | ((rate_code & 0x0F) << CYBER_RATE_SHIFT);
}

/**
 * @brief Industrial CRC-8-ATM / ITU Polynomial 0x07 (x^8 + x^2 + x + 1)
 */
inline uint8_t computeCyberCRC8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

inline uint8_t computeCyberChecksum(const CyberPacket& p) {
  return computeCyberCRC8(reinterpret_cast<const uint8_t*>(&p), sizeof(CyberPacket) - 1);
}

inline uint8_t computeCyberChecksum(const volatile CyberPacket& p) {
  return computeCyberCRC8(reinterpret_cast<const uint8_t*>((const void*)&p), sizeof(CyberPacket) - 1);
}

inline uint8_t computeCyberCrc8(const CyberPacket& p) {
  return computeCyberChecksum(p);
}

inline uint8_t computeCyberCrc8(const volatile CyberPacket& p) {
  return computeCyberChecksum(p);
}

inline bool isCyberPacketValid(const CyberPacket& p) {
  if (p.device_id != CYBER_DEVICE_ID) return false;
  if (p.axis_lx < -100 || p.axis_lx > 100) return false;
  if (p.axis_ly < -100 || p.axis_ly > 100) return false;
  if (p.axis_rx < -100 || p.axis_rx > 100) return false;
  if (p.axis_ry < -100 || p.axis_ry > 100) return false;
  if (p.speed_gov > 100) return false;
  if (p.turn_ratio > 100) return false;
  if (p.pot_aux > 100) return false;
  if (computeCyberChecksum(p) != p.crc8) return false;
  return true;
}

inline bool isCyberPacketValid(const volatile CyberPacket& p) {
  if (p.device_id != CYBER_DEVICE_ID) return false;
  if (p.axis_lx < -100 || p.axis_lx > 100) return false;
  if (p.axis_ly < -100 || p.axis_ly > 100) return false;
  if (p.axis_rx < -100 || p.axis_rx > 100) return false;
  if (p.axis_ry < -100 || p.axis_ry > 100) return false;
  if (p.speed_gov > 100) return false;
  if (p.turn_ratio > 100) return false;
  if (p.pot_aux > 100) return false;
  if (computeCyberChecksum(p) != p.crc8) return false;
  return true;
}
#endif

#endif // CYBERROVER_PROTOCOL_H
