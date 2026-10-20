/*
 * ================================================================================
 * CYBERROVER X5 — TRANSMITTER HARDWARE PINOUT & CONFIGURATION (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCU     : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Handheld Master Transmitter Unit
 * ================================================================================
 */

#ifndef CYBERROVER_CONFIG_H
#define CYBERROVER_CONFIG_H

#include <Arduino.h>

// =============================================================================
//  ANALOG INPUT PIN ASSIGNMENTS (7 Channels)
// =============================================================================
#define PIN_STICK_LX      PB1   // Left Stick X   (ADC9) [Steering / Turn Blinkers]
#define PIN_STICK_LY      PB0   // Left Stick Y   (ADC8) [Throttle / Speed & RPM]
#define PIN_STICK_RX      PA4   // Right Stick X  (ADC4) [Inverted 90° Gimbal mount]
#define PIN_STICK_RY      PA3   // Right Stick Y  (ADC3) [Camera Tilt]
#define PIN_POT_LEFT      PA0   // Potentiometer L(ADC0) [Speed Governor Factor]
#define PIN_POT_CENTER    PA1   // Potentiometer C(ADC1) [Camera Tilt Trim]
#define PIN_POT_RIGHT     PA2   // Potentiometer R(ADC2) [Auxiliary / Battery Sim]

// =============================================================================
//  DIGITAL INPUT PIN ASSIGNMENTS (6 Channels)
// =============================================================================
#define PIN_TOGGLE_LEFT   PB3   // Toggle Left    (Active-LOW  : LOW = ON/ARM, HIGH = OFF/PARK)
#define PIN_TOGGLE_RIGHT  PB4   // Toggle Right   (Active-LOW  : LOW = MUTE ON, HIGH = SOUND ACTIVE)
#define PIN_PUSH_LEFT     PA8   // Push Button L  (Active-HIGH : HIGH = PRESSED, LOW = RELEASED)
#define PIN_PUSH_RIGHT    PA15  // Push Button R  (Active-HIGH : HIGH = PRESSED, LOW = RELEASED)
#define PIN_STICK_CLK_L   PA10  // Gimbal Click L (Active-LOW  : LOW = PRESSED, HIGH = UP)
#define PIN_STICK_CLK_R   PA11  // Gimbal Click R (Active-LOW  : LOW = PRESSED, HIGH = UP)

// =============================================================================
//  FEEDBACK ACTUATORS & INDICATORS
// =============================================================================
#define PIN_BUZZER        PB8   // Piezo Buzzer driver
#define PIN_HAPTIC        PB9   // Haptic Vibration Motor (via MOSFET)
#define PIN_LED_HB        PC13  // Onboard Heartbeat LED (Active-LOW)

// =============================================================================
//  HARDWARE SPI1 (NRF24L01+ Transceiver)
// =============================================================================
#define PIN_NRF_SCK       PA5   // Hardware SPI1 SCK
#define PIN_NRF_MISO      PA6   // Hardware SPI1 MISO
#define PIN_NRF_MOSI      PA7   // Hardware SPI1 MOSI
#define PIN_NRF_CE        PB10  // Radio Chip Enable
#define PIN_NRF_CSN       PB11  // Radio Chip Select Not

// =============================================================================
//  HARDWARE I2C1 (1.3" OLED Pilot HUD)
// =============================================================================
#define OLED_SCL_PIN      PB6   // Hardware I2C1 SCL
#define OLED_SDA_PIN      PB7   // Hardware I2C1 SDA
#define USE_SH1106        1     // 1 for 1.3" SH1106 OLED, 0 for 0.96" SSD1306

// =============================================================================
//  TASK SCHEDULING INTERVALS (Deterministic Multi-Rate Loop)
// =============================================================================
#define TX_INTERVAL_MS          25   // 40 Hz RF transmission (25ms cycle)
#define OLED_INTERVAL_MS        40   // 25 FPS HUD refresh (40ms cycle)
#define TELEMETRY_INTERVAL_MS  100   // 10 Hz Serial telemetry (100ms cycle)
#define HEARTBEAT_INTERVAL_MS  500   // 1 Hz Heartbeat LED toggle (500ms cycle)
#define IWDG_TIMEOUT_MS       1500   // Independent Hardware Watchdog Timeout (1.5s)

#endif // CYBERROVER_CONFIG_H
