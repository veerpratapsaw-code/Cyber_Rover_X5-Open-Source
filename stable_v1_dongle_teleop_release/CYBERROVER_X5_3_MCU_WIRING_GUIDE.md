# 🔌 CYBERROVER X5 — 3-MCU MASTER HARDWARE WIRING & PINOUT GUIDE

> **DOCUMENT STATUS:** Official Hardware Benchmark & Wiring Blueprint (Stable Release v1.0)  
> **LEAD ARCHITECT:** Veer Pratap Saw (Veer) | Katras / Ramgarh, Jharkhand  
> **TARGET VEHICLE:** CyberRover X5 (Industrial Heavy-Duty Reconnaissance UGV)  
> **SCOPE:** Full pin-by-pin electrical connections for all 3 Microcontrollers, Dual BTS7960 43A Drivers, Power Distribution, and Optical Telemetry.

---

## 🗺️ SYSTEM OVERVIEW & WIRING SCHEMATIC

```
+========================================================================================+
|                              CYBERROVER X5 WIRING TOPOLOGY                            |
+========================================================================================+

     [ LAPTOP USB PORT ]
              |
         +5V, GND, D+, D-
              |
              v
     +-----------------------------------+
     | MCU 1: ESP32-CAM USB DONGLE (TX)  |  <--- Channel 1 Locked (2412 MHz)
     | U0TXD (GPIO 1), U0RXD (GPIO 3)    |  <--- 115200 Baud Serial Stream from Python
     +-----------------------------------+
                      ) ) )
                ESP-NOW 2.4 GHz
            (Bi-Directional Wireless)
                      ( ( (
     +-----------------------------------+
     | MCU 2: ESP32-S3 ROVER GATEWAY     |  <--- Channel 1 Locked (2412 MHz)
     | Pin 17 (TX1) ------------------+  |
     | Pin 18 (RX1) <---------------+ |  |
     | 5V & GND (System Bus)        | |  |
     +------------------------------|-|--+
                                    | |
                           UART Link| | (115200 Baud, 8N1)
                                    | |
                                    v |
     +--------------------------------v--+
     | MCU 3: STM32 BLUE PILL SUPERVISOR |  <--- 72 MHz ARM Cortex-M3 (3.3V Logic)
     | PA10 (RX1) <-------------------+  |
     | PA9  (TX1) --------------------+  |
     |                                   |
     | 3.3V Logic Rail ------------+     |
     | GND (Star Ground) --------+ |     |
     +---------------------------|-|-----+
                                 | |
               +-----------------+ | (Common 3.3V Logic Rail)
               |                   |
               v                   v
     +-------------------+   +-------------------+
     | LEFT BTS7960      |   | RIGHT BTS7960     |
     | VCC <-------------+   | VCC <-------------+
     | GND <-------------+   | GND <-------------+
     | R_EN, L_EN = 3.3V |   | R_EN, L_EN = 3.3V |
     | RPWM <--- PB9     |   | RPWM <--- PB6     | (TIM4 Hardware PWM)
     | LPWM <--- PB8     |   | LPWM <--- PB7     | (TIM4 Hardware PWM)
     +-------------------+   +-------------------+
          |         |             |         |
      (M+ / M-) (B+ / B-)     (M+ / M-) (B+ / B-)
          |         |             |         |
          v         |             v         |
     +-----------+  |        +-----------+  |
     | 2x LEFT   |  +--------| 2x RIGHT  |  +----+ 3S 12V Li-ion Battery
     | JOHNSON   |           | JOHNSON   |       | (High Current Bus)
     | MOTORS    |           | MOTORS    |       v
     +-----------+           +-----------+   [ +11.1V to +12.6V ]
```

---

## 📌 DETAILED PINOUT SPECIFICATION TABLES

### 1. MCU 1: ESP32-CAM USB Dongle Transmitter (Pilot Side)

* **Physical Module:** AI-Thinker ESP32-CAM mounted on USB-MB Baseboard or FTDI FT232RL.
* **Location:** Plugged directly into pilot's Laptop USB port.

| Pin Label | Type | Wire Connection | Description |
| :--- | :--- | :--- | :--- |
| **5V / VCC** | Power In | Laptop USB +5.0V | Powers ESP32 core and Wi-Fi radio (500mA USB bus) |
| **GND** | Ground | Laptop USB GND | Common pilot reference ground |
| **U0TXD (GPIO 1)**| Digital Out | USB-TTL RX (CH340 / CP2102)| Sends ACK telemetry (`STAT:ACK_OK,RSSI:...`) to Python |
| **U0RXD (GPIO 3)**| Digital In | USB-TTL TX (CH340 / CP2102)| Receives 6-byte frames `[0xAA, CMD, SPD, GEAR, TURBO, 0x55]` |
| **GPIO 0** | Config | Open / Floating (RUN Mode) | Pull to GND **only** when flashing firmware via Arduino IDE |

---

### 2. MCU 2: ESP32-S3 DevKit (N16R8) — Rover RF Gateway Bridge

