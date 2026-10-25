# 🚜 ROVER CHASSIS PINOUT & WIRING MATRIX (v2.0)

> **Platform:** CyberRover X5 Chassis  
> **Master Wireless Receiver:** ESP32-S3 (ESP-NOW to UART Bridge)  
> **Motor Controller:** STM32F103C8T6 (Blue Pill, 72 MHz ARM Cortex-M3)  
> **Motor Drivers:** Dual BTS7960 43A MOSFET H-Bridges  
> **Motors:** 4x 12V 600 RPM Johnson Geared Motors (555 Armature, 100mm Wheels)  
> **Power Rail:** 3S 2200 mAh LiPo (11.1V Nominal, 12.6V Peak) -> XT60 -> Common Ground Bus

---

## 1. ESP32-S3 to STM32 Blue Pill Interconnect

| ESP32-S3 Pin | Signal Direction | STM32 Blue Pill Pin | Description |
|:---|:---:|:---|:---|
| **GPIO 43 (U0TXD)** | $\longrightarrow$ | **PA10 (USART1 RX)** | Motor commands: `[0xAA, CMD, SPEED, CSUM, 0x55]` |
| **GPIO 44 (U0RXD)** | $\longleftarrow$ | **PA9 (USART1 TX)** | Chassis telemetry: battery volts & current data |
| **GND** | $\longleftrightarrow$ | **GND** | Direct low-resistance signal ground return |
| **5V / VIN** | Input | **5V Bus** | Powered from 5V 3A UBEC/buck regulator |

---

## 2. STM32 Blue Pill to Dual BTS7960 Motor Drivers

| Function | STM32 Pin | Driver 1 (Left Track) | Driver 2 (Right Track) | Signal Type |
|:---|:---|:---|:---|:---|
| **Left Forward PWM** | **PB8** | `L_EN` + `R_EN` tied HIGH; `RPWM` | — | 0–255 PWM (TIM4_CH3) |
| **Left Reverse PWM** | **PB9** | `LPWM` | — | 0–255 PWM (TIM4_CH4) |
| **Right Forward PWM** | **PB6** | — | `L_EN` + `R_EN` tied HIGH; `RPWM` | 0–255 PWM (TIM4_CH1) |
| **Right Reverse PWM** | **PB7** | — | `LPWM` | 0–255 PWM (TIM4_CH2) |
| **Driver 5V Logic** | **5V** | `VCC` | `VCC` | Logic power supply |
| **Driver Signal GND** | **GND** | `GND` | `GND` | Common ground return |

---

## 3. Sensor & Voltage Feedback Pinout

| Peripheral | STM32 Pin | Wiring Description |
|:---|:---|:---|
| **3S Battery Voltage Sensing** | **PA0 (ADC1_IN0)** | Center tap of 30k $\Omega$ / 7.5k $\Omega$ voltage divider (12.6V -> 2.52V) |
| **Left Driver Current Sensing** | **PA1 (ADC1_IN1)** | `IS` pin from Left BTS7960 |
| **Right Driver Current Sensing**| **PA2 (ADC1_IN2)** | `IS` pin from Right BTS7960 |
| **Chassis Status LED** | **PC13** | Onboard active-low LED (Heartbeat indicator) |

---

## 4. Power & High-Current Drivetrain Topology

```
[3S 2200 mAh LiPo Battery]
         │ (12 AWG with XT60)
         ├──► Dual BTS7960 B+ (12V High-Current Rail to 4x Johnson Motors)
         │
         ├──► 5V 3A Step-Down UBEC
         │        ├──► ESP32-S3 5V
         │        └──► STM32 Blue Pill 5V
         │
         └──► Common Star Ground Return (Negative battery post)
```
