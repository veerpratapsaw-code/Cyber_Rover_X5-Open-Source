/*
 * ================================================================================
 * CYBERROVER X5 — RECEIVER SAFETY FAILSAFE ENGINE IMPLEMENTATION
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#include "receiver_failsafe.h"

ReceiverFailsafe::ReceiverFailsafe()
  : m_timeoutMs(500),
    m_requiredRecoveryPackets(3),
    m_consecutiveRecoveryCount(0),
    m_lastPacketTime(0),
    m_failsafeActive(true),
    m_justEnteredFailsafe(false),
    m_justRecovered(false),
    m_failsafeCount(0) {
  clampToSafeNeutral();
}

void ReceiverFailsafe::init(uint32_t timeoutMs, uint8_t requiredPacketsToRecover) {
  m_timeoutMs                = timeoutMs > 0 ? timeoutMs : 500;
  m_requiredRecoveryPackets  = requiredPacketsToRecover > 0 ? requiredPacketsToRecover : 3;
  m_consecutiveRecoveryCount = 0;
  m_lastPacketTime           = 0;
  m_failsafeActive           = true;
  m_justEnteredFailsafe      = false;
  m_justRecovered            = false;
  m_failsafeCount            = 0;
  clampToSafeNeutral();
}

void ReceiverFailsafe::clampToSafeNeutral() {
  m_command.throttle_norm   = 0;
  m_command.steering_norm   = 0;
  m_command.is_armed        = false;
  m_command.failsafe_active = true;
}

void ReceiverFailsafe::computeMotorCommand(const RoverPacket& pkt) {
  // Speed Governor Scaling (15% to 100%)
  float governor = 0.15f + 0.85f * ((float)pkt.pot_l / 4095.0f);
  if (governor > 1.0f)  governor = 1.0f;
  if (governor < 0.15f) governor = 0.15f;

  // Throttle Normalization with Deadband (PB0)
  int ly = (int)pkt.stick_ly;
  int deadHigh = ADC_NOMINAL_CENTER + ADC_DEFAULT_DEADBAND;
  int deadLow  = ADC_NOMINAL_CENTER - ADC_DEFAULT_DEADBAND;
  const int joyMargin = 80;

  float rawThrottle = 0.0f;
  if (ly > deadHigh) {
    int maxFwd = (4095 - joyMargin) - deadHigh;
    if (maxFwd > 0) rawThrottle = (float)(ly - deadHigh) / (float)maxFwd;
  } else if (ly < deadLow) {
    int maxRev = deadLow - joyMargin;
    if (maxRev > 0) rawThrottle = -(float)(deadLow - ly) / (float)maxRev;
  }

  if (rawThrottle > 1.0f)  rawThrottle = 1.0f;
  if (rawThrottle < -1.0f) rawThrottle = -1.0f;

  // Steering Normalization (PB1)
  int lx = (int)pkt.stick_lx;
  float rawSteering = 0.0f;
  if (lx > deadHigh) {
    int maxRight = (4095 - joyMargin) - deadHigh;
    if (maxRight > 0) rawSteering = (float)(lx - deadHigh) / (float)maxRight;
  } else if (lx < deadLow) {
    int maxLeft = deadLow - joyMargin;
    if (maxLeft > 0) rawSteering = -(float)(deadLow - lx) / (float)maxLeft;
  }

  if (rawSteering > 1.0f)  rawSteering = 1.0f;
  if (rawSteering < -1.0f) rawSteering = -1.0f;

  // Scale with governor ceiling
  m_command.throttle_norm   = (int16_t)(rawThrottle * governor * 1000.0f);
  m_command.steering_norm   = (int16_t)(rawSteering * 1000.0f);
  m_command.is_armed        = (pkt.toggle_l == 1);
  m_command.failsafe_active = false;
}

void ReceiverFailsafe::onValidPacket(const RoverPacket& pkt, uint32_t nowMs) {
  m_lastPacketTime = nowMs;

  if (m_failsafeActive) {
    m_consecutiveRecoveryCount++;
    if (m_consecutiveRecoveryCount >= m_requiredRecoveryPackets) {
      m_failsafeActive = false;
      m_justRecovered  = true;
      m_consecutiveRecoveryCount = 0;
    }
  }

  if (!m_failsafeActive) {
    computeMotorCommand(pkt);
  } else {
    clampToSafeNeutral();
  }
}

void ReceiverFailsafe::update(uint32_t nowMs) {
  if (!m_failsafeActive) {
    if (nowMs - m_lastPacketTime > m_timeoutMs) {
      m_failsafeActive      = true;
      m_justEnteredFailsafe = true;
      m_failsafeCount++;
      m_consecutiveRecoveryCount = 0;
      clampToSafeNeutral();
    }
  }
}
