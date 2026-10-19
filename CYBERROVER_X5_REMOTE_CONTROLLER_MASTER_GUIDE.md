# 🎮 CYBERROVER X5: REMOTE CONTROLLER MASTER GUIDE & HARDWARE ENCYCLOPEDIA
### *The Complete Engineering, Construction, and Architectural Specification for the Handheld Dual-Radio Transmitter*

> **Document Status**: Authoritative Production Blueprint & Engineering Reference  
> **Vehicle Platform**: CyberRover X5 (Autonomous Heavy-Duty Hazmat & Disaster Reconnaissance UGV)  
> **Lead Architect**: Veer Pratap Saw (Veer), Class 10 Student | Katras / Dhanbad / Ramgarh, Jharkhand, India  
> **Chassis Construction**: 2mm/3mm MDF Board with 6mm Plywood Corner Reinforcement (Zero 3D-Printer Required)  
> **Electronics Standard**: Zero-DuPont Dotted Perfboard (Direct Soldered Ribbon Bus)  
> **Power Standard**: 1S 18650 Li-ion + TP4056 USB-C + Mini 5V Boost Converter  
> **Visual Reference Archive**: High-resolution concept renders and raw unpainted wood views are archived in `remote_designs/`.

---

## 📑 TABLE OF CONTENTS
1. [Executive Summary & Heritage (Lessons from X4.3)](#1-executive-summary--heritage-lessons-from-x43)
2. [Complete Technical Rationale: Why NOT X vs Why DID Use Y](#2-complete-technical-rationale-why-not-x-vs-why-did-use-y)
3. [Physical Form Factor & Ergonomic Blueprint](#3-physical-form-factor--ergonomic-blueprint)
4. [8-Channel Control Architecture & Operational Envelope](#4-8-channel-control-architecture--operational-envelope)
5. [Dual-Radio Network Topology (NRF24 + LoRa Hybrid)](#5-dual-radio-network-topology-nrf24--lora-hybrid)
6. [Electrical Architecture & Level-Shifting Analysis](#6-electrical-architecture--level-shifting-analysis)
7. [Master STM32 Pinout Matrix (Zero Pin Collisions)](#7-master-stm32-pinout-matrix-zero-pin-collisions)
8. [Power & Charging Subsystem (The 1S Voltage Dropout Fix)](#8-power--charging-subsystem-the-1s-voltage-dropout-fix)
9. [Rear Service Hatch & External Programming Interface](#9-rear-service-hatch--external-programming-interface)
10. [Step-by-Step Workshop Construction & Woodworking Guide](#10-step-by-step-workshop-construction--woodworking-guide)
11. [Complete Production Firmware (STM32 Arduino C++)](#11-complete-production-firmware-stm32-arduino-c)

---

## 1. EXECUTIVE SUMMARY & HERITAGE (LESSONS FROM X4.3)

In CyberRover X4.3, the handheld controller was constructed inside a cardboard casing with loose DuPont jumper wires, a single 18650 cell, and basic analog thumbsticks. While it successfully won the **1st Prize State-Level Science Championship in Ramgarh**, real-world field operations revealed four critical engineering vulnerabilities:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ THE 4 CRITICAL FAILURE POINTS OF THE LEGACY X4.3 CONTROLLER                           │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 1. DuPont Jumper Wire Loosening:                                                       │
│    Female-to-female jumper pins rely on tiny internal spring leaves. Handheld vibration│
│    and thumb force caused intermittent disconnects. Hot glue applied externally failed │
│    to secure internal electrical contact.                                              │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 2. The I2C OLED "Black Screen" Lockup:                                                 │
│    I2C bus (SDA/SCL) has no hardware timeout in basic libraries. A 1ms vibration glitch│
│    on a loose wire permanently locked the MCU I2C state machine, freezing the display. │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 3. Buzzer Switching Voltage Sag:                                                       │
│    When the 5V active alert buzzer fired, its inductive surge dragged down the 3.3V    │
│    rail, causing RF packet drops and display jitter.                                   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 4. Cardboard Casing Flexure:                                                           │
│    Cardboard flexed under grip pressure, pulling wires and straining solder joints.    │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

**The CyberRover X5 Remote is engineered specifically to eliminate all four vulnerabilities permanently**, without requiring an expensive 3D printer or factory-printed PCBs.

---

## 2. COMPLETE TECHNICAL RATIONALE: WHY NOT "X" VS WHY DID USE "Y"

This section records every design trade-off and component rejection decided during engineering reviews:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   THE MASTER COMPONENT DECISION MATRIX                                          │
├────────────────────────────┬──────────────────────────────────────┬─────────────────────────────────────────────┤
│ Subsystem / Component      │ ❌ What Was REJECTED & Why            │ ✅ What Was SELECTED & Why                  │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 1. Enclosure Material      │ ❌ Cardboard: Flexes, pulls wires.   │ ✅ 2mm/3mm MDF + 6mm Plywood Corners:       │
│                            │ ❌ 3D Printed PETG: No 3D printer.   │    Rigid, easy to cut with hand tools,      │
│                            │ ❌ Pre-fab ABS: Hard to drill clean. │    wood screws bite firmly into corners.    │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 2. Joystick Hardware       │ ❌ PS4 Repair Sticks: 14 bare pins,  │ ✅ Standard 5-Pin Dual-Axis XY Breakout:    │
│                            │    no mounting holes, solder snaps.  │    4x M3 corner mounting holes, large 2.54mm│
│                            │ ❌ Elecrow Crowtail: Hack shorts     │    through-hole solder pads, independent    │
│                            │    button to analog line, cuts ADC   │    digital pushbutton (`SW`), full 0–4095   │
│                            │    range in half (200–700).          │    ADC range, costs only ₹45 each.          │
│                            │ ❌ C&K Industrial: ₹587 bare switch. │                                             │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 3. Microcontroller         │ ❌ Arduino Nano: Only 10-bit ADC,    │ ✅ STM32F103C8T6 (Blue Pill):               │
│                            │    runs out of pins, 5V logic resets │    72 MHz 32-bit ARM, 10x 12-bit linear ADCs│
│                            │    3.3V NRF24 radios.                │    (0–4095), native 3.3V SPI/UART/I2C,      │
│                            │ ❌ ESP32-S3: Damaged solder pins,    │    fits standard 0.6" perfboard headers.    │
│                            │    0.9" wide spacing doesn't fit.    │                                             │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 4. Logic Level Shifting    │ ❌ 5V-to-3.3V Bidirectional Shifters:│ ✅ Zero Level Converters Needed:            │
│                            │    Adds 8 extra failure points,      │    Because STM32 is native 3.3V, NRF24,     │
│                            │    propagation delay, wire clutter.  │    LoRa, OLED, and Pots all share the exact │
│                            │                                      │    same native 3.3V logic rail directly!    │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 5. Power Regulation (1S)   │ ❌ Raw 1S into AMS1117-3.3V LDO:     │ ✅ 1S 18650 + TP4056 + Mini 5V Boost (MT3608):│
│                            │    AMS1117 has 1.1V dropout! When    │    Boosts 3.0V–4.2V battery to constant 5.0V│
│                            │    battery drops to 3.7V, radio rail │    before feeding STM32 onboard LDO. Zero   │
│                            │    dips to 2.6V, causing MCU reset.  │    radio brownouts even on empty battery!   │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 6. Primary RF Link         │ ❌ Wi-Fi / ESP-NOW Alone:            │ ✅ Dual-Radio Hybrid (NRF24 + LoRa):        │
│                            │    Fails behind 1 concrete wall (35m)│    NRF24 delivers 10ms high-speed driving;  │
│                            │ ❌ LoRa Alone: 200ms latency causes  │    LoRa 915 MHz provides 400m building      │
│                            │    1.5m blind travel at 11 km/h.     │    penetration and limp-home failsafe.      │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 7. Display Allocation      │ ❌ 0.96" on Remote: Too small to read│ ✅ 1.3" OLED on Remote (SH1106 HUD):        │
│                            │    while thumbs are on gimbals.      │    35% larger viewing area for pilot.       │
│                            │                                      │    0.96" OLED goes on the Rover chassis for │
│                            │                                      │    local maintenance and diagnostics.       │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 8. Throttle Management     │ ❌ Software Fixed Throttle:          │ ✅ Dedicated Rotary Speed Governor (CH3):   │
│                            │    A 11.3 km/h rover indoors will    │    Potentiometer limits top speed from 10%  │
│                            │    ram walls on a 2mm stick nudge.   │    (1.1 km/h crawl) to 100% (11.3 km/h).    │
├────────────────────────────┼──────────────────────────────────────┼─────────────────────────────────────────────┤
│ 9. Wiring Architecture     │ ❌ Loose DuPont Jumpers:             │ ✅ Direct-Soldered Ribbon Cable + Perfboard:│
│                            │    Guaranteed to loosen with motion. │    Soldered joints with heat-shrink tubing. │
│                            │                                      │    Removable headers only for MCU & radios. │
└────────────────────────────┴──────────────────────────────────────┴─────────────────────────────────────────────┘
```

---

## 3. PHYSICAL FORM FACTOR & ERGONOMIC BLUEPRINT

### Comparative Dimensions: Commercial Radios vs. CyberRover X5
* **RadioMaster Pocket:** $156.6\text{ mm (W)} \times 125.3\text{ mm (H)} \times 65.1\text{ mm (D)}$
* **RadioMaster TX12:** $159.0\text{ mm (W)} \times 170.0\text{ mm (H)} \times 108.0\text{ mm (D)}$
* **RadioMaster Boxer:** $178.0\text{ mm (W)} \times 180.0\text{ mm (H)} \times 85.0\text{ mm (D)}$
* **CyberRover X5 Remote:** **$170.0\text{ mm (W)} \times 125.0\text{ mm (H)} \times 42.0\text{ mm (D)}$**

```
        ◄─────────────────────── 170 mm (Width) ────────────────────────►
   ▲    ┌───────────────────────────────────────────────────────────────┐
   │    │  [2.4G ANTENNA]                               [LoRa ANTENNA]  │
   │    │                                                               │
   │    │   [ARM TOGGLE]     (SPEED KNOB)     (TILT KNOB)   [MODE TOGGLE│
 125 mm │                     POT 1            POT 2                    │
 (H)    │                  ┌───────────────────┐                        │
   │    │                  │   1.3" OLED HUD   │                        │
   │    │                  │  CYBERROVER X5    │                        │
   │    │                  └───────────────────┘                        │
   │    │       ┌───┐                                       ┌───┐       │
   │    │      ┌┘   └┐          (O)           (O)          ┌┘   └┐      │
   │    │     │  (O)  │         HORN         LIGHT        │  (O)  │     │
   │    │      └┐   ┌┘                                     └┐   ┌┘      │
   ▼    │       └───┘                                       └───┘       │
        │    LEFT JOYSTICK                               RIGHT JOYSTICK │
        │     (THROTTLE)                                   (STEERING)   │
        └───────────────────────────────────────────────────────────────┘
                                 ▲
                          Depth: 42 mm
```

### Ergonomic Placement Rationale:
1. **$170\text{ mm}$ Width:** Centers the gimbals $105\text{ mm}$ apart, matching the natural rest angle of human thumbs while leaving a $40\text{ mm}$ central cavity for the 1.3" OLED display.
2. **$125\text{ mm}$ Height:** Matches the exact palm-grip height of the RadioMaster Pocket.
3. **$42\text{ mm}$ Depth:** Leaves $18\text{ mm}$ of clear space behind the $24\text{ mm}$ deep joystick mechanisms for ribbon cable routing, while remaining significantly slimmer than commercial radios ($65\text{--}108\text{ mm}$).
4. **Top Handle / Kickstand (Optional):** Acts as a roll cage protecting top SMA antenna jacks from impact, hooks fingers for one-handed transport, and tilts the transmitter at a $35^\circ$ angle when resting on a workbench.

---

## 4. 8-CHANNEL CONTROL ARCHITECTURE & OPERATIONAL ENVELOPE

```
┌─────────┬──────────────┬──────────────┬────────────────────────┬───────────────────────────────────────────┐
│ Channel │ Type         │ Physical Input│ Range / States         │ Vehicle Function / Action                 │
├─────────┼──────────────┼──────────────┼────────────────────────┼───────────────────────────────────────────┤
│ CH1     │ Analog       │ Right Stick X│ -100% to +100%         │ Steering (Differential left/right balance)│
│ CH2     │ Analog       │ Left Stick Y │ -100% to +100%         │ Throttle (Forward drive & reverse brake)  │
│ CH3     │ Analog       │ Rotary Knob 1│ 10% to 100%            │ Speed Governor (Caps max vehicle speed)   │
│ CH4     │ Analog       │ Rotary Knob 2│ 0° to 60°              │ FPV Camera Tilt (Micro-servo elevation)   │
│ CH5     │ Digital (2P) │ Toggle Switch│ DISARMED / ARMED       │ Master Emergency Stop (Cuts all PWM)      │
│ CH6     │ Digital (3P) │ 3-Way Switch │ CRUISE / TANK / CRAWL  │ Drive Dynamic Mode selector               │
│ CH7     │ Digital (2P) │ Push / Toggle│ OFF / ON               │ Tactical High-Power Headlights            │
│ CH8     │ Digital (Mom)│ Pushbutton   │ RELEASED / PRESSED     │ Vehicle Alarm Horn / Alert Buzzer         │
└─────────┴──────────────┴──────────────┴────────────────────────┴───────────────────────────────────────────┘
```

### Speed Envelope Scaling:
With 100mm wheels and 600 RPM Johnson motors, top vehicle velocity is **$11.31\text{ km/h}$** ($3.14\text{ m/s}$).
$$\text{Effective Motor PWM} = \text{Raw Joystick Throttle} \times \left(\frac{\text{CH3 Knob Value}}{100}\right)$$
* **CH3 set to 15% (Indoor Crawl):** Clamps speed to **$1.70\text{ km/h}$**. Safe for indoor navigation.
* **CH3 set to 50% (Patrol):** Clamps speed to **$5.65\text{ km/h}$**.
* **CH3 set to 100% (Sprint):** Unlocks full **$11.31\text{ km/h}$** for open terrain.

---

## 5. DUAL-RADIO NETWORK TOPOLOGY (NRF24 + LORA HYBRID)

```
                  ┌────────────────────────────────────────┐
                  │       Dual-Link State Machine          │
                  └──────────────────┬─────────────────────┘
                                     │
           ┌─────────────────────────┴─────────────────────────┐
           ▼                                                   ▼
 ┌───────────────────┐                               ┌───────────────────┐
 │   Primary Link    │                               │   Secondary Link  │
 │   NRF24L01 (2.4G) │                               │   LoRa (915 MHz)  │
 ├───────────────────┤                               ├───────────────────┤
 │ • 50 Hz to 100 Hz │                               │ • 5 Hz refresh    │
 │ • 10ms - 15ms lag │                               │ • 180ms - 220ms   │
 │ • Max speed: 11.3 │                               │ • Clamped speed:  │
 │   km/h (Sprint)   │                               │   1.8 km/h (Safe) │
 └─────────┬─────────┘                               └─────────┬─────────┘
           │                                                   │
           ▼                                                   ▼
 [Normal High-Speed Operation]                       [Limp-Home Failsafe]
 (Vehicle within 100m / LOS)                         (Penetrating concrete walls)
```

### The 10-Byte Packed Control Packet:
```cpp
struct __attribute__((__packed__)) CyberControlPacket {
  uint8_t  header;       // 0xAA (Packet sync byte)
  int8_t   steer;        // CH1: -100 to +100
  int8_t   throttle;     // CH2: -100 to +100
  uint8_t  speed_gov;    // CH3: 10 to 100 (% Max Speed)
  uint8_t  cam_tilt;     // CH4: 0 to 60 (Degrees elevation)
  uint8_t  switches;     // Bit 0: Arm, Bit 1-2: Mode, Bit 3: Light, Bit 4: Horn
  uint16_t packet_id;    // 16-bit packet counter
  uint8_t  crc8;         // Cyclic Redundancy Check (CCITT 0x07)
};
```

---

## 6. ELECTRICAL ARCHITECTURE & LEVEL-SHIFTING ANALYSIS

Because the **STM32F103C8T6 runs natively on 3.3V logic**, all remote peripherals share the same voltage domain:

```
                       ALL HARDWARE IS NATIVE 3.3V!
    ┌───────────────────────────────────────────────────────────────┐
    │  • STM32 Blue Pill Logic:       3.3V                          │
    │  • NRF24L01+PA+LNA Radio:       3.3V (Burns instantly on 5V!) │
    │  • Reyax RYLR998 LoRa:          3.3V (3.0V - 3.6V native)     │
    │  • 1.3-inch OLED Display:       3.3V compatible               │
    │  • Joysticks & Potentiometers:  Powered from 3.3V rail        │
    │  • Toggle & Push Switches:      Switched to GND (3.3V logic)  │
    └───────────────────────────────────────────────────────────────┘
```

* **No Level Shifters Required:** Eliminates 8 logic-level converter channels, removing component clutter and failure points.
* **NRF24 Power Filtering:** Solder a **$100\mu\text{F}$ electrolytic capacitor + $0.1\mu\text{F}$ ceramic capacitor** directly across the NRF24 `VCC` and `GND` pins to absorb 115mA RF transmission spikes.
* **I2C Bus Lockup Protection:** Add external **$4.7\text{k}\Omega$ pull-up resistors** from `SCL` and `SDA` to `3.3V`.

---

## 7. MASTER STM32 PINOUT MATRIX (ZERO PIN COLLISIONS)

| Peripheral Component | Pins Used | Hardware Resource | Electrical Notes |
| :--- | :--- | :--- | :--- |
| **Left Joystick (Throttle)** | `PA0` | ADC1 Channel 0 | 12-Bit Linear ADC ($0\text{--}4095$) |
| **Left Joystick (Pan / Yaw)** | `PA1` | ADC1 Channel 1 | 12-Bit Linear ADC ($0\text{--}4095$) |
| **Right Joystick (Steering)** | `PA2` | ADC1 Channel 2 | 12-Bit Linear ADC ($0\text{--}4095$) |
| **Right Joystick / Pot (Cam Tilt)**| `PA3`| ADC1 Channel 3 | 12-Bit Linear ADC ($0\text{--}4095$) |
| **Potentiometer 1 (Speed Gov)**| `PA4` | ADC1 Channel 4 | 12-Bit Linear ADC ($0\text{--}4095$) |
| **Potentiometer 2 (Turn Radius Limiter)**| `PB0`| ADC1 Channel 8 | 12-Bit Linear ADC ($0\text{--}4095$) |
| **Potentiometer 3 (Aux / Dimmer)**| `PB1`| ADC1 Channel 9 | 12-Bit Linear ADC ($0\text{--}4095$) |
| **NRF24L01 Radio (SPI1)** | `PA5` (SCK) | Hardware SPI1 | High-speed 3.3V bus |
| | `PA6` (MISO) | Hardware SPI1 | High-speed 3.3V bus |
| | `PA7` (MOSI) | Hardware SPI1 | High-speed 3.3V bus |
| | `PA8` (CE) | GPIO Output | Chip Enable |
| | `PB12` (CSN) | GPIO Output | Chip Select Not (bypasses PA4 MODF trap) |
| **USB-TTL Monitor & Flashing** | `PA9` (TX1) | Hardware USART1 | Connect to USB-TTL RXD |
| | `PA10` (RX1)| Hardware USART1 | Connect to USB-TTL TXD |
| **1.3" I2C OLED Display** | `PB6` (SCL) | Hardware I2C1 | Add $4.7\text{k}\Omega$ pullup to 3.3V |
| | `PB7` (SDA) | Hardware I2C1 | Add $4.7\text{k}\Omega$ pullup to 3.3V |
| **Arm / E-Stop Toggle Switch** | `PB13` | Digital Input | Internal `INPUT_PULLUP` to GND |
| **3-Way Switch #1 (Drive Mode)**| `PB8`, `PB9` | Digital Inputs | Internal `INPUT_PULLUP` to GND (Pos 1: Cruise, Pos 2: Sport, Open: Crawl) |
| **3-Way Switch #2 (Lights / Aux)**| `PB10`, `PB11`| Digital Inputs | Internal `INPUT_PULLUP` to GND (Pos 1: Low, Pos 2: High, Open: OFF) |
| **Momentary Push Button #1** | `PB14` | Digital Input | Internal `INPUT_PULLUP` to GND (Horn / Siren / Turbo) |
| **Momentary Push Button #2** | `PB15` | Digital Input | Internal `INPUT_PULLUP` to GND (E-Brake / Camera Reset) |
| **Status Heartbeat LED** | `PC13` | Onboard LED | Blinks at 1 Hz when transmitting |
| **Spare Expansion GPIOs** | `PB4`, `PB5`, `PA11`, `PA12`, `PC14`, `PC15`, `PA15`, `PB3` | GPIOs | 8 pins completely free for buzzer, haptics, or gimbal clicks |

* **Hardware Integrity:** 100% Native 3.3V logic throughout; zero level converters; switches switch cleanly to GND.

---

## 8. POWER & CHARGING SUBSYSTEM (THE 1S DROPOUT FIX)

```
 [1S 18650 LI-ION CELL] (3.0V - 4.2V)
          │
          ▼
 [TP4056 USB-C CHARGER MODULE] ──► Charges cell safely via smartphone USB-C cable
          │ (B+, B-)
          ▼ (OUT+, OUT-)
 [MASTER ROCKER POWER SWITCH]
          │
          ▼
 [MINI 5V BOOST CONVERTER (MT3608)] ──► Boosts variable 3.0V-4.2V to constant 5.0V
          │
          ├──────────────────────────► 5V Alert Buzzer & Haptic Motor Driver
          ▼
 [STM32 "5V" POWER INPUT PIN] ───────► High-efficiency onboard RT9193 3.3V LDO
          │
          ▼ (Constant 3.30V Bus)
 ┌────────────────────────────────────────────────────────┐
 │ • STM32 ARM Cortex-M3 Core                             │
 │ • NRF24L01+PA+LNA (+ 100µF capacitor)                  │
 │ • Reyax RYLR998 LoRa Module                            │
 │ • 1.3-inch OLED HUD                                    │
 │ • 2x Joysticks & 2x Potentiometers                     │
 └────────────────────────────────────────────────────────┘
```

---

## 9. REAR SERVICE HATCH & EXTERNAL PROGRAMMING INTERFACE

```
                         REAR PANEL LAYOUT
          ┌───────────────────────────────────────────────────────────┐
          │                                                           │
          │                                                           │
          │               REMOVABLE SERVICE HATCH DOOR                │
          │         ┌───────────────────────────────────────┐         │
          │         │                                       │         │
          │         │  [USB-C PORT]      [5-PIN PROGRAM]    │         │
          │         │  (TP4056 Charging) (TTL Flash Header) │         │
          │         │                                       │         │
          │         │  [SLIDE SWITCH]    [18650 BATTERY]    │         │
          │         │  (BOOT0: RUN/PROG) (Internal Holder)  │         │
          │         │                                       │         │
          │         └───────────────────────────────────────┘         │
          │                   Held by 2 small screws                  │
          └───────────────────────────────────────────────────────────┘
```

### The 5-Pin Programming Header:
Solder a 5-pin female header right beside the TP4056 USB-C port, wired internally to the STM32 Blue Pill:
1. **Pin 1:** `GND`
2. **Pin 2:** `5V`
3. **Pin 3:** `PA9` (STM32 USART1 TX ➔ Connect to USB-TTL RX)
4. **Pin 4:** `PA10` (STM32 USART1 RX ➔ Connect to USB-TTL TX)
5. **Pin 5:** `RESET` (STM32 NRST pin)

* Alongside this header, a miniature slide switch routes the STM32 `BOOT0` pin:
  * **Slide UP:** Connects `BOOT0` to `3.3V` (Enter Bootloader for flashing via Arduino IDE / STM32CubeProgrammer).
  * **Slide DOWN:** Connects `BOOT0` to `GND` (Normal operating mode).
* **Benefit:** You can flash firmware and recharge the internal battery without disassembling the transmitter.

---

## 10. STEP-BY-STEP WORKSHOP CONSTRUCTION & WOODWORKING GUIDE

### Material Cutting Schedule:
* **Front Faceplate:** $170\text{ mm} \times 125\text{ mm} \times 2.5\text{ mm}$ MDF (1 piece)
* **Rear Backplate:** $170\text{ mm} \times 125\text{ mm} \times 2.5\text{ mm}$ MDF (1 piece)
* **Top & Bottom Rails:** $170\text{ mm} \times 42\text{ mm} \times 2.5\text{ mm}$ MDF (2 pieces)
* **Left & Right Sides:** $120\text{ mm} \times 42\text{ mm} \times 2.5\text{ mm}$ MDF (2 pieces)
* **Service Hatch Door:** $75\text{ mm} \times 50\text{ mm} \times 2.5\text{ mm}$ MDF (1 piece)
* **Corner Posts:** $15\text{ mm} \times 15\text{ mm} \times 37\text{ mm}$ 6mm Plywood (4 pieces)

### Assembly Sequence:
1. **Corner Gluing:** Glue the 4 plywood corner posts inside the side walls using PVA wood glue (Fevicol). Clamp for 2 hours to form a rigid rectangular box frame.
2. **Faceplate Drilling:** Mark component centers with a pencil:
   * Drill two $26\text{ mm}$ holes for the joystick gimbals.
   * Cut a $35 \times 24\text{ mm}$ rectangular window for the 1.3" OLED.
   * Drill $7\text{ mm}$ holes for potentiometers and $6\text{ mm}$ holes for toggle switches.
3. **Top Edge Antenna Mounts:** Drill two $6.5\text{ mm}$ holes on the top rail spaced $110\text{ mm}$ apart. Secure the gold SMA bulkhead female connectors using their supplied lock nuts.
4. **Wiring & Solder Phase:** 
   * Mount joysticks, pots, switches, and OLED to the front panel using counter-sunk M3 bolts.
   * Solder 4-conductor flat ribbon cables directly to each joystick.
   * Run common ground and 3.3V buses across switches and potentiometers.
   * Route cables down to the Dotted Perfboard holding the STM32 Blue Pill socket.
5. **Finishing:** Sand outer edges with 240-grit sandpaper, apply 1 coat of diluted PVA wood sealer, sand with 400-grit, and finish with two light coats of matte black spray paint or carbon-fiber vinyl wrap.

---

## 11. COMPLETE PRODUCTION FIRMWARE (STM32 ARDUINO C++)

```cpp
/**
 * CYBERROVER X5 - INDUSTRIAL DUAL-RADIO TRANSMITTER FIRMWARE
 * Platform: STM32F103C8T6 (Blue Pill) @ 72 MHz
 * Dual RF: NRF24L01+PA+LNA (SPI2 @ 50 Hz) + Reyax RYLR998 LoRa (USART1 @ 5 Hz)
 */

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <U8g2lib.h>

// --- PIN DEFINITIONS ---
#define PIN_STICK_LX    PA0 // Throttle
#define PIN_STICK_LY    PA1 // Steering
#define PIN_POT_SPEED   PA4 // Speed Governor
#define PIN_POT_TILT    PA5 // Camera Tilt
#define PIN_BAT_DIV     PB0 // Battery Voltage Divider
#define PIN_SW_ARM      PB1 // Master Arm (Active LOW)
#define PIN_SW_MODE1    PB8 // Drive Mode Pos 1
#define PIN_SW_MODE2    PB9 // Drive Mode Pos 2
#define PIN_BTN_HORN    PB10// Horn (Active LOW)
#define PIN_BTN_LIGHT   PB11// Light (Active LOW)
#define PIN_HAPTIC      PA15// Vibration Motor Driver
#define PIN_BUZZER      PB3 // Active Buzzer
#define PIN_LED_HB      PC13// Onboard Heartbeat LED

// NRF24 SPI2 Configuration
#define NRF_CE_PIN      PA8
#define NRF_CSN_PIN     PB12
RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
const uint64_t RF_PIPE = 0xE8E8F0F0E1LL;

// 1.3" I2C OLED (SH1106 Driver)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// 10-Byte Packed Control Packet
struct __attribute__((__packed__)) ControlPacket {
  uint8_t  header;       // 0xAA
  int8_t   steer;        // -100 to +100
  int8_t   throttle;     // -100 to +100
  uint8_t  speed_gov;    // 10 to 100%
  uint8_t  cam_tilt;     // 0 to 60 deg
  uint8_t  switches;     // Bit 0: Arm, Bit 1-2: Mode, Bit 3: Light, Bit 4: Horn
  uint16_t packet_id;    // Frame count
  uint8_t  crc8;         // Checksum
} txPacket;

uint16_t frameCounter = 0;
uint32_t lastNrfTime = 0;
uint32_t lastLoraTime = 0;
uint32_t lastDisplayTime = 0;

// CRC-8 Calculation (Polynomial 0x07)
uint8_t calculateCRC8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x07;
      else crc <<= 1;
    }
  }
  return crc;
}

// Deadband & Scaling Filter
int8_t processStick(int pin, bool invert) {
  int raw = analogRead(pin); // 12-bit: 0 to 4095
  int centered = raw - 2048;
  const int deadband = 80;
  if (abs(centered) < deadband) return 0;
  int mapped = (centered > 0) ? map(centered, deadband, 2047, 0, 100)
                              : map(centered, -2048, -deadband, -100, 0);
  return invert ? -constrain(mapped, -100, 100) : constrain(mapped, -100, 100);
}

void setup() {
  pinMode(PIN_LED_HB, OUTPUT);
  pinMode(PIN_HAPTIC, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SW_ARM, INPUT_PULLUP);
  pinMode(PIN_SW_MODE1, INPUT_PULLUP);
  pinMode(PIN_SW_MODE2, INPUT_PULLUP);
  pinMode(PIN_BTN_HORN, INPUT_PULLUP);
  pinMode(PIN_BTN_LIGHT, INPUT_PULLUP);

  analogReadResolution(12);

  // Initialize NRF24 Radio
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(108);
  radio.openWritingPipe(RF_PIPE);
  radio.stopListening();

  // Initialize LoRa UART1
  Serial1.begin(115200);

  // Initialize OLED HUD
  u8g2.begin();
  
  // Power-on Chirp
  digitalWrite(PIN_BUZZER, HIGH);
  delay(100);
  digitalWrite(PIN_BUZZER, LOW);
}

void loop() {
  uint32_t now = millis();

  // 1. Read Inputs & Formulate Packet
  txPacket.header = 0xAA;
  txPacket.throttle = processStick(PIN_STICK_LX, false);
  txPacket.steer = processStick(PIN_STICK_LY, false);
  txPacket.speed_gov = map(analogRead(PIN_POT_SPEED), 0, 4095, 10, 100);
  txPacket.cam_tilt = map(analogRead(PIN_POT_TILT), 0, 4095, 0, 60);

  // Pack digital switches into bitfield
  txPacket.switches = 0;
  if (digitalRead(PIN_SW_ARM) == LOW)   txPacket.switches |= (1 << 0); // Armed
  if (digitalRead(PIN_SW_MODE1) == LOW) txPacket.switches |= (1 << 1); // Mode 1
  if (digitalRead(PIN_SW_MODE2) == LOW) txPacket.switches |= (2 << 1); // Mode 2
  if (digitalRead(PIN_BTN_LIGHT) == LOW)txPacket.switches |= (1 << 3); // Light
  if (digitalRead(PIN_BTN_HORN) == LOW) txPacket.switches |= (1 << 4); // Horn

  txPacket.packet_id = frameCounter++;
  txPacket.crc8 = calculateCRC8((uint8_t*)&txPacket, sizeof(ControlPacket) - 1);

  // 2. Transmit via High-Speed NRF24 (50 Hz Loop = Every 20ms)
  if (now - lastNrfTime >= 20) {
    lastNrfTime = now;
    radio.write(&txPacket, sizeof(ControlPacket));
    digitalWrite(PIN_LED_HB, (frameCounter % 50 < 25) ? LOW : HIGH);
  }

  // 3. Transmit via Backup LoRa Link (5 Hz Loop = Every 200ms)
  if (now - lastLoraTime >= 200) {
    lastLoraTime = now;
    Serial1.printf("AT+SEND=0,%d,%02X%02X%02X%02X\r\n", 
                   sizeof(ControlPacket), 
                   txPacket.header, txPacket.throttle, txPacket.steer, txPacket.switches);
  }

  // 4. Update 1.3" OLED HUD (10 Hz Refresh = Every 100ms)
  if (now - lastDisplayTime >= 100) {
    lastDisplayTime = now;
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "CYBERROVER X5 PRO");
    
    // Arm Status
    if (txPacket.switches & 0x01) {
      u8g2.drawStr(0, 24, "SYS: ARMED");
    } else {
      u8g2.drawStr(0, 24, "SYS: DISARMED (SAFE)");
    }

    // Speed Governor Limit & Tilt
    char buf[32];
    snprintf(buf, sizeof(buf), "SPD CAP:%d%% TILT:%d", txPacket.speed_gov, txPacket.cam_tilt);
    u8g2.drawStr(0, 38, buf);

    // Throttle & Steer Live Values
    snprintf(buf, sizeof(buf), "THR:%+3d%% STR:%+3d%%", txPacket.throttle, txPacket.steer);
    u8g2.drawStr(0, 52, buf);

    // Radio Link Indicator
    u8g2.drawStr(0, 64, "RF: NRF-50Hz | LORA-5Hz");
    u8g2.sendBuffer();
  }
}
```
