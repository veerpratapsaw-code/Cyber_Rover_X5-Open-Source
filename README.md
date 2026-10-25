# 🚀 CYBERROVER X5 — HEAVY-DUTY HAZMAT & DISASTER RECONNAISSANCE UGV

![CyberRover X5 & Custom Handheld Remote](user/media/photos/cyberrover_x5_hero.jpg)

**Lead Architect:** **Veer Pratap Saw** (Katras / Dhanbad / Ramgarh, Jharkhand, India)  
**System Status:** **Physical Ground Teleoperation Tested & Verified (v2.0)**  
**Drivetrain:** 4x 12V 600 RPM Johnson Geared Motors (555 Armature) + 100mm Ribbed Wheels  
**Teleop Link:** 50 Hz Zero-Drop ESP-NOW (Channel 1, Smart Direct Unicast, 0ms Modem Wake-Up)  
**Quick Links:** 📖 **[How to Build](user/HOW_TO_MAKE.md)** | 📐 **[Tinkercad 3D Model](https://www.tinkercad.com/things/4SGPQKu9c76-fighter-cyberrover-x5)** | 📱 **[Phone & PC Specs](user/REQUIREMENTS.md)** | 🧾 **[Parts (BOM)](user/BOM.md)** | ⚠️ **[Traps to Avoid](user/WHAT_NOT_TO_DO.md)**

> ### 📐 Interactive 3D CAD Blueprint (Public on Tinkercad)
> Want to see what you are actually building before cutting wood or ordering parts? Inspect every standoff, motor bracket, and deck cutout in interactive 3D:  
> 
> [![CyberRover X5 3D CAD on Tinkercad](user/media/cad_and_designs/fighter_cyberrover_x5_tinkercad_3d.png)](https://www.tinkercad.com/things/4SGPQKu9c76-fighter-cyberrover-x5)  
> 👉 **[Click Here to Open & Rotate the 3D Model Live on Tinkercad (Free in Browser)](https://www.tinkercad.com/things/4SGPQKu9c76-fighter-cyberrover-x5)**

> ### 🛒 Verified Hardware Supplier: 100% Sourced from [Robu.in](https://robu.in)
> You can purchase **all mechanical and electronic items** (Johnson 600 RPM motors, EasyMech brackets, 100mm wheels, BTS7960 drivers, STM32, ESP32, and UBEC step-down modules) directly from **[Robu.in](https://robu.in)** at the exact authentic prices listed in [`user/BOM.md`](user/BOM.md) as Veer personally purchased all components from Robu.in for this build!

---

## 📂 REPOSITORY ARCHITECTURE

```
cyberrover X5/
├── ai/                               <-- DEDICATED FOR AI ASSISTANTS & LLMS
│   ├── AGENTS.md                     <-- Mandatory directives, operating rules & memory mandate
│   ├── AI_CONTEXT_HANDOFF.txt        <-- Continuous engineering memory log (Steps 1 to 4.35)
│   ├── CYBERROVER_MASTER_KNOWLEDGE_BASE.md <-- 868-line comprehensive technical encyclopedia
│   ├── CYBERROVER_X5_REMOTE_CONTROLLER_MASTER_GUIDE.md <-- Complete transmitter blueprint
│   └── README.md                     <-- AI ingestion guide & quickstart
│
├── user/                             <-- DEDICATED FOR HUMAN DEVELOPERS & BUILDERS
│   ├── HOW_TO_MAKE.md                <-- STEP-BY-STEP PRACTICAL BUILD GUIDE
│   ├── REQUIREMENTS.md               <-- PHONE VS PC CODING & WORKSHOP SPECS
│   ├── BOM.md                        <-- LOWEST-COST PARTS LIST & SALVAGE HACKS
│   ├── WHAT_NOT_TO_DO.md             <-- ROOKIE MISTAKES & CHATGPT TRAPS TO AVOID
│   │
│   ├── development/                  <-- ACTIVE DEVELOPMENT & BENCH TEST LAB
│   │   ├── firmware/                 <-- Working firmware (TX, Motor Supervisor, Gateway, Shared)
│   │   ├── hardware_bench_tests/     <-- 10 isolated peripheral diagnostic test sketches
│   │   ├── unit_tests/               <-- Automated host C++ verification suite (289/289 pass)
│   │   └── scratch/                  <-- Development test scripts & display mockups
│   │
│   ├── stable_versions/              <-- PRODUCTION RELEASES WITH COMPLETE WIRING
│   │   ├── v1_dongle_teleop/         <-- Release v1.0: Laptop USB Dongle Teleop + Cockpit
│   │   │   ├── firmware/             <-- Verified snapshots of TX Dongle, S3 Bridge, Motor MCU
│   │   │   ├── pc_cockpit/           <-- Python ground station controller
│   │   │   └── wiring/               <-- 3-MCU pin-to-pin wiring blueprint
│   │   │
│   │   └── v2_handheld_remote_teleop/<-- Release v2.0: Custom Handheld Transmitter Teleop
│   │       ├── firmware/             <-- 01_remote_tx, 02_gateway, 03_rover_bridge, 04_motor_mcu
│   │       ├── wiring/               <-- Handheld TX and Rover chassis wiring matrices
│   │       └── README.md             <-- 3-step flashing instructions
│   │
│   ├── media/                        <-- VISUAL ASSETS & FIELD TEST DOCUMENTATION
│   │   ├── photos/
│   │   │   ├── 01_field_test_oct2026/ <-- Photos of the assembled rover & painted remote
│   │   │   ├── 02_remote_controller_blueprints/ <-- Workshop & CAD transmitter renders
│   │   │   └── 03_historical_prototypes_x1_to_x43/ <-- Historical X1 through X4.3 archive
│   │   ├── videos/                   <-- Field test recordings, drifts, and ramp jumps
│   │   └── cad_and_designs/          <-- 3D concepts and mechanical layouts
│   │
│   └── docs/                         <-- MODULAR TECHNICAL SPECIFICATIONS
│       ├── TECHNICAL_ARCHITECTURE.md
│       ├── WIRING_AND_COMPATIBILITY_CHECKLIST.md
│       ├── FAILSAFE_AND_FAULT_SPECIFICATION.md
│       └── KNOWN_LIMITATIONS_AND_RESIDUAL_RISKS.md
│
├── AGENTS.md                         <-- System entry point for AI IDE rule loaders
└── README.md                         <-- This document
```

---

## ⚡ EMPIRICAL FIELD PERFORMANCE BENCHMARKS (3S 2200 mAh LIPO)

* **Top Sprint Speed:** **$11.88\text{ km/h}$** ($3.30\text{ m/s}$) at 12.6V peak on 100mm wheels.
* **Controlled Acceleration:** Reaches full cruise speed in **$0.26\text{ seconds}$** (`ACCEL_STEP = 8.0f` = 800 PWM/s).
* **Traction & Torque:** Combined motor stall torque of **$49.76\text{ kg-cm}$** ($4.88\text{ N}\cdot\text{m}$) yielding $97.6\text{ N}$ tractive force. Easily breaks traction on concrete for power drifts and spins.
* **Payload Envelope:** Recommended mission payload **$3.0\text{ kg}$ to $4.0\text{ kg}$**; maximum tank-spin payload $\sim 5.5\text{ kg}$; trailer towing $> 20\text{ kg}$.
* **Field Battery Discharge:** In 10 minutes of intense stunt driving (full throttle sprints, ramps, jumps, and drifts), voltage dropped from **12.60V to 11.90V** ($572\text{ mAh}$ consumed at $3.43\text{ A}$ average draw).
  * **Stunt / Extreme Driving Runtime:** **~31.5 minutes total** (21.5 minutes remaining).
  * **Normal Patrol / Reconnaissance Runtime:** **~38 to 45 minutes total**.
  * **Indoor Crawl & Survey Runtime:** **~55 to 65 minutes total**.

---

## 🚀 3-STEP QUICK FLASH GUIDE

1. **Remote Transmitter:**
   * Flash `user/stable_versions/v2_handheld_remote_teleop/firmware/01_remote_tx_stm32` to the STM32 Blue Pill.
   * Flash `user/stable_versions/v2_handheld_remote_teleop/firmware/02_remote_gateway_esp32` to the ESP32 DevKit.
2. **Rover Chassis:**
   * Flash `user/stable_versions/v2_handheld_remote_teleop/firmware/03_rover_s3_bridge` to the Rover ESP32-S3.
   * Flash `user/stable_versions/v2_handheld_remote_teleop/firmware/04_rover_stm32_motor_supervisor` to the Rover STM32 Blue Pill.
3. **Drive:**
   * Power on in any order (Remote first or Car first).
   * Once the remote LED turns solid blue, flip Toggle Left (PB4) to **ARM**!
