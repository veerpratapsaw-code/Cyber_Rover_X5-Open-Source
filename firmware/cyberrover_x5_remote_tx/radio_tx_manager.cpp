/*
 * ================================================================================
 * CYBERROVER X5 — TRANSMITTER RADIO STATE MACHINE & DRIVER IMPLEMENTATION
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#include "radio_tx_manager.h"
#include <SPI.h>
#include <RF24.h>

static RF24 s_radio(PB10, PB11); // Initialized with default pins, re-configured in init()

RadioTxManager::RadioTxManager()
  : m_cePin(PB10),
    m_csnPin(PB11),
    m_channel(CYBERROVER_RF_CHANNEL),
    m_pipeAddress(CYBERROVER_RF_PIPE),
    m_state(RADIO_TX_UNINIT),
    m_lastTxAck(false),
    m_lastAckSuccessTime(0),
    m_consecutiveAckDrops(0),
    m_packetsSent(0),
    m_packetsAcked(0),
    m_ackFailures(0),
    m_justConnected(false),
    m_justLost(false),
    m_wasConnected(false) {
}

bool RadioTxManager::init(uint8_t cePin, uint8_t csnPin, uint8_t channel, uint64_t pipeAddress) {
  m_cePin       = cePin;
  m_csnPin      = csnPin;
  m_channel     = channel;
  m_pipeAddress = pipeAddress;

  SPI.begin();

  if (!s_radio.begin()) {
    m_state = RADIO_TX_UNINIT;
    return false;
  }

  s_radio.setPALevel(RF24_PA_LOW);           // Bench power (prevents brownout on battery/USB)
  s_radio.setDataRate(RF24_250KBPS);         // 250 kbps for maximum sensitivity & penetration
  s_radio.setChannel(m_channel);             // Channel 108 (2.508 GHz)
  s_radio.setRetries(5, 15);                 // 1500us delay, 15 retries (maximum RF reliability)
  s_radio.setPayloadSize(sizeof(RoverPacket));
  s_radio.openWritingPipe(m_pipeAddress);
  s_radio.stopListening();                   // Transmitter mode

  m_state = RADIO_TX_READY;
  return true;
}

bool RadioTxManager::transmit(RoverPacket& packet, uint32_t nowMs) {
  if (m_state == RADIO_TX_UNINIT) {
    return false;
  }

  m_packetsSent++;

  // Non-blocking transmission with auto-acknowledgement
  m_lastTxAck = s_radio.write(&packet, sizeof(RoverPacket));

  if (m_lastTxAck) {
    m_packetsAcked++;
    m_consecutiveAckDrops = 0;
    m_lastAckSuccessTime = nowMs;

    if (!m_wasConnected) {
      m_wasConnected = true;
      m_justConnected = true;
    }
    m_state = RADIO_TX_LINK_ACTIVE;
  } else {
    m_ackFailures++;
    m_consecutiveAckDrops++;

    if (m_consecutiveAckDrops >= 4 && m_consecutiveAckDrops < 40) {
      m_state = RADIO_TX_LINK_DEGRADED;
    } else if (m_consecutiveAckDrops >= 40) { // ~1000ms at 40 Hz
      if (m_wasConnected) {
        m_wasConnected = false;
        m_justLost = true;
      }
      m_state = RADIO_TX_LINK_LOST;
    }
  }

  return m_lastTxAck;
}

void RadioTxManager::updateLinkState(uint32_t nowMs) {
  if (m_state == RADIO_TX_UNINIT) return;

  // Timeout check: if no ACK for > 1000ms, declare LINK_LOST
  if (m_wasConnected && (nowMs - m_lastAckSuccessTime > 1000)) {
    m_wasConnected = false;
    m_justLost = true;
    m_state = RADIO_TX_LINK_LOST;
  }
}
