/*
 * ================================================================================
 * CYBERROVER X5 — RECEIVER SAFETY FAILSAFE ENGINE (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Features       : Deterministic 500ms Timeout, Safe Motor Clamping, Reconnection Guard
 * ================================================================================
 */

#ifndef RECEIVER_FAILSAFE_H
#define RECEIVER_FAILSAFE_H

#include <stdint.h>
#include <stdbool.h>
#include "cyberrover_protocol.h"

struct RoverMotorCommand {
  int16_t throttle_norm;   // -1000 (Full Reverse) to +1000 (Full Forward)
  int16_t steering_norm;   // -1000 (Full Left) to +1000 (Full Right)
  bool    is_armed;        // true = Motors Active ('D'), false = Parked ('P')
  bool    failsafe_active; // true = Signal Lost, Neutral Enforced
  uint32_t failsafe_count; // Number of times failsafe was tripped
};

class ReceiverFailsafe {
public:
  ReceiverFailsafe();

  void init(uint32_t timeoutMs = 500, uint8_t requiredPacketsToRecover = 3);
  void onValidPacket(const RoverPacket& pkt, uint32_t nowMs);
  void update(uint32_t nowMs);

  bool isFailsafeActive() const                { return m_failsafeActive; }
  bool justEnteredFailsafe()                   { bool r = m_justEnteredFailsafe; m_justEnteredFailsafe = false; return r; }
  bool justRecovered()                         { bool r = m_justRecovered; m_justRecovered = false; return r; }

  const RoverMotorCommand& getCommand() const  { return m_command; }
  uint32_t getPacketAgeMs(uint32_t nowMs) const{ return (nowMs >= m_lastPacketTime) ? (nowMs - m_lastPacketTime) : 0; }
  uint32_t getFailsafeCount() const            { return m_failsafeCount; }

private:
  uint32_t          m_timeoutMs;
  uint8_t           m_requiredRecoveryPackets;
  uint8_t           m_consecutiveRecoveryCount;

  uint32_t          m_lastPacketTime;
  bool              m_failsafeActive;
  bool              m_justEnteredFailsafe;
  bool              m_justRecovered;
  uint32_t          m_failsafeCount;

  RoverMotorCommand m_command;

  void computeMotorCommand(const RoverPacket& pkt);
  void clampToSafeNeutral();
};

#endif // RECEIVER_FAILSAFE_H
