/*
 * ================================================================================
 * CYBERROVER X5 — ANALOG ACQUISITION & NOISE REJECTION ENGINE (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Subsystem      : 7-Channel Industrial ADC Conditioning Subsystem
 *
 * FILTER PIPELINE:
 *   1. Hardware 12-Bit ADC Sample (0 - 4095).
 *   2. 3-Sample Moving Window Median Filter (Rejects isolated impulsive EMI spikes).
 *   3. Configurable Single-Pole Low-Pass IIR Filter:
 *      y[n] = y[n-1] + alpha * (x_med - y[n-1])
 *      - Joysticks     : alpha = 0.70 (Latency < 6ms, aggressive response preserved)
 *      - Potentiometers: alpha = 0.25 (Maximum smoothing for steady setpoints)
 *   4. Zero-Step Deadband Normalization (Guarantees zero-point stability without jump)
 *   5. Potentiometer Boundary Hysteresis (Suppresses notch chatter)
 * ================================================================================
 */

#ifndef ANALOG_FILTER_H
#define ANALOG_FILTER_H

#include <stdint.h>
#include <stdbool.h>

class AnalogChannelFilter {
public:
  AnalogChannelFilter();

  /**
   * @brief Configure channel filter parameters.
   * @param alpha IIR filter weight (0.01 to 1.0). Higher = faster response.
   * @param invert Invert 12-bit range (4095 - x) for 90-degree rotated gimbals.
   * @param deadband Half-width deadband around center (e.g. 140 counts).
   * @param center Calibration center reading (nominal 2048).
   */
  void configure(float alpha, bool invert, uint16_t deadband = 0, uint16_t center = 2048);

  /**
   * @brief Feed a raw 12-bit ADC reading through the filter pipeline.
   * @param rawInput Raw reading from analogRead() (0 - 4095).
   * @return Conditioned, filtered, and bounded output (0 - 4095).
   */
  uint16_t update(uint16_t rawInput);

  /**
   * @brief Compute smooth normalized deflection (-1.0f to +1.0f) with zero-jump deadband.
   * @return Normalized floating-point deflection.
   */
  float getNormalizedDeflection() const;

  uint16_t getRaw() const      { return m_raw; }
  uint16_t getFiltered() const { return m_filtered; }
  uint32_t getOutlierCount() const { return m_outlierCount; }

private:
  float    m_alpha;
  bool     m_invert;
  uint16_t m_deadband;
  uint16_t m_center;

  uint16_t m_raw;
  uint16_t m_filtered;
  float    m_iirState;
  uint16_t m_medianWindow[3];
  uint8_t  m_medianIndex;
  bool     m_initialized;
  uint32_t m_outlierCount;

  static uint16_t computeMedian3(uint16_t a, uint16_t b, uint16_t c);
};

/**
 * @brief Potentiometer Hysteresis Notch Tracker.
 * Prevents continuous chattering/clicking when a knob sits on a boundary.
 */
class PotNotchTracker {
public:
  PotNotchTracker();
  void configure(uint8_t totalNotches, uint16_t hysteresisCounts);
  bool update(uint16_t currentReading, int& currentNotch);

private:
  uint8_t  m_totalNotches;
  uint16_t m_hysteresis;
  int      m_lastNotch;
  uint16_t m_lastAcceptedReading;
  bool     m_initialized;
};

#endif // ANALOG_FILTER_H
