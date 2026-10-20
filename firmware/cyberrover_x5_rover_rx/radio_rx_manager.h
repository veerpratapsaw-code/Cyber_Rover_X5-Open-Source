/*
 * ================================================================================
 * CYBERROVER X5 — RECEIVER RADIO STATE MACHINE & DRIVER (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Features       : Zero SPI Contention, Strict 12-Bit Packet Sanity, Auto-Recovery
 * ================================================================================
 */

#ifndef RADIO_RX_MANAGER_H
#define RADIO_RX_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "cyberrover_protocol.h"

enum RadioRxState {
  RADIO_RX_UNINIT,
  RADIO_RX_LISTENING,
  RADIO_RX_RECEIVING,
  RADIO_RX_ERROR
};

class RadioRxManager {
public:
  RadioRxManager();

  bool init(uint8_t cePin, uint8_t csnPin, uint8_t channel, uint64_t pipeAddress);
  bool processIncoming(RoverPacket& outPacket, uint32_t nowMs);
  void checkBackgroundRecovery(uint32_t nowMs, uint32_t lastPacketTimeMs);

  RadioRxState getState() const              { return m_state; }
  uint32_t getPacketsReceived() const        { return m_packetsReceived; }
  uint32_t getCorruptRejected() const        { return m_corruptPackets; }
  uint32_t getPacketRateHz() const           { return m_displayRateHz; }
  bool justRecovered()                       { bool r = m_justRecovered; m_justRecovered = false; return r; }

private:
  uint8_t      m_cePin;
  uint8_t      m_csnPin;
  uint8_t      m_channel;
  uint64_t     m_pipeAddress;

  RadioRxState m_state;
  uint32_t     m_packetsReceived;
  uint32_t     m_corruptPackets;
  uint32_t     m_rateCounter;
  uint32_t     m_lastRateCheck;
  uint32_t     m_displayRateHz;

  uint32_t     m_lastRecoveryProbe;
  bool         m_justRecovered;
};

#endif // RADIO_RX_MANAGER_H