* **Physical Module:** ESP32-S3-WROOM-1 DevKit (N16R8).
* **Location:** Mounted on Rover Main Electronics Tray.

| Pin Label | Type | Wire Connection | Notes / Requirements |
| :--- | :--- | :--- | :--- |
| **5V / VIN** | Power In | Rover +5.0V Regulated Bus | Fed from high-efficiency DC-DC buck converter |
| **GND** | Ground | Rover Master Star Ground | Must be tied directly to STM32 GND and Battery Negative |
| **GPIO 17 (TX1)**| Digital Out | **STM32 Pin PA10 (RX1)** | High-speed inter-MCU UART (<10us packet forwarding) |
| **GPIO 18 (RX1)**| Digital In | **STM32 Pin PA9 (TX1)** | Receives non-blocking `$M,Left,Right\n` motor telemetry |
| **USB Native Port**| Diagnostics| Optional Laptop USB | Used for initial flashing & viewing live signal HUD |

> [!IMPORTANT]
> **BUS CONTENTION WARNING:** If you connect a separate USB-to-TTL adapter to the STM32's `PA10` pin while ESP32-S3 `GPIO 17` is also wired to `PA10`, both chips will fight each other, destroying data! **Never attach two TX outputs to PA10 simultaneously.** The ESP32-S3 automatically relays all necessary data.

---

### 3. MCU 3: STM32F103C8T6 (Blue Pill) — Drivetrain Supervisor

* **Physical Module:** STM32F103C8T6 minimum system development board @ 72 MHz.
* **Operating Logic:** 100% Native 3.3V CMOS.

| Pin | Subsystem | Connected To | Function / Timing |
| :--- | :--- | :--- | :--- |
| **PB6** | Right Motor FWD | Right BTS7960 **RPWM** | Hardware Timer `TIM4_CH1` PWM (0 to 255) |
| **PB7** | Right Motor REV | Right BTS7960 **LPWM** | Hardware Timer `TIM4_CH2` PWM (0 to 255) |
| **PB9** | Left Motor FWD | Left BTS7960 **RPWM** | Hardware Timer `TIM4_CH4` PWM (0 to 255) |
| **PB8** | Left Motor REV | Left BTS7960 **LPWM** | Hardware Timer `TIM4_CH3` PWM (0 to 255) |
| **PA10**| Inter-MCU RX | ESP32-S3 **GPIO 17 (TX1)**| Receives 5-byte command packets `[0xAA, CMD, SPD, CSUM, 0x55]` |
| **PA9** | Inter-MCU TX | ESP32-S3 **GPIO 18 (RX1)**| Transmits 10 Hz telemetry string `$M,Left,Right\n` |
| **PB0** | Current Sense | Right BTS7960 **R_IS** | ADC8 (Right Forward Motor Current Sense) |
| **PB1** | Current Sense | Right BTS7960 **L_IS** | ADC9 (Right Reverse Motor Current Sense) |
| **PA0** | Current Sense | Left BTS7960 **R_IS** | ADC0 (Left Forward Motor Current Sense) |
| **PA1** | Current Sense | Left BTS7960 **L_IS** | ADC1 (Left Reverse Motor Current Sense) |
| **PB5** | Optical Status | Red Status LED (Cathode) | Active-LOW (Red ON = Reverse / Watchdog Cut) |
| **PA11**| Optical Status | Blue Status LED (Cathode)| Active-LOW (Blue ON = Forward / Active Drive) |
| **PC13**| Heartbeat LED | Onboard Blue Pill LED | Toggles on every valid incoming teleop frame |
| **3.3V**| Logic Power Out | Both BTS7960 **VCC Pins** | **MANDATORY:** Lowers 74HC244 buffer threshold to 2.31V! |
| **5V** | Power In | Rover +5.0V Bus | Feeds onboard 3.3V LDO to guarantee stable 3.3V output |
| **GND** | Ground | Rover Master Star Ground | Ground reference for PWM, ADCs, and UART |

---

### 4. Dual BTS7960 (IBT-2) 43A Motor Drivers

Each driver module controls one pair of 12V 600 RPM Johnson motors in parallel.

```
       BTS7960 8-PIN LOGIC HEADER PINOUT
      +---------------------------------+
      | (1) VCC   (2) GND   (3) L_EN    |
      | (4) R_EN  (5) LPWM  (6) RPWM    |
      | (7) L_IS  (8) R_IS              |
      +---------------------------------+
```

#### A. Left Side BTS7960 Driver (Controls Left Wheels)
| Driver Pin | Wire Destination | Electrical Function |
| :--- | :--- | :--- |
| **VCC** | **STM32 3.3V Rail** | Powers 74HC244 buffer at 3.3V ($V_{IH} = 2.31\text{V}$) |
| **GND** | **System Star GND** | Logic ground reference |
| **L_EN** | **Tied to VCC (3.3V)** | Permanently enables Reverse Half-Bridge |
| **R_EN** | **Tied to VCC (3.3V)** | Permanently enables Forward Half-Bridge |
| **RPWM** | **STM32 Pin PB9** | Forward PWM (`TIM4_CH4`) |
| **LPWM** | **STM32 Pin PB8** | Reverse PWM (`TIM4_CH3`) |
| **R_IS** | **STM32 Pin PA0** | Forward current diagnostic analog voltage |
| **L_IS** | **STM32 Pin PA1** | Reverse current diagnostic analog voltage |
| **B+** | **Battery +12V (+3S)** | Heavy 14-16 AWG high-current supply screw terminal |
| **B-** | **Battery Negative (-)** | Heavy 14-16 AWG ground return screw terminal |
| **M+ / M-**| **2x Left Johnson Motors** | Heavy wiring to armature terminals (parallel pair) |

