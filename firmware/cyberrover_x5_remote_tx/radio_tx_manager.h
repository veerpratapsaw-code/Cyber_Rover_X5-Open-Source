/*
 * ================================================================================
 * CYBERROVER X5 — TRANSMITTER RADIO STATE MACHINE & DRIVER (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Features       : Non-Blocking Transmission, ACK Tracking, Bounded Error Recovery
 * ================================================================================
 */

#ifndef RADIO_TX_MANAGER_H
#define RADIO_TX_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "cyberrover_protocol.h"

enum RadioTxState {
  RADIO_TX_UNINIT,
  RADIO_TX_READY,
  RADIO_TX_LINK_ACTIVE,
  RADIO_TX_LINK_DEGRADED,
  RADIO_TX_LINK_LOST
};

class RadioTxManager {
public:
  RadioTxManager();

  bool init(uint8_t cePin, uint8_t csnPin, uint8_t channel, uint64_t pipeAddress);
  bool transmit(RoverPacket& packet, uint32_t nowMs);
  void updateLinkState(uint32_t nowMs);

  RadioTxState getState() const        { return m_state; }
  bool isLinkConnected() const         { return m_state == RADIO_TX_LINK_ACTIVE || m_state == RADIO_TX_LINK_DEGRADED; }
  bool justConnected()                 { bool r = m_justConnected; m_justConnected = false; return r; }
  bool justLost()                      { bool r = m_justLost; m_justLost = false; return r; }

  uint32_t getPacketsSent() const      { return m_packetsSent; }
  uint32_t getPacketsAcked() const     { return m_packetsAcked; }
  uint32_t getAckFailures() const      { return m_ackFailures; }
  bool getLastTxAck() const            { return m_lastTxAck; }

private:
  uint8_t      m_cePin;
  uint8_t      m_csnPin;
  uint8_t      m_channel;
  uint64_t     m_pipeAddress;

  RadioTxState m_state;
  bool         m_lastTxAck;
  uint32_t     m_lastAckSuccessTime;
  uint32_t     m_consecutiveAckDrops;

  uint32_t     m_packetsSent;
  uint32_t     m_packetsAcked;
  uint32_t     m_ackFailures;

  bool         m_justConnected;
  bool         m_justLost;
  bool         m_wasConnected;
};

#endif // RADIO_TX_MANAGER_H
