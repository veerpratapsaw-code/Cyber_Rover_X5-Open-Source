# 🎮 REMOTE TRANSMITTER PINOUT & WIRING MATRIX (v2.0)

> **Platform:** Handheld Custom Transmitter  
> **Primary Microcontroller:** STM32F103C8T6 (Blue Pill, 72 MHz ARM Cortex-M3)  
> **Wireless Gateway Module:** ESP32 DevKit (2.4 GHz ESP-NOW Unicast)  
> **Power Rail:** 1S 18650 Li-ion -> TP4056 USB-C -> Master Rocker Switch -> MT3608 5V Boost -> STM32 5V Pin

---

## 1. STM32 Blue Pill Pin Assignments

| STM32 Pin | Peripheral Function | Hardware Connection | Notes / Electrical Mode |
|:---|:---|:---|:---|
| **PA0** | Right Gimbal Y-Axis | Gimbal `VRy` (Rotated 90°) | Analog `0–4095` (12-bit ADC) |
| **PA1** | Right Gimbal X-Axis | Gimbal `VRx` (Rotated 90°) | Inverted in software (`4095 - raw`) |
| **PA2** | Knob 1 (Left Pot) | 10k Rotary Potentiometer | Speed Governor (15% to 100%) |
| **PA3** | Knob 2 (Center Pot) | 10k Rotary Potentiometer | Dynamic Turn Ratio (0% to 75%) |
| **PA4** | Knob 3 (Right Pot) | 10k Rotary Potentiometer | Aux Control / Menu Page Selector |
| **PA9** | UART1 TX | ESP32 RX0 (GPIO 3) | 115200 Baud 19-byte CyberFrame |
| **PA10** | UART1 RX | ESP32 TX0 (GPIO 1) | Return `$RTL` Telemetry Stream |
| **PA11** | Left Gimbal Click (`SW`) | Momentary Tactile Switch | Active-LOW (`INPUT_PULLUP`) |
| **PA15** | Pushbutton Left | Normally Closed (NC) Tactile | Calibrated Active-HIGH in firmware |
| **PB0** | Left Gimbal Y-Axis | Gimbal `VRy` (Rotated 90°) | Analog `0–4095` (12-bit ADC) |
| **PB1** | Left Gimbal X-Axis | Gimbal `VRx` (Rotated 90°) | Analog `0–4095` (12-bit ADC) |
| **PB3** | Pushbutton Right | Normally Closed (NC) Tactile | Calibrated Active-HIGH (Hardware JTAG freed) |
| **PB4** | Toggle Switch Left | SPST Toggle Switch to GND | Master ARM / PARK (`INPUT_PULLUP`) |
| **PB5** | Toggle Switch Right | SPST Toggle Switch to GND | Drive Mode / Mute (`INPUT_PULLUP`) |
| **PB6** | I2C1 SCL | 1.3" I2C OLED (SH1106) SCL | 400 kHz Fast I2C Bus |
| **PB7** | I2C1 SDA | 1.3" I2C OLED (SH1106) SDA | 400 kHz Fast I2C Bus |
| **PB8** | Haptic Vibration Motor | 2N2222 Transistor Base | ERM Vibration Alert Output |
| **PB9** | Piezo Buzzer | Active 5V Buzzer Signal | Audio Chirp / Alarm Alert Output |

---

## 2. ESP32 Gateway Module Interconnect

| ESP32 Pin | Signal | Connected To | Purpose |
|:---|:---|:---|:---|
| **RX0 (GPIO 3)** | UART RX | STM32 PA9 (TX1) | Receives CyberFrame packets |
| **TX0 (GPIO 1)** | UART TX | STM32 PA10 (RX1) | Forwards `$RTL` telemetry packets |
| **GND** | Star Ground | Common Ground Bus | Star ground reference |
| **VIN / 5V** | 5V Power | 5V Boost Rail | High-efficiency regulated 5V |
| **GPIO 2** | Status LED | Onboard Blue LED | Blinking = Searching; Solid = Paired |

---

## 3. Power Subsystem Topology

```
[1S 18650 Li-ion] 
       │
       ▼
 [TP4056 USB-C Charger & Battery Protection Module]
       │
       ▼
 [Master SPST Rocker Switch]
       │
       ▼
 [MT3608 Mini Boost Converter] (Adjusted to precisely 5.05V)
       │
       ├──► STM32 5V Pin (Feeds onboard 3.3V LDO with zero brownout)
       ├──► ESP32 VIN Pin
       └──► 5V Active Piezo Buzzer
```
