# 🔌 CYBERROVER X5 — WIRING & HARDWARE COMPATIBILITY CHECKLIST (FIRMWARE v1.5)

**Document Classification:** Hardware Pinout & Physical Assembly Specification  
**Lead Architect & Developer:** Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh, Jharkhand  

---

## 1. Transmitter Pinout & Electrical Polarity (Handheld Controller)

```
                        STM32F103C8T6 (BLUE PILL - REMOTE TX)
                                  +-------------------+
                   Left Stick X --| PB1          PB0  |-- Left Stick Y
                 Right Stick X  --| PA4          PA3  |-- Right Stick Y
              Speed Governor Pot--| PA0          PA2  |-- Aux / Battery Pot
              Camera Tilt Trim  --| PA1          PA15 |-- Push Right (NO, Active-HIGH)
          Master Arm (Active-LOW)-| PB3          PA8  |-- Push Left  (NO, Active-HIGH)
         SOUND MUTE (Active-LOW)--| PB4          PA12 |-- Left Stick Click (Active-LOW)
                OLED I2C1 SCL   --| PB6          PA11 |-- Right Stick Click(Active-LOW)
                OLED I2C1 SDA   --| PB7          PA5  |-- NRF24 SPI1 SCK
                 Piezo Buzzer   --| PB8          PA6  |-- NRF24 SPI1 MISO
                 Haptic Motor   --| PB9          PA7  |-- NRF24 SPI1 MOSI
                 NRF24 CE       --| PB10         PC13 |-- Onboard Heartbeat LED
                 NRF24 CSN      --| PB11         3.3V |-- NRF24 VCC (with 10uF cap)
                 Main Boost 5V  --| 5V           GND  |-- Common System Ground
                                  +-------------------+
```

### Complete Transmitter Table

| Pin | Channel Name | Type | Electrical Polarity | Physical Function |
| :--- | :--- | :--- | :--- | :--- |
| **PB1** | Left Stick X | Analog ADC9 | $0\text{V} - 3.3\text{V}$ | Vehicle Steering & Turn Blinkers |
| **PB0** | Left Stick Y | Analog ADC8 | $0\text{V} - 3.3\text{V}$ | Vehicle Throttle / Speed & RPM |
| **PA4** | Right Stick X | Analog ADC4 | Inverted ($4095 - x$) | Right Stick Horizontal Axis |
| **PA3** | Right Stick Y | Analog ADC3 | $0\text{V} - 3.3\text{V}$ | FPV Camera Tilt Elevation |
| **PA0** | Pot Left | Analog ADC0 | $0\text{V} - 3.3\text{V}$ | Speed Governor Limit ($15\% - 100\%$) |
| **PA1** | Pot Center | Analog ADC1 | $0\text{V} - 3.3\text{V}$ | Camera Tilt Center Trim |
| **PA2** | Pot Right | Analog ADC2 | $0\text{V} - 3.3\text{V}$ | Auxiliary / Simulated Battery Dial |
| **PB3** | Toggle Left | Digital | Active-LOW (`INPUT_PULLUP`) | Master Arm Switch ($0 = \text{Park}, 1 = \text{Drive}$) |
| **PB4** | Toggle Right | Digital | Active-LOW (`INPUT_PULLUP`) | **SOUND MUTE SWITCH** ($1 = \text{Muted}, 0 = \text{Sound On}$) |
| **PA8** | Push Left | Digital | Active-HIGH (`INPUT`) | Auxiliary Push Button Left |
| **PA15**| Push Right | Digital | Active-HIGH (`INPUT`) | Auxiliary Push Button Right |
| **PA10**| Stick Click L| Digital | Active-LOW (`INPUT_PULLUP`) | Left Gimbal Push Switch |
| **PA11**| Stick Click R| Digital | Active-LOW (`INPUT_PULLUP`) | Right Gimbal Push Switch |
| **PB9** | Buzzer | Output | Active-HIGH | Piezo Buzzer (Silenced when PB4 Mute active) |
| **PB8** | Haptic Motor | Output | Active-HIGH | Vibration Motor via MOSFET |
| **PC13**| Heartbeat LED| Output | Active-LOW | Onboard Pilot Status LED |
| **PB6** | OLED SCL | I2C1 | $400\text{ kHz}$ Fast Mode | Display Clock |
| **PB7** | OLED SDA | I2C1 | Hardware I2C | Display Data |
| **PB10**| NRF24 CE | Output | Active-HIGH | Radio Transmit Trigger |
| **PB11**| NRF24 CSN | Output | Active-LOW | Radio SPI Bus Select |
| **PA5** | NRF24 SCK | SPI1 | Hardware Clock | Radio SPI Clock |
| **PA6** | NRF24 MISO | SPI1 | Hardware Input | Radio Master-In-Slave-Out |
| **PA7** | NRF24 MOSI | SPI1 | Hardware Output | Radio Master-Out-Slave-In |

---

## 2. Receiver Pinout (Rover Carrier Node)

```
                        STM32F103C8T6 (BLUE PILL - ROVER RX)
                                  +-------------------+
                   NRF24 CE     --| PA3          PA5  |-- NRF24 SPI1 SCK
                   NRF24 CSN    --| PA4          PA6  |-- NRF24 SPI1 MISO
                   Hardware 5V  --| 5V           PA7  |-- NRF24 SPI1 MOSI
                   System GND   --| GND          PC13 |-- Heartbeat / Link LED
                   USART1 TX    --| PA9          PA10 |-- USART1 RX
                                  +-------------------+
```

### Critical Receiver Electrical Rules:
1. **NRF24 Adapter Board Power:** If using the black adapter board with onboard AMS1117-3.3 regulator, connect adapter `VCC` to **5V** (never 3.3V, to avoid the $1.2\text{V}$ dropout trap).
2. **Adapter Ground:** Connect adapter `GND` directly to Blue Pill **GND** (mandatory common reference).
3. **Decoupling Capacitor:** Solder a **$10\mu\text{F} - 100\mu\text{F}$ capacitor** directly across `VCC` and `GND` of the NRF24 module.
