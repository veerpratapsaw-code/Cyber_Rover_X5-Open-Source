# 🚀 CyberRover X5 — Stable Dongle Teleoperation System v1.0 Release

**Lead Architect & Developer:** Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh, Jharkhand, India  
**System Milestone:** Stable Dongle Teleoperation Release (Zero Jerk, Zero Crawl, Soft-Start Acceleration)  
**Git Tag:** `v1.0-stable-dongle-teleop`  
**Date:** October 2026

---

## 📋 Table of Contents
1. [Release Summary](#-release-summary)
2. [Folder Structure](#-folder-structure)
3. [Master Documentation Files](#-master-documentation-files)
4. [Flashing & Deployment Instructions](#-flashing--deployment-instructions)
5. [PC Cockpit Operation Guide](#-pc-cockpit-operation-guide)
6. [Architectural Breakthroughs & Physics](#-architectural-breakthroughs--physics)

---

## 🎯 Release Summary

This stable release captures the complete, battle-tested, zero-jerk, bidirectional teleoperation stack for **CyberRover X5**. It links a Laptop PC via an ESP32-CAM USB Dongle directly over ESP-NOW (2.4 GHz) to an onboard ESP32-S3 Bridge, which relays high-speed commands to an STM32F103C8T6 Blue Pill Motor Supervisor controlling dual 43A BTS7960 (IBT-2) MOSFET H-bridges and four 12V 600 RPM Johnson gear motors.

### Key Performance Benchmarks
- **End-to-End Latency:** $1.4\text{ to }2.2\text{ ms}$ (Keyboard keystroke to MOSFET gate switch).
- **Control Cadence:** $90\text{ Hz}$ ($11.1\text{ ms}$ stream from PC).
- **Safety Dead-Man Watchdog:** $350\text{ ms}$ timeout on STM32 + $11\text{ ms}$ keystroke release stop on PC.
- **Drivetrain Stability:** Soft-Start Inception at 48 PWM, smooth progressive acceleration curve ($2.2\text{ PWM/cycle}$ ramp over $530\text{ ms}$), instant deceleration response ($18.0\text{ PWM/cycle}$ brake over $70\text{ ms}$).
- **Zero-Crawl Ground Lock:** Timer counter register flush (`analogWrite(pin, 0)` followed by `digitalWrite(pin, LOW)`) guarantees true $0.00\text{V}$ rail clamp with zero MOSFET residual floating.

---

## 📂 Folder Structure

```text
stable_v1_dongle_teleop_release/
├── CYBERROVER_X5_3_MCU_WIRING_GUIDE.md    <- Complete schematic, pinout tables, electrical rules
├── CYBERROVER_X5_DONGLE_SYSTEM_SPECS.txt  <- Full technical specification & packet protocols
├── README.md                              <- This quick-start and deployment guide
│
├── firmware/
│   ├── 01_laptop_dongle_tx/
│   │   └── espnow_laptop_dongle_tx.ino    <- ESP32-CAM USB Dongle transmitter firmware
│   │
│   ├── 02_rover_s3_bridge_rx/
│   │   └── espnow_rx_esp32s3_bridge.ino   <- ESP32-S3 onboard bridge & telemetry reply firmware
│   │
│   └── 03_stm32_motor_supervisor/
│       └── cyberrover_x5_stm32_motor_supervisor.ino <- STM32F103C8T6 dual BTS7960 motor controller
│
└── pc_cockpit/
    ├── pc_gamer_controller.py             <- Windows 90 Hz 1ms multimedia timer teleop cockpit
    └── rover_mac.txt                      <- Cached target ESP32-S3 MAC address
```

---

## 📖 Master Documentation Files

1. [CYBERROVER_X5_3_MCU_WIRING_GUIDE.md](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/stable_v1_dongle_teleop_release/CYBERROVER_X5_3_MCU_WIRING_GUIDE.md):
   - High-level 4-tier communication block diagram.
   - Comprehensive pinout tables for Dongle (ESP32-CAM), Rover Bridge (ESP32-S3), and Motor Supervisor (STM32F103C8T6).
   - Dual BTS7960 (IBT-2) pin connections, heavy-gauge 12V power wiring, and star grounding rules.
   - Deep electrical physics explanation of the **3.3V vs 5.0V IBT-2 buffer threshold**.
2. [CYBERROVER_X5_DONGLE_SYSTEM_SPECS.txt](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/stable_v1_dongle_teleop_release/CYBERROVER_X5_DONGLE_SYSTEM_SPECS.txt):
   - Exhaustive technical manual, packet frame formats (binary serial, ESP-NOW struct, NMEA-style UART telemetry), gear tables, and timing budgets.

---

## ⚡ Flashing & Deployment Instructions

### 1. STM32F103C8T6 Blue Pill Motor Supervisor
- **Source:** [`firmware/03_stm32_motor_supervisor/cyberrover_x5_stm32_motor_supervisor.ino`](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/stable_v1_dongle_teleop_release/firmware/03_stm32_motor_supervisor/cyberrover_x5_stm32_motor_supervisor.ino)
- **Board Package:** STM32 MCU based boards by STMicroelectronics (STM32duino)
- **Board Selection:** `Generic STM32F1 series` -> `BluePill F103C8` (or ST-Link / USB Serial)
- **Settings:** `U(S)ART support: Enabled (generic Serial)`, `USB support: CDC (generic 'Serial' supersede U(S)ART)`
- **Flashing Method:** ST-Link V2 (SWD: SWDIO, SWCLK, GND, 3.3V) or USB-TTL adapter via UART1 (PA9/PA10 with BOOT0 set to 1). Remember to return BOOT0 to 0 after flashing.

### 2. ESP32-S3 Rover Bridge
- **Source:** [`firmware/02_rover_s3_bridge_rx/espnow_rx_esp32s3_bridge.ino`](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/stable_v1_dongle_teleop_release/firmware/02_rover_s3_bridge_rx/espnow_rx_esp32s3_bridge.ino)
- **Board Package:** `esp32` by Espressif Systems (v2.0.x or v3.x)
- **Board Selection:** `ESP32S3 Dev Module`
- **Settings:** `USB CDC On Boot: Enabled`, `Flash Size: 8MB/16MB`, `PSRAM: OPI PSRAM` (or Disabled depending on board variant).
- **Flashing:** Connect USB to native USB port or USB-UART port. Press Boot + Reset if required.

### 3. ESP32-CAM USB Dongle Transmitter
- **Source:** [`firmware/01_laptop_dongle_tx/espnow_laptop_dongle_tx.ino`](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/stable_v1_dongle_teleop_release/firmware/01_laptop_dongle_tx/espnow_laptop_dongle_tx.ino)
- **Board Package:** `esp32` by Espressif Systems
- **Board Selection:** `AI Thinker ESP32-CAM`
- **Flashing:** Connect via USB-to-UART baseboard or FTDI adapter (IO0 jumpered to GND during flash; disconnect IO0 after flash and press Reset).

---

## 🎮 PC Cockpit Operation Guide

1. **Requirements:**
   ```bash
   pip install pyserial
   ```
2. **Launch Cockpit:**
   ```powershell
   python stable_v1_dongle_teleop_release/pc_cockpit/pc_gamer_controller.py
   ```
3. **Controls:**
   - **Movement:** `[W]` Forward, `[S]` Reverse, `[A]` Left Pivot, `[D]` Right Pivot
   - **Curves / Diagonals:** `[W+D]` or `[E]` Front-Right, `[W+A]` or `[Q]` Front-Left, `[S+D]` or `[C]` Back-Right, `[S+A]` or `[Z]` Back-Left
   - **Dead-Man Safety:** Releasing keys instantly commands 0 PWM stop ($11\text{ ms}$ response)
   - **Gears:** `[1]` to `[9]` (Gears 1-9: $3.11\text{ km/h}$ crawl to $11.31\text{ km/h}$ max)
   - **Turbo Boost:** Hold `[SPACEBAR]` for instant 100% full throttle ($255\text{ PWM}$)
   - **Exit:** `[ESC]` safely stops vehicle and terminates script

---

## 🔬 Architectural Breakthroughs & Physics

1. **Elimination of the Low-Speed Runaway Crawl:**
   - *Discovery:* On STM32Duino, `digitalWrite(pin, LOW)` simply alters the GPIO output data register (ODR) but fails to disconnect or reset the hardware timer capture/compare register (`TIM4->CCRx`).
   - *Solution:* The firmware forces an active timer reset using `analogWrite(pin, 0)` before zeroing the GPIO pin. This locks the BTS7960 low-side MOSFET and prevents gate capacitance creep.

2. **Elimination of Motor Jerking & Dead-Man False Triggers:**
   - *Discovery:* Sending verbose ASCII diagnostic strings over STM32 `Serial1` (PA9/PA10) at 100 Hz clogged the hardware TX ring buffer, stalling the STM32 core and causing it to drop incoming bytes on PA10. This starved the 350ms watchdog, causing stuttering stop-and-go jerks.
   - *Solution:* STM32 `Serial1` TX traffic was minimized to a compact 10-byte `$M,L,R\n` sentence transmitted strictly at 10 Hz, leaving 99% of UART bandwidth open for incoming teleoperation commands.

3. **Soft-Start Inception Curve:**
   - *Discovery:* Johnson 555-core motors experience magnetic stiction below ~45 PWM. Stepping from 0 PWM with a large step creates violent whiplash, while small steps from 0 waste hundreds of milliseconds in the deadband.
   - *Solution:* On initial motion transition, the supervisor instantly injects `MIN_START_PWM = 48.0f`, then ramps smoothly at `ACCEL_STEP = 2.2f` over $530\text{ ms}$, producing a refined, realistic throttle curve.
