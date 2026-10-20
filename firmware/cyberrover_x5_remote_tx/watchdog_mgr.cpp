/*
 * ================================================================================
 * CYBERROVER X5 — HARDWARE INDEPENDENT WATCHDOG (IWDG) IMPLEMENTATION
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#include "watchdog_mgr.h"
#include <Arduino.h>

#if defined(ARDUINO_ARCH_STM32)
#include <IWatchdog.h>
#endif

WatchdogManager::WatchdogManager()
  : m_enabled(false),
    m_wasResetByWatchdog(false) {
}

bool WatchdogManager::init(uint32_t timeoutMs) {
#if defined(ARDUINO_ARCH_STM32)
  // Check if previous reboot was caused by IWDG hardware reset
  m_wasResetByWatchdog = IWatchdog.isReset(true);

  // Initialize watchdog with timeout in microseconds
  uint32_t timeoutUs = timeoutMs * 1000UL;
  IWatchdog.begin(timeoutUs);
  m_enabled = IWatchdog.isEnabled();
  return m_enabled;
#else
  m_enabled = false;
  return false;
#endif
}

void WatchdogManager::reload() {
#if defined(ARDUINO_ARCH_STM32)
  if (m_enabled) {
    IWatchdog.reload();
  }
#endif
}
