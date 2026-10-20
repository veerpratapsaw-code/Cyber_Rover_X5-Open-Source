/*
 * ================================================================================
 * CYBERROVER X5 — AUDIO-HAPTIC ALERT ENGINE & ARBITRATION (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Features       : Priority-Based Non-Blocking FSM with Toggle Right Sound Mute
 * ================================================================================
 */

#ifndef ALERT_ENGINE_H
#define ALERT_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

enum AlertType {
  ALERT_NONE = 0,
  ALERT_POT_NOTCH,      // Priority 1 (Low)
  ALERT_BUTTON_CLICK,   // Priority 1 (Low)
  ALERT_TOGGLE_ON,      // Priority 2 (Medium)
  ALERT_TOGGLE_OFF,     // Priority 2 (Medium)
  ALERT_TURBO_KICK,     // Priority 2 (Medium)
  ALERT_LINK_HANDSHAKE, // Priority 3 (High)
  ALERT_LINK_LOST       // Priority 4 (Critical)
};

enum AlertPriority {
  PRIORITY_IDLE     = 0,
  PRIORITY_LOW      = 1,
  PRIORITY_MEDIUM   = 2,
  PRIORITY_HIGH     = 3,
  PRIORITY_CRITICAL = 4
};

class AlertEngine {
public:
  AlertEngine();
  void init(uint8_t buzzerPin, uint8_t hapticPin);

  /**
   * @brief Trigger an alert event with priority arbitration.
   * If an equal or higher priority alert is currently active, lower alerts are discarded.
   */
  void trigger(AlertType alert);

  /**
   * @brief Non-blocking state machine update. Call in main loop.
   */
  void update(uint32_t nowMs);

  /**
   * @brief Set Mute state (Toggle Right PB4).
   * When muted, the buzzer is silenced (0 sound), while haptic pulses remain active.
   */
  void setMuted(bool mute) { m_muted = mute; }
  bool isMuted() const     { return m_muted; }

  AlertType getCurrentAlert() const { return m_currentAlert; }

private:
  uint8_t       m_buzzerPin;
  uint8_t       m_hapticPin;
  bool          m_muted;

  AlertType     m_currentAlert;
  AlertPriority m_currentPriority;
  uint32_t      m_alertStartTime;
  uint8_t       m_alertPhase;

  void applyOutputs(bool buzzer, bool haptic);
  static AlertPriority getPriority(AlertType alert);
};

#endif // ALERT_ENGINE_H
