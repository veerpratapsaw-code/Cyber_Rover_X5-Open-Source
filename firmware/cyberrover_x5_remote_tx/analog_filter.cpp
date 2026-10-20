/*
 * ================================================================================
 * CYBERROVER X5 — ANALOG ACQUISITION & NOISE REJECTION ENGINE IMPLEMENTATION
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * ================================================================================
 */

#include "analog_filter.h"
#include <stdlib.h>

AnalogChannelFilter::AnalogChannelFilter()
  : m_alpha(0.70f),
    m_invert(false),
    m_deadband(0),
    m_center(2048),
    m_raw(2048),
    m_filtered(2048),
    m_iirState(2048.0f),
    m_medianIndex(0),
    m_initialized(false),
    m_outlierCount(0) {
  m_medianWindow[0] = 2048;
  m_medianWindow[1] = 2048;
  m_medianWindow[2] = 2048;
}

void AnalogChannelFilter::configure(float alpha, bool invert, uint16_t deadband, uint16_t center) {
  if (alpha < 0.01f) alpha = 0.01f;
  if (alpha > 1.0f)  alpha = 1.0f;
  m_alpha    = alpha;
  m_invert   = invert;
  m_deadband = deadband;
  m_center   = center;
}

uint16_t AnalogChannelFilter::computeMedian3(uint16_t a, uint16_t b, uint16_t c) {
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

uint16_t AnalogChannelFilter::update(uint16_t rawInput) {
  // 1. Clamping to valid 12-bit ADC range
  if (rawInput > 4095) {
    rawInput = 4095;
    m_outlierCount++;
  }
  m_raw = rawInput;

  // 2. Cold-start initialization
  if (!m_initialized) {
    m_medianWindow[0] = rawInput;
    m_medianWindow[1] = rawInput;
    m_medianWindow[2] = rawInput;
    m_iirState = (float)rawInput;
    m_initialized = true;
  }

  // 3. Outlier check vs previous state
  if (abs((int)rawInput - (int)m_filtered) > 1500) {
    m_outlierCount++;
  }

  // 4. Moving Median Filter (Window 3)
  m_medianWindow[m_medianIndex] = rawInput;
  m_medianIndex = (m_medianIndex + 1) % 3;
  uint16_t med = computeMedian3(m_medianWindow[0], m_medianWindow[1], m_medianWindow[2]);

  // 5. Low-Pass IIR Filter
  m_iirState = m_iirState + m_alpha * ((float)med - m_iirState);

  int conditioned = (int)(m_iirState + 0.5f);
  if (conditioned < 0)    conditioned = 0;
  if (conditioned > 4095) conditioned = 4095;

  // 6. Optional Axis Inversion
  if (m_invert) {
    conditioned = 4095 - conditioned;
  }

  m_filtered = (uint16_t)conditioned;
  return m_filtered;
}

float AnalogChannelFilter::getNormalizedDeflection() const {
  int center = (int)m_center;
  int dead   = (int)m_deadband;
  int val    = (int)m_filtered;

  int highThreshold = center + dead;
  int lowThreshold  = center - dead;
  const int joyMargin = 80; // Physical mechanical stop allowance

  if (val > highThreshold) {
    int maxTravel = (4095 - joyMargin) - highThreshold;
    if (maxTravel <= 0) return 1.0f;
    float norm = (float)(val - highThreshold) / (float)maxTravel;
    return (norm > 1.0f) ? 1.0f : norm;
  } else if (val < lowThreshold) {
    int maxTravel = lowThreshold - joyMargin;
    if (maxTravel <= 0) return -1.0f;
    float norm = (float)(lowThreshold - val) / (float)maxTravel;
    return (norm > 1.0f) ? -1.0f : -norm;
  } else {
    return 0.0f; // Inside deadband
  }
}

// =============================================================================
//  POTENTIOMETER HYSTERESIS NOTCH TRACKER
// =============================================================================
PotNotchTracker::PotNotchTracker()
  : m_totalNotches(20),
    m_hysteresis(65),
    m_lastNotch(-1),
    m_lastAcceptedReading(0),
    m_initialized(false) {
}

void PotNotchTracker::configure(uint8_t totalNotches, uint16_t hysteresisCounts) {
  m_totalNotches = totalNotches > 0 ? totalNotches : 20;
  m_hysteresis   = hysteresisCounts;
}

bool PotNotchTracker::update(uint16_t currentReading, int& currentNotch) {
  if (currentReading > 4095) currentReading = 4095;
  uint16_t notchWidth = 4096 / m_totalNotches; // ~204 counts
  int candidateNotch = currentReading / notchWidth;
  if (candidateNotch >= m_totalNotches) candidateNotch = m_totalNotches - 1;

  if (!m_initialized) {
    m_lastNotch = candidateNotch;
    m_lastAcceptedReading = currentReading;
    m_initialized = true;
    currentNotch = candidateNotch;
    return false;
  }

  currentNotch = m_lastNotch;

  // Hysteresis check: transition allowed only if notch differs AND delta exceeds hysteresis
  if (candidateNotch != m_lastNotch) {
    if (abs((int)currentReading - (int)m_lastAcceptedReading) > (int)m_hysteresis) {
      m_lastNotch = candidateNotch;
      m_lastAcceptedReading = currentReading;
      currentNotch = candidateNotch;
      return true; // Fired valid notch transition
    }
  }

  return false;
}
