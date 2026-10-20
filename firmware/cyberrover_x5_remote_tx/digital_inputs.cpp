/*
 * ================================================================================
 * CYBERROVER X5 — DIGITAL DEBOUNCING & INPUT STATE MACHINE IMPLEMENTATION
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#include "digital_inputs.h"

DigitalInputChannel::DigitalInputChannel()
  : m_debouncedState(false),
    m_lastRaw(false),
    m_lastTransitionTime(0),
    m_justPressed(false),
    m_justReleased(false),
    m_hasToggled(false),
    m_initialized(false) {
  m_config.pin = 0;
  m_config.tag = "";
  m_config.activeLevel = false;
  m_config.isToggle = false;
  m_config.debounceMs = 20;
}

void DigitalInputChannel::init(const DigitalConfig& config) {
  m_config = config;
  m_debouncedState = false;
  m_lastRaw = false;
  m_lastTransitionTime = 0;
  m_justPressed = false;
  m_justReleased = false;
  m_hasToggled = false;
  m_initialized = false;
}

void DigitalInputChannel::update(uint32_t nowMs, bool rawPinReading) {
  // Clear one-shot pulse flags
  m_justPressed  = false;
  m_justReleased = false;
  m_hasToggled   = false;

  bool active = (rawPinReading == m_config.activeLevel);
  m_lastRaw = rawPinReading;

  // Startup settling initialization
  if (!m_initialized) {
    m_debouncedState = active;
    m_lastTransitionTime = nowMs;
    m_initialized = true;
    return;
  }

  // State change detected on raw pin
  if (active != m_debouncedState) {
    if (nowMs - m_lastTransitionTime >= m_config.debounceMs) {
      m_lastTransitionTime = nowMs;
      m_debouncedState = active;
      m_hasToggled = true;

      if (m_debouncedState) {
        m_justPressed = true;
      } else {
        m_justReleased = true;
      }
    }
  } else {
    // Reading matches current debounced state -> reset debounce counter
    m_lastTransitionTime = nowMs;
  }
}
