/*
 * ================================================================================
 * CYBERROVER X5 — AUDIO-HAPTIC ALERT ENGINE & ARBITRATION IMPLEMENTATION
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#include "alert_engine.h"
#include <Arduino.h>

AlertEngine::AlertEngine()
  : m_buzzerPin(0),
    m_hapticPin(0),
    m_muted(false),
    m_currentAlert(ALERT_NONE),
    m_currentPriority(PRIORITY_IDLE),
    m_alertStartTime(0),
    m_alertPhase(0) {
}

void AlertEngine::init(uint8_t buzzerPin, uint8_t hapticPin) {
  m_buzzerPin = buzzerPin;
  m_hapticPin = hapticPin;
  pinMode(m_buzzerPin, OUTPUT);
  pinMode(m_hapticPin, OUTPUT);
  applyOutputs(false, false);
}

AlertPriority AlertEngine::getPriority(AlertType alert) {
  switch (alert) {
    case ALERT_LINK_LOST:       return PRIORITY_CRITICAL;
    case ALERT_LINK_HANDSHAKE:  return PRIORITY_HIGH;
    case ALERT_TOGGLE_ON:
    case ALERT_TOGGLE_OFF:
    case ALERT_TURBO_KICK:      return PRIORITY_MEDIUM;
    case ALERT_POT_NOTCH:
    case ALERT_BUTTON_CLICK:    return PRIORITY_LOW;
    default:                    return PRIORITY_IDLE;
  }
}

void AlertEngine::applyOutputs(bool buzzer, bool haptic) {
  // If muted, buzzer is strictly held LOW (zero sound)
  bool effectiveBuzzer = (buzzer && !m_muted);
  digitalWrite(m_buzzerPin, effectiveBuzzer ? HIGH : LOW);
  digitalWrite(m_hapticPin, haptic ? HIGH : LOW);
}

void AlertEngine::trigger(AlertType alert) {
  if (alert == ALERT_NONE) return;

  AlertPriority newPriority = getPriority(alert);

  // Priority Arbitration: Higher priority can pre-empt lower priority.
  // Equal or lower priority is dropped if a pattern is already executing.
  if (m_currentAlert != ALERT_NONE && newPriority <= m_currentPriority) {
    return;
  }

  m_currentAlert    = alert;
  m_currentPriority = newPriority;
  m_alertStartTime  = millis();
  m_alertPhase      = 0;

  // Initialize first phase outputs
  switch (alert) {
    case ALERT_LINK_LOST:
    case ALERT_LINK_HANDSHAKE:
    case ALERT_TOGGLE_ON:
    case ALERT_TOGGLE_OFF:
    case ALERT_BUTTON_CLICK:
    case ALERT_POT_NOTCH:
      applyOutputs(true, true);
      break;
    case ALERT_TURBO_KICK:
      applyOutputs(false, true); // Haptic only for turbo kick
      break;
    default:
      applyOutputs(false, false);
      break;
  }
}

void AlertEngine::update(uint32_t nowMs) {
  if (m_currentAlert == ALERT_NONE) return;

  uint32_t elapsed = nowMs - m_alertStartTime;

  switch (m_currentAlert) {
    case ALERT_LINK_LOST:
      // Pattern: Long Pulse (150ms) -> Pause (100ms) -> Long Pulse (200ms)
      if (m_alertPhase == 0 && elapsed >= 150) {
        applyOutputs(false, false);
        m_alertPhase = 1;
      } else if (m_alertPhase == 1 && elapsed >= 250) {
        applyOutputs(true, true);
        m_alertPhase = 2;
      } else if (m_alertPhase == 2 && elapsed >= 450) {
        applyOutputs(false, false);
        m_currentAlert    = ALERT_NONE;
        m_currentPriority = PRIORITY_IDLE;
      }
      break;

    case ALERT_LINK_HANDSHAKE:
      // Pattern: Double Pip: 60ms ON -> 35ms OFF -> 70ms ON -> OFF
      if (m_alertPhase == 0 && elapsed >= 60) {
        applyOutputs(false, false);
        m_alertPhase = 1;
      } else if (m_alertPhase == 1 && elapsed >= 95) {
        applyOutputs(true, true);
        m_alertPhase = 2;
      } else if (m_alertPhase == 2 && elapsed >= 165) {
        applyOutputs(false, false);
        m_currentAlert    = ALERT_NONE;
        m_currentPriority = PRIORITY_IDLE;
      }
      break;

    case ALERT_TOGGLE_ON:
      // Buzzer 30ms, Haptic 75ms
      if (m_alertPhase == 0 && elapsed >= 30) {
        applyOutputs(false, true);
        m_alertPhase = 1;
      } else if (m_alertPhase == 1 && elapsed >= 75) {
        applyOutputs(false, false);
        m_currentAlert    = ALERT_NONE;
        m_currentPriority = PRIORITY_IDLE;
      }
      break;

    case ALERT_TOGGLE_OFF:
      // Buzzer 20ms, Haptic 50ms
      if (m_alertPhase == 0 && elapsed >= 20) {
        applyOutputs(false, true);
        m_alertPhase = 1;
      } else if (m_alertPhase == 1 && elapsed >= 50) {
        applyOutputs(false, false);
        m_currentAlert    = ALERT_NONE;
        m_currentPriority = PRIORITY_IDLE;
      }
      break;

    case ALERT_BUTTON_CLICK:
      // Buzzer 25ms, Haptic 60ms
      if (m_alertPhase == 0 && elapsed >= 25) {
        applyOutputs(false, true);
        m_alertPhase = 1;
      } else if (m_alertPhase == 1 && elapsed >= 60) {
        applyOutputs(false, false);
        m_currentAlert    = ALERT_NONE;
        m_currentPriority = PRIORITY_IDLE;
      }
      break;

    case ALERT_POT_NOTCH:
      // Buzzer 12ms, Haptic 25ms (Subtle tactile notch)
      if (m_alertPhase == 0 && elapsed >= 12) {
        applyOutputs(false, true);
        m_alertPhase = 1;
      } else if (m_alertPhase == 1 && elapsed >= 25) {
        applyOutputs(false, false);
        m_currentAlert    = ALERT_NONE;
        m_currentPriority = PRIORITY_IDLE;
      }
      break;

    case ALERT_TURBO_KICK:
      // Strong Haptic Pulse (90ms)
      if (elapsed >= 90) {
        applyOutputs(false, false);
        m_currentAlert    = ALERT_NONE;
        m_currentPriority = PRIORITY_IDLE;
      }
      break;

    default:
      applyOutputs(false, false);
      m_currentAlert    = ALERT_NONE;
      m_currentPriority = PRIORITY_IDLE;
      break;
  }
}
