/*
 * ================================================================================
 * CYBERROVER X5 — RECEIVER HARDWARE PINOUT & CONFIGURATION (v1.5)
 * ================================================================================
 * Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Target MCU     : STM32F103C8T6 (Blue Pill) @ 72 MHz ARM Cortex-M3
 * Subsystem      : Carrier Node Receiver & Vehicle Failsafe Supervisor
 * ================================================================================
 */

#ifndef CYBERROVER_RX_CONFIG_H
#define CYBERROVER_RX_CONFIG_H

#include <Arduino.h>

// =============================================================================
//  HARDWARE SPI1 (NRF24L01+ Receiver Node)
// =============================================================================
#define PIN_NRF_CE      PA3   // Radio Chip Enable
#define PIN_NRF_CSN     PA4   // Radio Chip Select Not
#define PIN_NRF_SCK     PA5   // Hardware SPI1 SCK
#define PIN_NRF_MISO    PA6   // Hardware SPI1 MISO
#define PIN_NRF_MOSI    PA7   // Hardware SPI1 MOSI

// Indicators & Telemetry
#define PIN_LED_HB      PC13  // Onboard Heartbeat / Packet Activity LED (Active-LOW)

// Safety Failsafe Timers
#define FAILSAFE_TIMEOUT_MS           500  // 500ms packet loss threshold
#define RECONNECT_REQUIRED_PACKETS     3   // 3 consecutive valid packets to clear failsafe
#define TELEMETRY_INTERVAL_MS         100  // 10 Hz Serial Telemetry Stream

#endif // CYBERROVER_RX_CONFIG_H