#### B. Right Side BTS7960 Driver (Controls Right Wheels)
| Driver Pin | Wire Destination | Electrical Function |
| :--- | :--- | :--- |
| **VCC** | **STM32 3.3V Rail** | Powers 74HC244 buffer at 3.3V ($V_{IH} = 2.31\text{V}$) |
| **GND** | **System Star GND** | Logic ground reference |
| **L_EN** | **Tied to VCC (3.3V)** | Permanently enables Reverse Half-Bridge |
| **R_EN** | **Tied to VCC (3.3V)** | Permanently enables Forward Half-Bridge |
| **RPWM** | **STM32 Pin PB6** | Forward PWM (`TIM4_CH1`) |
| **LPWM** | **STM32 Pin PB7** | Reverse PWM (`TIM4_CH2`) |
| **R_IS** | **STM32 Pin PB0** | Forward current diagnostic analog voltage |
| **L_IS** | **STM32 Pin PB1** | Reverse current diagnostic analog voltage |
| **B+** | **Battery +12V (+3S)** | Heavy 14-16 AWG high-current supply screw terminal |
| **B-** | **Battery Negative (-)** | Heavy 14-16 AWG ground return screw terminal |
| **M+ / M-**| **2x Right Johnson Motors**| Heavy wiring to armature terminals (parallel pair) |

---

## ⚡ ELECTRICAL ENGINEERING TRUTHS & GOLDEN RULES

### Rule 1: The 3.3V vs 5V BTS7960 Buffer Physics
> [!CAUTION]
> **NEVER connect the 8-pin control header `VCC` pin of the BTS7960 to 5.0V!**  
> * If `VCC` is 5.0V, the onboard **74HC244** octal buffer requires a minimum HIGH input voltage of:  
>   $$V_{IH,\text{min}} = 0.7 \times 5.0\text{V} = \mathbf{3.50\text{V}}$$  
> * The STM32 outputs **3.30V**, which falls directly into the invalid/marginal logic threshold. When the 4 high-power Johnson motors commutate and pull 10A+, ground bounce causes the 3.3V signal to dip, dropping PWM pulses and producing violent motor chattering.  
> * **When connected to 3.3V:**  
>   $$V_{IH,\text{min}} = 0.7 \times 3.3\text{V} = \mathbf{2.31\text{V}}$$  
>   This yields a massive **+0.99V noise-immune safety margin** ($3.30\text{V} - 2.31\text{V} = +0.99\text{V}$). The BTS7960 power IC's native input threshold is only 2.2V, meaning 3.3V drives the driver with 100% industrial reliability without any level-converter boards!

### Rule 2: Single Star Grounding (Prevent Ground Loops)
All ground paths must meet at a **Single Central Star Ground Point**:
1. Battery Negative (`B-`)
2. STM32 Ground (`GND`)
3. ESP32-S3 Ground (`GND`)
4. BTS7960 Driver Logic Grounds (`GND`)
* Never route high-current motor return paths through breadboard rails or thin jumper wires. High-current motor ground returns belong exclusively on heavy 14–16 AWG silicone wires to the `B-` screw terminals.

### Rule 3: Snubber Capacitors on Johnson Motors
* Solder a **0.1µF (100nF) 50V ceramic disc capacitor** directly across each Johnson motor's `+` and `-` terminal tabs.
* Solder a 0.1µF capacitor from each terminal tab to the motor's metal can chassis.
* This absorbs RF brush commutation arcing at the source, preventing 2.4 GHz RF degradation.

---

## 🚦 OPTICAL STATUS INDICATOR PALETTE

The status cluster uses a common-anode dual-color LED (Red/Blue):
* **Anode (+)**: Connected to 3.3V via 220Ω resistor.
* **Red Cathode (-)**: Connects to **PB5** (LOW = Red ON).
* **Blue Cathode (-)**: Connects to **PA11** (LOW = Blue ON).

| Color | PB5 (Red) | PA11 (Blue) | Vehicle State |
| :--- | :--- | :--- | :--- |
| **OFF** | HIGH | HIGH | Motors Stopped / Neutral (0.00V GND Locked) |
| **BLUE** | HIGH | LOW | Driving Forward / Gentle Steering Curves |
| **RED** | LOW | HIGH | Reverse Driving OR Dead-Man Watchdog Engaged |
| **MAGENTA** (Red+Blue)| LOW | LOW | Rapid Tank Spin (Counter-Rotating Left & Right) |

---
**END OF MASTER WIRING SPECIFICATION**
