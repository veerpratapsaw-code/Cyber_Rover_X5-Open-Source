/*
 * ================================================================================
 * CYBERROVER X5 — DIGITAL DEBOUNCING & INPUT STATE MACHINE (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#ifndef DIGITAL_INPUTS_H
#define DIGITAL_INPUTS_H

#include <stdint.h>
#include <stdbool.h>

struct DigitalConfig {
  uint8_t     pin;
  const char* tag;
  bool        activeLevel; // LOW or HIGH
  bool        isToggle;
  uint8_t     debounceMs;
};

class DigitalInputChannel {
public:
  DigitalInputChannel();
  void init(const DigitalConfig& config);
  void update(uint32_t nowMs, bool rawReading);

  bool isPressed() const    { return m_debouncedState; }
  bool justPressed() const  { return m_justPressed; }
  bool justReleased() const { return m_justReleased; }
  bool hasToggled() const   { return m_hasToggled; }

  uint8_t getPin() const    { return m_config.pin; }
  const char* getTag() const{ return m_config.tag; }
  bool getRaw() const       { return m_lastRaw; }

private:
  DigitalConfig m_config;
  bool          m_debouncedState;
  bool          m_lastRaw;
  uint32_t      m_lastTransitionTime;
  bool          m_justPressed;
  bool          m_justReleased;
  bool          m_hasToggled;
  bool          m_initialized;
};

#endif // DIGITAL_INPUTS_H
