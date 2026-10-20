/*
 * ================================================================================
 * CYBERROVER X5 — RECEIVER RADIO STATE MACHINE & DRIVER IMPLEMENTATION
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#include "radio_rx_manager.h"
#include <SPI.h>
#include <RF24.h>

static RF24 s_rxRadio(PA3, PA4); // Re-configured in init()

RadioRxManager::RadioRxManager()
  : m_cePin(PA3),
    m_csnPin(PA4),
    m_channel(CYBERROVER_RF_CHANNEL),
    m_pipeAddress(CYBERROVER_RF_PIPE),
    m_state(RADIO_RX_UNINIT),
    m_packetsReceived(0),
    m_corruptPackets(0),
    m_rateCounter(0),
    m_lastRateCheck(0),
    m_displayRateHz(0),
    m_lastRecoveryProbe(0),
    m_justRecovered(false) {
}

bool RadioRxManager::init(uint8_t cePin, uint8_t csnPin, uint8_t channel, uint64_t pipeAddress) {
  m_cePin       = cePin;
  m_csnPin      = csnPin;
  m_channel     = channel;
  m_pipeAddress = pipeAddress;

  SPI.begin();

  if (!s_rxRadio.begin()) {
    m_state = RADIO_RX_ERROR;
    return false;
  }

  s_rxRadio.setPALevel(RF24_PA_LOW);           // Bench power (prevents brownouts)
  s_rxRadio.setDataRate(RF24_250KBPS);         // 250 kbps for maximum sensitivity and range
  s_rxRadio.setChannel(m_channel);             // Channel 108 (2.508 GHz)
  s_rxRadio.setPayloadSize(sizeof(RoverPacket));
  s_rxRadio.openReadingPipe(1, m_pipeAddress); // Listen on primary pipe
  s_rxRadio.startListening();

  m_state = RADIO_RX_LISTENING;
  return true;
}

bool RadioRxManager::processIncoming(RoverPacket& outPacket, uint32_t nowMs) {
  // Update packet rate calculator every 1000ms
  if (nowMs - m_lastRateCheck >= 1000) {
    m_lastRateCheck = nowMs;
    m_displayRateHz = m_rateCounter;
    m_rateCounter   = 0;
  }

  if (m_state == RADIO_RX_UNINIT || m_state == RADIO_RX_ERROR) {
    return false;
  }

  bool packetFound = false;

  // Non-blocking packet retrieval with zero register interrogation
  while (s_rxRadio.available()) {
    RoverPacket temp;
    s_rxRadio.read(&temp, sizeof(RoverPacket));

    // Strict 12-bit ADC and digital validation
    if (isRoverPacketValid(temp)) {
      outPacket = temp;
      m_packetsReceived++;
      m_rateCounter++;
      packetFound = true;
      m_state = RADIO_RX_RECEIVING;
    } else {
      // Discard floating SPI / bus noise
      m_corruptPackets++;
      s_rxRadio.flush_rx();
    }
  }

  return packetFound;
}

void RadioRxManager::checkBackgroundRecovery(uint32_t nowMs, uint32_t lastPacketTimeMs) {
  // Only probe hardware if link has been lost for over 2000 ms
  if ((nowMs - lastPacketTimeMs > 2000) && (nowMs - m_lastRecoveryProbe >= 2000)) {
    m_lastRecoveryProbe = nowMs;

    if (!s_rxRadio.isChipConnected()) {
      m_state = RADIO_RX_ERROR;
      if (init(m_cePin, m_csnPin, m_channel, m_pipeAddress)) {
        m_justRecovered = true;
      }
    }
  }
}
