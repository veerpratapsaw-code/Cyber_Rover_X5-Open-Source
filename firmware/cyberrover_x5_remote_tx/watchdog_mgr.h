/*
 * ================================================================================
 * CYBERROVER X5 — HARDWARE INDEPENDENT WATCHDOG (IWDG) SUBSYSTEM (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCU     : STM32F103C8T6 Hardware IWDG (Clocked from 40 kHz LSI)
 * ================================================================================
 */

#ifndef WATCHDOG_MGR_H
#define WATCHDOG_MGR_H

#include <stdint.h>
#include <stdbool.h>

class WatchdogManager {
public:
  WatchdogManager();
  bool init(uint32_t timeoutMs);
  void reload();
  bool wasResetByWatchdog() const { return m_wasResetByWatchdog; }
  bool isEnabled() const           { return m_enabled; }

private:
  bool m_enabled;
  bool m_wasResetByWatchdog;
};

#endif // WATCHDOG_MGR_H
