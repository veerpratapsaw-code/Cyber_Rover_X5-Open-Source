# 🚀 CYBERROVER MASTER KNOWLEDGE BASE & TECHNICAL ENCYCLOPEDIA
### *The Definitive Repository Archive: Evolution from X1 to X4.3 & Complete Architectural Blueprint for CyberRover X5*

> **Document Classification**: Authoritative Engineering Reference & AI System Knowledge Base  
> **Target Platform**: CyberRover X5 (Autonomous Heavy-Duty Hazmat & Disaster Reconnaissance UGV)  
> **Lead Architect & Developer**: Veer Pratap Saw (Veer), Class 10 Student | Katras / Dhanbad / Ramgarh, Jharkhand, India  
> **Project Heritage**: RBOTICS / Cyber Rover (October 2025 – Present)  
> **Cumulative R&D Effort**: 850+ Hands-on Engineering Hours across 1 Year  
> **Major Accolades**:
> - 🥇 **1st Prize Winner — State Level Science & Technology Exhibition** (Ramgarh, September 2026)
> - 🥇 **1st Prize & Gold Medal — School Science & Robotics Exhibition** (Katras, Dhanbad, August 2026)
> - 🏆 **2nd Prize — BIT Sindri Engineering Exhibition** (April 2026)
> - 🏆 **3rd Prize — BIT Sindri Engineering Exhibition** (June 2026)
> - 🏆 **2nd Prize — School RoboFight Tournament** (January 2026)
>
> **Media Archive Reference**: All 257 authentic historical photos, CAD screenshots, prototype pictures, remote controller internals, and exhibition media are archived in `past_reference_images/`.

---

## 📑 TABLE OF CONTENTS

1. [Developer Profile & Engineering Philosophy](#1-developer-profile--engineering-philosophy)
2. [Complete Chronological Evolution (X1 ➔ X4.3)](#2-complete-chronological-evolution-x1--x43)
   - Phase 0: The Failed 2D Ultrasonic Room Scanner (Oct 2025)
   - Phase 1: CyberRover X1 — 8WD Combat Warrior (Oct 2025 – Jan 2026)
   - Phase 2: CyberRover X2 — Environmental Reconnaissance Pivot (Feb – Apr 2026)
   - Phase 3: CyberRover X3 — YOLO AI Computer Vision & SAPI Voice (May – Jun 2026)
   - The Engineering Impasse: Why the 8WD TT Platform Failed
   - Phase 4: CyberRover X4 — The Heavy-Duty 4WD Redesign (Jul – Aug 2026)
   - Phase 5: CyberRover X4.2 — Open-Source Audit & Gold Medal (Aug – Sep 2026)
   - Phase 6: CyberRover X4.3 — Segregated Dual-Tier Architecture & LoRa (Sep 2026)
3. [CyberRover X5 Upgrade Specifications & Hardware Shortlist](#3-cyberrover-x5-upgrade-specifications--hardware-shortlist)
   - Current State: Completely Disassembled & Ready for X5
   - 3D CAD Analysis (Tinkercad Design Breakdown)
   - Shortlisted Components (Robu.in Parts Deep-Dive)
   - Industrial Explosion-Proof Concept Architecture
4. [Master Pinout Matrices & Electrical Interconnects](#4-master-pinout-matrices--electrical-interconnects)
   - Arduino Nano (Layer 2 Environmental Master)
   - Arduino Uno (Layer 1 Motor & Radar Brain)
   - ESP32-S3 (Rover Master Gateway)
   - ESP32 DevKit (Handheld Remote Controller)
   - ESP32-CAM (Wi-Fi Telemetry & Optical Hub)
5. [Signal Conditioning, Level-Shifting & Power Schematics](#5-signal-conditioning-level-shifting--power-schematics)
   - 5V to 3.3V Logic Level Dividers
   - 3S Battery Voltage Sensing & Calibration
   - Dual-Rail Power Distribution Grid & Star Grounding
6. [Communication Protocols & Network Framing](#6-communication-protocols--network-framing)
   - 2.4 GHz ESP-NOW Peer-to-Peer Link (100 Hz Loop)
   - Binary 9-Byte CyberPacket & CRC-8 CCITT (0x07)
   - UART Command Synchronization Frame (0xAA 0x55 @ 38400)
   - Sub-GHz Long-Range LoRa Telemetry (Reyax RYLR998 @ 868/915 MHz)
   - 17-Field LoRa CSV String Specification
7. [Atmospheric Sensor Suite & Calibration Metrics](#7-atmospheric-sensor-suite--calibration-metrics)
   - MQ-4 Methane / Natural Gas Sensor
   - MQ-7 Toxic Carbon Monoxide Sensor
   - MQ-135 Hazardous Air Quality / VOC Sensor
   - BMP280 High-Precision Barometer & Altimeter
   - DHT11 Digital Climate Probe
   - u-blox NEO-6M High-Gain GPS Module
8. [Mobility, Powertrain & Motor Driver Dynamics](#8-mobility-powertrain--motor-driver-dynamics)
   - Dual BTS7960 43A MOSFET H-Bridge Architecture
   - Motor PWM Scaling, Deadzone & Acceleration Ramping
   - Differential Skid-Steer (Tank Turn) Kinematics
   - Ultrasonic Obstacle Avoidance & Self-Parking Logic
9. [Self-Healing Watchdogs & Failsafe Systems](#9-self-healing-watchdogs--failsafe-systems)
   - 400ms Radio Disconnect Failsafe Brake
   - Ultrasonic Pin Auto-Healing Routine
   - I2C LCD / OLED Memory Bus Watchdog
   - Battery Critical Voltage Cutoff Alarm
10. [Ground Cockpit, DBMS & Software Architecture](#10-ground-cockpit-dbms--software-architecture)
    - Tactical Ground Station Dashboard (HTML5 / Vanilla CSS / JS)
    - SQLite Relational Database Engine (`mission_telemetry.db`)
    - Procedural "RoboEyes" OLED HUD Animation Engine
    - YOLOv5 / YOLOv8 Real-Time AI Inference Pipeline (X3)
    - React 19 + Vite 3D Presentation Web Platform
11. [Exhibition Heritage, Speech Scripts & Judges Viva Defense](#11-exhibition-heritage-speech-scripts--judges-viva-defense)
    - Complete 15-Minute Hinglish Exhibition Presentation Script
    - The 10 Judges Defense Viva Questions & High-Confidence Answers
    - Financial Accounting & Budget Realities (~₹27k–₹30k Cumulative)
12. [CyberRover X5 Implementation & Transition Blueprint](#12-cyberrover-x5-implementation--transition-blueprint)
    - X4.3 ➔ X5 Architectural Evolution Matrix
    - Motor Selection: Johnson 300 RPM vs 600 RPM Analysis
    - Zero-Latency 5.8GHz FPV Optical Reconnaissance Pipeline
    - 2D LiDAR (YDLIDAR X2) 360° Laser SLAM Integration
    - Recommended Step-by-Step Construction Roadmap

---

## 👤 1. DEVELOPER PROFILE & ENGINEERING PHILOSOPHY

### The Creator Behind the Machine
* **Name**: **Veer Pratap Saw** (Veer)
* **Academic Level**: **Class 10 Student** (Secondary School)
* **Location**: Katras, Dhanbad / Ramgarh, Jharkhand, India (the heart of India's underground coal mining belt)
* **Engineering Timeline**: Began robotics journey in **October 2025**. Reached state-level championship recognition by **September 2026** (less than 1 full year).
* **Work Constraints**: Regular school attendance, intensive board exam preparation, limited daily free time (no 14-hour uninterrupted coding sessions).

### Working Style & Capabilities
* **Supervisory System Architect**: Veer manages, designs, and directs code from a top-level architectural perspective rather than hand-typing low-level C++ registers.
* **AI-Assisted Pair Programming**: Veer uses advanced agentic AI (Google DeepMind Antigravity) as an engineering force-multiplier. He directs the architecture, specifies pinouts, identifies bugs, tests hardware, and supervises the codebase.
* **Code Literacy**: Able to read, understand, trace, and debug complex embedded C++, Python, and JavaScript code. Learns rapidly from technical documentation, YouTube engineering channels, and hands-on experimentation.
* **Practical Hardware Builder**: Exceptional hands-on craft in soldering, wire harnessing, chassis fabrication, battery pack assembly, mechanical mounting, and physical failure debugging.

### Core Philosophy
> *"It is much better to sacrifice a machine's hardware than to risk a human rescue worker's life."*

---

## ⏳ 2. COMPLETE CHRONOLOGICAL EVOLUTION (X1 ➔ X4.3)

```
 Oct 2025                 Jan 2026                 Apr 2026                Jun 2026                Aug - Sep 2026
    │                        │                        │                       │                          │
    ▼                        ▼                        ▼                       ▼                          ▼
 [ Phase 0 ] ────────► [ Phase 1: X1 ] ──────► [ Phase 2: X2 ] ──────► [ Phase 3: X3 ] ──────► [ Phase 4/5: X4/X4.2 ] ──► [ Phase 6: X4.3 ]
  4WD TB6612FNG;        8WD Combat Armor;       BIT Sindri 1st Visit;   BIT Sindri 2nd Visit;   4WD 37GB Geared Motors;       Dual-Tier Segregation;
  Ultrasonic 2D scan    Uno + S3 + 2x L298N;    Gas Sensing Pivot;      YOLO AI Object Vision;  Dual 43A BTS7960 Drivers;     Sub-GHz LoRa (RYLR998);
  Failed due to cone    Spiked battle wedge;    Curved Sonar Array;     Windows SAPI Voice;     ESP-NOW 100Hz Remote HUD;     u-blox GPS + 1.3" OLED;
  dispersion.           🏆 2nd Prize RoboFight  🏆 2nd Prize Sindri     🏆 3rd Prize Sindri     🥇 GOLD MEDAL Katras Dhanbad  🥇 1st Prize State Ramgarh
```

### Phase 0: The Failed 2D Ultrasonic Room Scanner (October 2025)
* **The Objective**: Build a compact 4WD robot to map an indoor room on a 2D Cartesian grid using ultrasonic sensors, creating a budget alternative to optical LiDAR.
* **Hardware Stack**:
  - Chassis: Cardboard and plywood base with narrowed triangular nose.
  - Motors: 4x Yellow TT DC gearmotors (plastic gears, 1:48 reduction).
  - Drivers: Dual **TB6612FNG** dual-H-bridge MOSFET drivers.
  - Compute: ESP32 DevKit V1.
  - Power: 3S 18650 Li-ion battery pack with LM2596 buck converter.
* **The Failure**: Ultrasonic acoustic waves emit in a 15° cone. Waves bounce off angled walls, curtains, and furniture at oblique angles, causing phantom echoes and missing boundaries. 2D grid mapping was fundamentally unachievable with acoustic transducers.
* **The Lesson**: Acoustic sensors are suited for coarse proximity radar, not precision spatial mapping. Real mapping requires optical LiDAR or SLAM.

### Phase 1: CyberRover X1 — 8WD Combat Warrior (Oct 2025 – Jan 2026)
* **The Pivot**: School announced a combat robotics competition ("RoboFight") in January 2026.
* **Hardware Upgrades**:
  - Converted chassis from 4WD to an aggressive **8WD** configuration (8 TT motors in parallel) to maximize contact area and tractive force.
  - Installed **dual L298N motor drivers** capable of delivering higher current than the delicate TB6612FNG modules.
  - Integrated an **Arduino Uno** for PWM motor driving paired with an **ESP32-S3**.
  - Built a rugged angled cardboard/plywood battle shell with a low-slung front steel wedge/ramp to flip opponent robots.
  - Power upgrade: 3S Li-ion battery pack protected by a **3S 40A BMS**.
* **Outcome**: 🏆 **Won 2nd Prize** in the school RoboFight tournament. 300+ engineering hours invested.

### Phase 2: CyberRover X2 — Environmental Reconnaissance Pivot (Feb – Apr 2026)
* **Context**: Invited to exhibit robotics at **BIT Sindri** (premier engineering college in Jharkhand) in April 2026. Classmate **Om Ashutosh** joined as co-developer.
* **Mission Shift**: Shifted from combat to search-and-rescue & hazardous underground mine inspection.
* **Innovations**:
  - Installed metal-oxide gas sensors (**MQ-4, MQ-7, MQ-135**) to detect methane and carbon monoxide.
  - Curved frontal array of **3x HC-SR04 ultrasonic sensors** for wide-angle collision detection.
  - Forward-facing **0.96" I2C OLED display** on the front deck for local telemetry.
  - Mounted a smartphone on the rear deck to stream live optical reconnaissance video.
  - **The Wi-Fi Phone Control Disaster**: Attempted driving the car over a smartphone Wi-Fi hotspot. Latency spiked over 800ms with packet buffering, causing the rover to crash into walls. Smartphone Wi-Fi driving was permanently abandoned.
* **Outcome**: 🏆 **Won 2nd Prize** at the BIT Sindri Exhibition. 150+ hours invested.

### Phase 3: CyberRover X3 — YOLO AI Computer Vision & SAPI Voice (May – Jun 2026)
* **Context**: Returned to BIT Sindri in June 2026 for a second competitive science exhibition.
* **Innovations**:
  - **Smartphone Optical Feed**: Realme C11 / Poco M6+ running IP Webcam streamed MJPEG video to a ground station laptop over local Wi-Fi.
  - **YOLO Deep Learning Vision**: Laptop processed frames using **YOLOv5 / YOLOv8** (models: `v8n`, `v8s`, `v5n`, `v5s`, `v8n-seg`) at 320x320 resolution on CPU.
  - **Windows SAPI Voice Synthesis**: Ground laptop dynamically spoke aloud detected objects ("Human presence detected", "Obstacle in path", "Hazard marker identified") through speakers using Microsoft Speech API.
  - **Autonomous Obstacle Avoidance**: Rover used its 3 ultrasonic sensors to autonomously navigate around barriers.
  - Sleek matte-black bodywork with red racing stripes.
* **Outcome**: 🏆 **Won 3rd Prize** at BIT Sindri Exhibition. 100+ hours invested.

### The Engineering Impasse: Why the 8WD TT Platform Had to Be Retired
Thorough field testing revealed three fatal engineering flaws in the X3 platform:
1. **Torque Starvation**: Yellow TT plastic-gear motors stalled whenever the rover encountered rubble, thick carpet, gravel, or steep slopes.
2. **Rigid Suspension Wheel Lift-Off**: On uneven ground, a rigid 8WD chassis caused middle or outer wheels to hang in mid-air, spinning uselessly while traction was lost.
3. **Control Loss**: Pure autonomous sonar wandering without a deterministic manual override made rover recovery dangerous in tight spaces.

### Phase 4: CyberRover X4 — The Heavy-Duty 4WD Redesign (Jul – Aug 2026)
* **Context**: Solo development resumed. Complete clean-slate mechanical and electrical redesign.
* **Major Innovations**:
  - **High-Torque Powertrain**: Swapped 8 TT motors for **4x industrial 37GB 12V geared DC motors** (all-metal planetary gearboxes).
  - **High-Current MOSFET Drivers**: Replaced weak L298N boards with **dual BTS7960 43A H-Bridges** (capable of handling 86A total surge current without heat sinks breaking a sweat).
  - **Handcrafted Handheld Remote Controller**:
    * Ergonomic dual-grip transmitter made from MDF, cardboard, and vinyl wrap.
    * Dual 2-axis analog thumbstick gimbals (throttle & steering).
    * ESP32 DevKit running **Cyber OS** with animated Panther boot logo.
    * 0.96" SSD1306 OLED HUD showing link quality, rover battery, and distances.
    * **ESP-NOW 2.4 GHz Peer-to-Peer Protocol**: 100 Hz refresh loop (10ms interval), <5ms transmission latency, zero reliance on external Wi-Fi routers.
  - **Distributed 5-Microcontroller Topology**:
    * Node 01: ESP32 Handheld Remote Controller
    * Node 02: ESP32-S3 Rover Master (Radio gateway & command dispatcher)
    * Node 03: Arduino Uno (Motor controller, BTS7960 PWM, 3x sonar, piezo siren)
    * Node 04: Arduino Nano (Gas sensors MQ-4, MQ-7, MQ-135, 16x2 I2C LCD)
    * Node 05: ESP32-CAM (Telemetry server, DHT11, BMP280, tactical flashlight)
* **Outcome**: 250+ engineering hours. Rock-solid driving dynamics.

### Phase 5: CyberRover X4.2 — Open-Source Audit & Gold Medal (Aug – Sep 2026)
* **School Exhibition Triumph**:
  - Presented at the Major School Science & Robotics Exhibition in Katras, Dhanbad.
  - Live demonstrations: instantaneous emergency braking, live toxic gas detection, obstacle radar, and zero-latency driving.
  - **Accolade**: 🥇 **Won 1st Prize & the Gold Medal**!
* **Open-Source Hardening**:
  - Full open-source audit: standardized code, created `WIRING.md`, `BOM.md`, `COST_ESTIMATE.md`.
  - Built React 19 + Vite interactive showcase website.
  - Developed tactical ground cockpit web dashboard with 6-channel Bézier real-time oscilloscope.
  - Printed comprehensive 9-panel professional exhibition poster sheet.

### Phase 6: CyberRover X4.3 — Segregated Dual-Tier Architecture & LoRa (Sep 2026)
* **State Exhibition Triumph**:
  - Competed at the **Regional / State-Level Science & Technology Exhibition in Ramgarh**.
  - **Accolade**: 🥇 **Won 1st Prize Winner (State Level)**!
* **Ground-Breaking Upgrades in X4.3**:
  1. **Dual-Tier Electromagnetically Segregated Hierarchy**:
     - *Layer 1 (Bottom Chassis)*: High-current motors, BTS7960 drivers, 3S battery, 5V buck converter, ESP32-S3 rover master, Arduino Uno motor driver.
     - *Layer 2 (Top Environmental Deck)*: Arduino Nano as dedicated Sensor Master, MQ-4, MQ-7, MQ-135, BMP280, DHT11, u-blox NEO-6M GPS, 1.3" OLED HUD, and LoRa radio.
     - Complete physical and electromagnetic isolation eliminated motor back-EMF spikes and ADC jitter.
  2. **Sub-GHz Long-Range LoRa Radio (Reyax RYLR998 @ 868/915 MHz)**:
     - Replaced 2.4 GHz Wi-Fi telemetry with Semtech SX1262-based LoRa.
     - Achieved **1+ kilometer range** capable of penetrating through mine rock, soil, and collapsed rubble where 2.4 GHz Wi-Fi dies within 20 meters.
  3. **Global Positioning System (u-blox NEO-6M GPS)**:
     - Real-time latitude, longitude, satellite count, GPS altitude, speed, and HDOP.
  4. **Procedural RoboEyes & Angled 1.3" OLED Instrument Binnacle**:
     - 1.3" I2C OLED display angled at 35° facing the rear operator.
     - Zero-RAM procedural animated "RoboEyes" boot sequence followed by 10-second cycling telemetry pages.
  5. **Tactical Laptop Ground Cockpit with SQLite DBMS**:
     - Standalone USB-TTL LoRa receiver base station.
     - Local web HUD displaying live artificial horizon (pitch/roll), multi-trace gas oscilloscope, and offline OpenStreetMap tiles.
     - ACID-compliant SQLite relational database (`mission_telemetry.db`) recording all packets with 1-click incident CSV export.

---

## 🔬 3. CYBERROVER X5 UPGRADE SPECIFICATIONS & HARDWARE SHORTLIST

### Current State of the Machine
* The previous rover (X4.3) has been **completely disassembled**.
* All mechanical frames, electronics, and wiring have been deconstructed.
* Veer has designed a new 3D CAD chassis model in Tinkercad and researched professional upgrade parts on Robu.in.
* Hardware selection will be discussed interactively, guided by the specs detailed below.

### 3D CAD Model Breakdown (Tinkercad Design Analysis)
Based on direct visual inspection of the CAD screenshots in the root directory:

```
                          CYBERROVER X5 CAD TOPOLOGY (TOP VIEW)
       ┌─────────────────────────────────────────────────────────────────┐
       │     [=== LEFT WHEEL ===]               [=== RIGHT WHEEL ===]    │
       │              │                                  │               │
       │              ▼                                  ▼               │
       │       ┌──────────────┐                  ┌──────────────┐        │
       │       │ Johnson Mtr  │                  │ Johnson Mtr  │        │
       │       └──────┬───────┘                  └──────┬───────┘        │
       │              │       ┌──────────────────┐      │                │
       │              └──────►│  ANGLED NOSE POD │◄─────┘                │
       │                      │ Camera Tilt Gimbal│                      │
       │                      │  LiDAR / Display │                      │
       │                      └──────────────────┘                      │
       │       ┌──────────────┐                  ┌──────────────┐        │
       │       │ 2x BTS7960   │   [3S 18650 BAY] │ LM2596 Buck  │        │
       │       │ Drivers 43A  │   (Blue Cells)   │ 5V Regulator │        │
       │       └──────────────┘                  └──────────────┘        │
       │              │                                  │               │
       │              ▼                                  ▼               │
       │       ┌──────────────┐                  ┌──────────────┐        │
       │       │ Johnson Mtr  │                  │ Johnson Mtr  │        │
       │       └──────┬───────┘                  └──────┬───────┘        │
       │              │        ┌────────────────┐       │                │
       │              │        │  ESP32-S3 BASE │       │                │
       │              │        │ High-Gain Whip │       │                │
       │              │        └────────────────┘       │                │
       │     [=== LEFT WHEEL ===]               [=== RIGHT WHEEL ===]    │
       └─────────────────────────────────────────────────────────────────┘
```

* **Dual-Deck Standoff Chassis**:
  - Lower Deck: Structural platform mounting 4x Johnson motor L-brackets, wheels, battery bay, and motor drivers.
  - Upper Deck: Protective canopy plate with access cutouts, mounting the pan-tilt camera gimbal and rear radio mast.
  - Structural Standoffs: 6x heavy-duty brass/aluminum standoffs joining top and bottom plates.
* **Front Bumper & Sensor Binnacle**:
  - Forward-angled wedge nose with central aperture for LiDAR/optical sensor and dual headlight cutout slots.
  - Micro-servo camera gimbal mounted on top of the front hood.
* **Central Energy Well**:
  - Longitudinal center battery compartment housing 3x 18650 Li-ion cells in series (3S configuration) or modular LiPo pack.
  - Flanked by high-power electronics: dual green BTS7960 driver blocks and gold step-down buck converter.
* **Rear Radio & Telemetry Deck**:
  - ESP32-S3 module seated on an expansion base board with vertical SMA connector and high-gain whip antenna.

---

### Shortlisted Components (Robu.in Analysis)

| Component Name | Robu.in SKU | Unit Price (INR) | Key Technical Specifications | Architectural Advantage for X5 |
| :--- | :---: | :---: | :--- | :--- |
| **Pro-Range 12V 300 RPM Johnson Geared DC Motor** | **5728** | **₹563.00** | • Base RPM: 18,000<br>• Operating: 6–18V (12V rated)<br>• Rated Torque: 34.2 N-cm (3.48 kg-cm)<br>• Stall Torque: 300 N-cm (30.6 kg-cm)<br>• Gearbox: 25x37 mm<br>• Shaft: 6mm diameter<br>• Grade A, Encoder Compatible | **Exceptional hill climbing and rubble crawling torque.** 300 N-cm stall torque prevents motor stall on steep inclines. |
| **Pro-Range 12V 600 RPM Johnson Geared DC Motor** | **5729** | **₹611.00** | • Base RPM: 18,000<br>• Operating: 6–18V (12V rated)<br>• Rated Torque: 15.1 N-cm (1.54 kg-cm)<br>• Stall Torque: 122 N-cm (12.4 kg-cm)<br>• Gearbox: 22x37 mm<br>• Shaft: 6mm diameter<br>• Grade A, Encoder Compatible | High-speed reconnaissance option. Lower torque than 300 RPM model, but delivers double linear velocity. |
| **EasyMech Advanced Johnson Motor L Clamp** | **33225** | **₹181.00** | • Material: Mild Steel (1.5mm thickness)<br>• Motor Mount Holes: 3mm<br>• Chassis Mount Holes: 4mm<br>• Weight: 34 gm | Heavy-duty steel bracket eliminating motor shaft flexing and chassis vibration under high load. |
| **Robot Wheel 100mm Dia x 44mm Width** | **5884** | **₹149.00** | • Diameter: 100 mm<br>• Width: 44 mm<br>• Shaft Bore: 6 mm (D-shaft)<br>• Load Capacity: 2.0 kg per wheel<br>• Deep-tread rubber tire | 100mm diameter provides **substantial ground clearance (35–45mm)**, rolling easily over rocks, cables, and debris. |
| **Double BTS7960 43A H-Bridge Motor Driver** | **456144** | **₹329.00** | • Input Voltage: 6–27V<br>• Max Current: 43A peak<br>• Logic Levels: 3.3V – 5.0V compatible<br>• Path Resistance: 16 mΩ<br>• PWM Frequency: up to 25 kHz | Virtually indestructible MOSFET H-bridge with built-in thermal and over-current protection. Zero heat issues. |
| **1/3" CMOS 1500TVL Mini FPV Camera (2.1mm)** | **674159** | **₹1,189.00** | • Input Voltage: DC 5V – 30V<br>• Lens: 2.1mm wide angle<br>• Min Illumination: 0.001 Lux / 1.2F (Starlight)<br>• Horizontal Res: 1500 TVL<br>• Signal: PAL / NTSC with OSD | **Zero-latency analog optical reconnaissance.** Ultra-low-light capability sees in dark mine tunnels without bright glare. |
| **5.8G UVC OTG Android AV Phone Receiver (ROTG01)** | **45529** | **₹5,899.00** | • Model: ROTG01<br>• Channels: 150CH auto-scan<br>• Antenna: SMA Female dipole<br>• Interface: USB Type-C / Micro-USB OTG<br>• OS: Android Phone, Windows Laptop (UVC Webcam) | Connects analog 5.8GHz video directly into Laptop Ground Cockpit or Phone with **under 35ms latency**! |
| **YDLIDAR X2 2D LiDAR 360° Scanner (8m)** | **887613** | **₹6,299.00** | • Range: 0.1m – 8.0m<br>• Scan Frequency: 7 Hz (configurable)<br>• Sampling Rate: 3,000 Hz<br>• Interface: 3.3V UART (115200 baud)<br>• Compatibility: ROS / ROS2 / Python | **True 2D laser mapping.** Completes the dream started in Phase 0: real-time room occupancy grid and SLAM! |

---

## ⚡ 4. MASTER PINOUT MATRICES & ELECTRICAL INTERCONNECTS

### 1. Arduino Nano Pinout Matrix (Layer 2 Environmental Master)

```
                       ┌─────────────────────────┐
                       │   ARDUINO NANO PINOUT   │
                       │    (LAYER 2 MASTER)     │
                       └────────────┬────────────┘
      RYLR998 LoRa RX (TTL) ── D0 [RX]   VIN ── 5V Step-Down Buck Rail
 RYLR998 TX (via 1k/2k Div) ── D1 [TX]   GND ── Common Star Ground
      u-blox GPS TX (9600)  ── D2 [RX]   RST ── Reset
              (Reserved TX) ── D3 [TX]   +5V ── Sensor VCC Rail
          DHT11 Data Line   ── D4        A7  ── (Aux Analog)
                (Spare PWM) ── D5        A6  ── (Aux Analog)
                (Spare PWM) ── D6        A5  ── BMP280 + OLED [SCL] (I2C)
                (Spare PWM) ── D7        A4  ── BMP280 + OLED [SDA] (I2C)
                (Spare PWM) ── D8        A3  ── 3S Li-ion Divider (0-25V)
                (Spare PWM) ── D9        A2  ── MQ-135 Air Quality Analog
               (Spare GPIO) ── D10       A1  ── MQ-7 Carbon Monoxide Analog
               (Spare GPIO) ── D11       A0  ── MQ-4 Methane / Gas Analog
               (Spare GPIO) ── D12      AREF ── Default 5.0V Reference
               (Builtin LED)── D13      3V3  ── 3.3V Regulated Out
                       └─────────────────────────┘
```

| Nano Pin | Connected Device | Signal Direction | Logic Level | Function & Notes |
| :--- | :--- | :--- | :--- | :--- |
| **A0** | MQ-4 Gas Sensor | Input (Analog) | 0–5.0V | Methane ($CH_4$) concentration (0..1023 ADC) |
| **A1** | MQ-7 Gas Sensor | Input (Analog) | 0–5.0V | Carbon Monoxide ($CO$) concentration (0..1023 ADC) |
| **A2** | MQ-135 Gas Sensor | Input (Analog) | 0–5.0V | Air Quality, Ammonia, Benzene, Smoke (0..1023 ADC) |
| **A3** | 0–25V Voltage Divider | Input (Analog) | 0–2.52V | 3S Li-ion battery tap ($30k\Omega / 7.5k\Omega$) |
| **A4** | BMP280 + 1.3" OLED | Bidirectional | 3.3V / 5.0V | Shared Hardware I2C SDA (OLED `0x3C`, BMP `0x76`) |
| **A5** | BMP280 + 1.3" OLED | Output (Clock) | 3.3V / 5.0V | Shared Hardware I2C SCL Clock line |
| **D0 (RX)**| Reyax RYLR998 LoRa | Input (Serial) | 3.3V TTL | Hardware Serial RX from LoRa TXD @ 115200 baud |
| **D1 (TX)**| Reyax RYLR998 LoRa | Output (Serial) | **3.33V Divided** | **Requires 1kΩ / 2kΩ divider** to protect LoRa 3.3V RXD |
| **D2 (RX)**| u-blox NEO-6M GPS | Input (Serial) | 3.3V TTL | SoftwareSerial RX from GPS TXD @ 9600 baud |
| **D3 (TX)**| u-blox NEO-6M GPS | Output (Serial) | — | Unconnected / Reserved |
| **D4** | DHT11 Climate Sensor | Bidirectional | 5.0V Digital | Single-bus digital humidity and temperature stream |

---

### 2. Arduino Uno Pinout Matrix (Layer 1 Motor & Sonar Brain)

```
                       ┌─────────────────────────┐
                       │    ARDUINO UNO PINOUT   │
                       │    (LAYER 1 MOTOR MCU)  │
                       └────────────┬────────────┘
                            ── D0 [RX]   VIN ── (Unused)
                            ── D1 [TX]   5V  ── 5.0V Regulated Rail
  ESP32-S3 UART Command Link── D2 [RX]   GND ── Common Star Ground
               (Spare GPIO) ── D3        GND ── Common Star Ground
               (Spare GPIO) ── D4        A0  ── Right US Echo (Analog In)
       Left Motor RPWM (Fwd)── D5 [PWM]  A1  ── Right US Trig (Analog Out)
       Left Motor LPWM (Rev)── D6 [PWM]  A2  ── Center US Echo (Analog In)
               (Spare GPIO) ── D7        A3  ── Center US Trig (Analog Out)
       Tactical Piezo Buzzer── D8        A4  ── Left US Echo (Analog In)
      Right Motor RPWM (Fwd)── D9 [PWM]  A5  ── Left US Trig (Analog Out)
      Right Motor LPWM (Rev)── D10[PWM]  A6  ── (Unused)
               (Spare GPIO) ── D11       A7  ── (Unused)
               (Spare GPIO) ── D12      AREF ── Default 5.0V
               (Builtin LED)── D13      3V3  ── (Unused)
                       └─────────────────────────┘
```

| Uno Pin | Peripheral Interconnect | Function Description |
| :--- | :--- | :--- |
| **Pin D2 (RX)** | ESP32-S3 GPIO 17 (TX) | One-way serial command stream (`0xAA 0x55` header @ 38400 baud) |
| **Pin D5 (PWM)**| Left BTS7960 `L_RPWM` | Left Motor Forward Drive PWM (0–255) |
| **Pin D6 (PWM)**| Left BTS7960 `L_LPWM` | Left Motor Reverse Drive PWM (0–255) |
| **Pin D8** | Piezo Buzzer Positive | Audible tactical alert / hazard alarm signal |
| **Pin D9 (PWM)**| Right BTS7960 `R_RPWM`| Right Motor Forward Drive PWM (0–255) |
| **Pin D10 (PWM)**| Right BTS7960 `R_LPWM`| Right Motor Reverse Drive PWM (0–255) |
| **Pin A0 / A1** | Right HC-SR04 | Pin A0 = Echo (Input), Pin A1 = Trig (Output) |
| **Pin A2 / A3** | Center HC-SR04 | Pin A2 = Echo (Input), Pin A3 = Trig (Output) |
| **Pin A4 / A5** | Left HC-SR04 | Pin A4 = Echo (Input), Pin A5 = Trig (Output) |

---

### 3. ESP32-S3 Pinout Matrix (Rover Master & RF Gateway)

| ESP32-S3 Pin | Destination | Role & Function |
| :--- | :--- | :--- |
| **GPIO 17 (TX1)** | Arduino Uno Pin D2 (RX) | Transmits decoded 9-byte `CyberPacket` to Uno @ 38400 baud |
| **GPIO 18 (RX1)** | Unconnected | Reserved / Return line removed to prevent ground-loop cross talk |
| **GPIO 48** | Built-in WS2812 RGB LED | Multi-color status indicator (Green=Connected, Blue=Searching, Red=Failsafe) |
| **5V / VBUS** | 5.0V Step-Down Rail | Primary compute power input |
| **3.3V Output** | 3.3V Secondary Rail | Clean onboard LDO output powering LoRa, GPS, and sensors |
| **GND** | Star Ground Rail | System-wide reference ground |

---

### 4. ESP32 DevKit Handheld Remote Controller Pinout

| ESP32 Pin | Controller Component | Hardware Configuration & Notes |
| :--- | :--- | :--- |
| **GPIO 35** | Left Joystick X-Axis | ADC1_CH7 (Analog Input with 8x oversampling) |
| **GPIO 34** | Left Joystick Y-Axis | ADC1_CH6 (Analog Input - Throttle Forward/Reverse) |
| **GPIO 25** | Left Joystick Pushbutton | Digital Input (Re-calibration trigger) |
| **GPIO 33** | Right Joystick X-Axis | ADC1_CH5 (Analog Input - Steering Left/Right) |
| **GPIO 32** | Right Joystick Y-Axis | ADC1_CH4 (Analog Input) |
| **GPIO 26** | Right Joystick Pushbutton | Digital Input (Instant Emergency Stop / Brake) |
| **GPIO 21** | 0.96" OLED SDA | I2C Data line (SSD1306 128x64 display) |
| **GPIO 22** | 0.96" OLED SCL | I2C Clock line |
| **GPIO 12** | Push Button 1 | Momentary tactical tactile switch |
| **GPIO 13** | Push Button 2 | Momentary tactical tactile switch |
| **GPIO 27** | Toggle Switch 1 | Mode selection (Armed / Parking Mode) |
| **GPIO 14** | Toggle Switch 2 | Mute / Acoustic siren enable toggle |
| **GPIO 4** | Active Buzzer | Audible click feedback and low-battery alarm |
| **GPIO 36 (VP)**| 1S Li-ion Battery Tap | ADC1_CH0 via 5:1 voltage divider (Calib trim factor: 1.0398f) |

---

### 5. ESP32-CAM Pinout Matrix (Telemetry Hub & Video in X4/X4.2)

| ESP32-CAM Pin | Connected Peripheral | Function & Level Shifting |
| :--- | :--- | :--- |
| **GPIO 16 (RX)** | Nano D1 (TX) | Gas telemetry serial input via 220Ω / 440Ω resistor divider |
| **GPIO 13** | BMP280 SDA | Barometer & altitude I2C data |
| **GPIO 15** | BMP280 SCL | Barometer & altitude I2C clock |
| **GPIO 14** | DHT11 Data Pin | Climate 1-wire digital data |
| **GPIO 4** | Flashlight Torch LED | Onboard high-power white LED (Ground Cockpit controllable) |
| **GPIO 33** | Red Status LED | Active-low link pulse LED |

---

## ⚡ 5. SIGNAL CONDITIONING, LEVEL-SHIFTING & POWER SCHEMATICS

### 1. Arduino Nano D1 (TX) ➔ RYLR998 RXD (5V to 3.3V Divider)
The Arduino Nano ATmega328P drives digital pins at 5.0V. The Semtech SX1262 engine inside the Reyax RYLR998 module has an absolute maximum input tolerance of 3.6V. Direct connection will permanently damage the transceiver.

```
Nano Pin D1 (5.0V TX) ────[ 1.0 kΩ Resistor ]────┬────> RYLR998 RXD (3.33V)
                                                 │
                                         [ 2.0 kΩ Resistor ]
                                                 │
Nano GND ────────────────────────────────────────┴────> Common GND
```

$$\mathbf{V_{out}} = 5.0\,	ext{V} 	imes \left(rac{2000\,\Omega}{1000\,\Omega + 2000\,\Omega}ight) = \mathbf{3.33\,	ext{Volts (100\% Safe)}}$$

---

### 2. Arduino Nano D1 (TX) ➔ ESP32-CAM GPIO 16 (220Ω / 440Ω Divider)
Used in CyberRover X4/X4.2 to route gas telemetry from Nano to ESP32-CAM:

```
Nano Pin D1 (5.0V TX) ────[ 220 Ω Resistor ]────┬────> ESP32-CAM GPIO 16 (RX)
                                                │
                                        [ 440 Ω Resistor ] (2x 220Ω in series)
                                                │
Nano GND ───────────────────────────────────────┴────> ESP32-CAM GND
```

$$\mathbf{V_{out}} = 5.0\,	ext{V} 	imes \left(rac{440\,\Omega}{220\,\Omega + 440\,\Omega}ight) = \mathbf{3.33\,	ext{Volts}}$$

---

### 3. Precision 3S Battery Voltage Divider (Nano Pin A3)
The 3S 18650 battery pack ranges from **9.60V (discharged)** to **12.60V (fully charged)**. The Nano ADC reference voltage is 5.00V. A 5:1 ratio module ($30	ext{k}\Omega / 7.5	ext{k}\Omega$) scales this voltage down safely:

```
3S Battery (+) [9.6V - 12.6V] ────[ 30.0 kΩ ]────┬────> Nano Pin A3 (Analog In)
                                                 │
                                          [ 7.5 kΩ ]
                                                 │
3S Battery (-) / GND ────────────────────────────┴────> Common Star Ground
```

$$\mathbf{V_{A3}} = rac{V_{	ext{battery}}}{5.0} = rac{12.60\,	ext{V}}{5.0} = \mathbf{2.52\,	ext{V (Safely below 5.0V ADC limit)}}$$

#### Calibration Trim Algorithm:
Because standard 5% carbon resistors and USB buck voltages vary, a calibrated software trim factor was calculated empirically against a laboratory digital multimeter:
- Uncalibrated reading: 14.20V
- Multimeter reference: 12.10V
- $	ext{Trim Factor} = rac{12.10}{14.20} pprox \mathbf{0.852f}$

```cpp
float rawVoltage = (analogRead(PIN_BATTERY) * 5.0f / 1023.0f) * 5.0f;
float batteryVoltage = rawVoltage * 0.852f; // Calibrated true pack voltage
```

---

### 4. Dual-Rail Power Distribution Grid & Star Grounding Topology

```
                         3S LITHIUM-ION BATTERY PACK
                         (Nominal: 11.1V | Peak: 12.6V)
                                       │
                ┌──────────────────────┴──────────────────────┐
                │                                             │
      [Heavy 16 AWG Leads]                          [Heavy 18 AWG Leads]
                │                                             │
                ▼                                             ▼
    DUAL BTS7960 43A H-BRIDGES                       HIGH-CURRENT 5V BUCK
    (Motor Power B+ / B-)                            (LM2596 / XL4015 3A)
                │                                             │
                │                                     +5.0V REGULATED BUS
                │                                             │
                │                                ├── Arduino Nano (5V Pin)
                │                                ├── Arduino Uno (5V Pin)
                │                                ├── MQ Heaters (3x 150mA = 450mA)
                │                                ├── DHT11 Climate Sensor
                │                                └── ESP32-S3 (5V Pin)
                │                                        │
                │                                        ▼ (S3 Onboard LDO)
                │                                   +3.3V LOGIC & RF BUS
                │                                        │
                │                                ├── Reyax RYLR998 LoRa TX
                │                                ├── u-blox NEO-6M GPS
                │                                ├── BMP280 Barometer
                │                                └── 1.3" I2C OLED Display
                │                                        │
                ▼                                        ▼
    ───────────────────────────────────────────────────────────
              UNIFIED COMMON STAR GROUND BUSBAR (GND)
    ───────────────────────────────────────────────────────────
```

> [!CRITICAL]
> **ELECTRICAL GOLDEN RULE**:
> Never power the RYLR998 LoRa transceiver or NEO-6M GPS from the Arduino Nano's onboard `3.3V` pin! The Nano's internal 3.3V regulator supplies only 30–50mA. Peak LoRa transmission bursts draw ~120mA, which instantly causes the Nano's MCU to brown out and reset. Always source 3.3V from the ESP32-S3's robust onboard 500mA LDO regulator.

---

## 📡 6. COMMUNICATION PROTOCOLS & NETWORK FRAMING

### 1. 2.4 GHz ESP-NOW Peer-to-Peer Teleoperation Link
* **Topology**: Direct MAC-addressed peer-to-peer (no router, no SSID, no DHCP handshake).
* **Target Receiver MAC**: `{0x1C, 0xDB, 0xD4, 0x4B, 0x08, 0x40}` (ESP32-S3 on rover).
* **Polling Rate**: **100 Hz** (10 milliseconds non-blocking timer).
* **Latency**: $<5\,	ext{ms}$ over the air; total loop latency $<30\,	ext{ms}$.
* **Failsafe Timeout**: 400 milliseconds (automatic motor cutoff if packets stop).

---

### 2. Binary 9-Byte `CyberPacket` Structure
To maximize transmission speed and eliminate JSON parsing overhead on 8-bit microcontrollers, all remote controls are packed into an ultra-dense **9-byte binary struct**:

```cpp
#pragma pack(push, 1)

struct CyberPacket {
    uint8_t seq;          // Rolling packet sequence counter (0..255)
    int8_t  leftX;        // Left Stick Horizontal (-100 .. +100)
    int8_t  leftY;        // Left Stick Vertical / Throttle (-100 .. +100)
    int8_t  rightX;       // Right Stick Horizontal / Steering (-100 .. +100)
    int8_t  rightY;       // Right Stick Vertical (-100 .. +100)
    uint8_t buttons;      // Packed bitmask for all pushbuttons and toggles
    uint8_t mode;         // 0 = MANUAL, 1 = SEMI_AUTO, 2 = AUTO, 3 = PARK
    uint8_t batPct;       // Remote Controller Battery (0 .. 100%)
    uint8_t crc8;         // Hardware-verified CRC-8 CCITT checksum
};

#pragma pack(pop)
```

#### Button Bitmask Mapping:
* `Bit 0 (0x01)`: Physical Push Button 1
* `Bit 1 (0x02)`: Physical Push Button 2
* `Bit 2 (0x04)`: Toggle Switch 1 (Armed / Park Mode)
* `Bit 3 (0x08)`: Toggle Switch 2 (Mute / Sound Engine)
* `Bit 4 (0x10)`: Left Joystick Center Click (Re-calibrate axes)
* `Bit 5 (0x20)`: Right Joystick Center Click (Instant Emergency Brake)

---

### 3. Fast CRC-8 CCITT (Polynomial 0x07) Algorithm
```cpp
inline uint8_t computeCRC8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

---

### 4. UART Command Synchronization Frame (ESP32-S3 ➔ Arduino Uno)
The ESP32-S3 forwards commands to the Uno over Hardware UART at **38400 baud** using a 2-byte sync header:

```
[ 0xAA ] [ 0x55 ] [ 9-Byte CyberPacket Payload ] [ 1-Byte CRC8 ]
```
The Uno's circular serial buffer looks for the sequence `0xAA 0x55`. If found, it reads the next 10 bytes and validates the CRC. If corrupted, it discards the frame without acting on erroneous motor speeds.

---

### 5. Sub-GHz LoRa Telemetry Protocol (Reyax RYLR998 @ 868/915 MHz)
Layer 2 broadcasts a structured CSV telemetry string every **1.5 to 2.0 seconds**:

```
CR43,<pkt_id>,<batt_v>,<mq4_adc>,<mq7_adc>,<mq135_adc>,<dht_temp>,<dht_hum>,<bmp_temp>,<bmp_press>,<bmp_alt>,<gps_fix>,<lat>,<lon>,<sats>,<hdop>,<gps_alt>,<speed>
```

#### 17-Field Definitions:
1. `CR43`: Header tag validating protocol version compatibility.
2. `pkt_id`: Monotonically increasing 32-bit integer counter.
3. `batt_v`: Calibrated battery pack voltage (e.g. `12.14`).
4. `mq4_adc`: Raw 10-bit ADC count for methane gas (0–1023).
5. `mq7_adc`: Raw 10-bit ADC count for carbon monoxide (0–1023).
6. `mq135_adc`: Raw 10-bit ADC count for toxic VOCs / air quality (0–1023).
7. `dht_temp`: Ambient temperature from DHT11 in °C.
8. `dht_hum`: Ambient relative humidity from DHT11 in %.
9. `bmp_temp`: High-precision barometric temperature from BMP280 in °C.
10. `bmp_press`: Atmospheric pressure in Pascals (e.g. `100845`).
11. `bmp_alt`: Calculated barometric altitude in meters above sea level.
12. `gps_fix`: Binary GPS fix status (`1` = valid 3D fix, `0` = searching).
13. `lat`: Latitude in WGS84 decimal degrees (e.g. `23.80412`).
14. `lon`: Longitude in WGS84 decimal degrees (e.g. `86.30219`).
15. `sats`: Number of visible GPS satellites (e.g. `8`).
16. `hdop`: Horizontal Dilution of Precision metric (e.g. `1.20`).
17. `speed`: Rover ground speed in km/h from Doppler triangulation.

---

## 🧪 7. ATMOSPHERIC SENSOR SUITE & CALIBRATION METRICS

| Sensor Module | Transducer Mechanism | Target Gases / Physical Metrics | Dynamic Range | Hardware Pin | Danger Threshold |
| :--- | :--- | :--- | :--- | :---: | :---: |
| **MQ-4** | Catalytic $SnO_2$ Metal Oxide | Methane ($CH_4$), Natural Gas, Fire-Damp | 300–10,000 ppm | Nano `A0` | $\ge 650	ext{ ADC}$ |
| **MQ-7** | Dual-Cycle Heating Core ($1.4	ext{V} / 5.0	ext{V}$) | Toxic Carbon Monoxide ($CO$) | 20–2,000 ppm | Nano `A1` | $\ge 750	ext{ ADC}$ |
| **MQ-135** | Wide-Spectrum Metal Oxide | Ammonia ($NH_3$), Benzene, Smoke, VOCs | 10–1,000 ppm | Nano `A2` | $\ge 600	ext{ ADC}$ |
| **BMP280** | Piezo-Resistive MEMS Barometer | Atmospheric Pressure & Altimetry | 300–1100 hPa | I2C (`A4/A5`) | Sudden Pressure Drop |
| **DHT11** | Resistive Polymer & NTC Bead | Ambient Temperature & Relative Humidity | 0–50°C / 20–90% RH | Nano `D4` | Heat Index $> 45^\circ	ext{C}$ |
| **NEO-6M** | 50-Channel GPS Engine | Geospatial Coordinates, Satellites, Speed | Worldwide L1 | Nano `D2` | Fix Loss Alarm |
| **Battery Divider**| $30	ext{k}\Omega / 7.5	ext{k}\Omega$ Precision Resistors | 3S Li-ion Real-Time Voltage | 0–25.0V DC | Nano `A3` | $< 9.60	ext{V}$ (Cutoff) |

---

## 🏎️ 8. MOBILITY, POWERTRAIN & MOTOR DRIVER DYNAMICS

### Dual BTS7960 43A MOSFET H-Bridge Architecture
* **Total Drive Capacity**: 2x 43A = **86A continuous capability**, easily managing the 12V Johnson motors under full stall.
* **Control Lines**:
  - `L_RPWM` (Pin 5) & `L_LPWM` (Pin 6): Left-side drive motors.
  - `R_RPWM` (Pin 9) & `R_LPWM` (Pin 10): Right-side drive motors.
  - `L_EN` & `R_EN`: All enable inputs tied permanently to the 5.0V logic rail.

### Software Speed Curves & Deadzone Handling
```cpp
const int MAX_PWM          = 255;
const int MIN_DRIVE_PWM    = 100; // Overcomes initial static friction
const int MIN_TURN_PWM     = 160; // Extra torque for skid steering
const int JOY_DEADZONE     = 6;   // Filters out mechanical stick drift
```

### Motor Acceleration Ramping & Direction Dead-Time
To prevent destructive back-EMF spikes and gear stripping when transitioning from full forward to full reverse, the motor driver enforces dynamic ramping:
* Acceleration Increment: `ACCEL_STEP = 12` per loop tick.
* Deceleration Increment: `DECEL_STEP = 18` per loop tick.
* Direction Change Dead-Time: **30 milliseconds** enforced neutral pause (`PWM = 0`) before reversing polarity.

### Autonomous Obstacle Radar Matrix
The Arduino Uno continuously samples 3x ultrasonic sensors:
* **Critical Danger Zone ($<20\,	ext{cm}$)**: Emergency halt, rapid reverse (`PWM = 175`), and 90° tank spin away from obstacle.
* **Warning Zone ($20\,	ext{cm} - 45\,	ext{cm}$)**: Slows speed, executes pivot turn toward whichever side has greater clearance.
* **Corridor Steering Assist**: Proportional speed biasing between left and right wheels to keep rover centered in narrow passages.

---

## 🛡️ 9. SELF-HEALING WATCHDOGS & FAILSAFE SYSTEMS

1. **400ms Radio Failsafe Watchdog**:
   - If the Arduino Uno does not receive a valid CRC-verified `CyberPacket` within 400 milliseconds, it automatically drives all motor PWM pins to `0`. The rover stops dead in its tracks.
2. **Ultrasonic Pin Auto-Healing**:
   - In student workshops, trigger and echo jumper wires are frequently swapped. The firmware detects zero-pulse returns and automatically swaps pin assignments in software to restore radar vision.
3. **I2C Memory Bus Watchdog (LCD / OLED Auto-Recovery)**:
   - High motor currents produce electromagnetic radiation that can freeze I2C display driver registers (`SSD1306` or `PCF8574`).
   - The Nano runs a background timer resetting and re-initializing the display every 6.0 seconds without rebooting sensor acquisition.
4. **Low-Battery Auto-Preservation**:
   - When the 3S battery drops below **9.60V (3.20V / cell)**, the rover sounds a continuous acoustic buzzer alarm and flashes HUD warnings to prevent permanent cell degradation.

---

## 🖥️ 10. GROUND COCKPIT, DBMS & SOFTWARE ARCHITECTURE

### 1. Tactical Ground Station Dashboard (`software/ground-dashboard/`)
* Built with zero-dependency native web technologies: **HTML5, Vanilla CSS, and modern JavaScript**.
* **Live Environmental Oscilloscope**: 6-channel Bézier canvas graph rendering real-time curves for Methane, CO, VOCs, Temperature, Humidity, and Pressure.
* **3D Inclinometer & Artificial Horizon**: Live pitch and roll attitude indicators alerting the pilot when traversing slopes exceeding 35°.
* **Offline GPS Cartography**: Loads pre-cached OpenStreetMap tiles from the local `tiles/` directory, providing geospatial tracking in underground or wilderness zones lacking internet connectivity.
* **1-Click Emergency Incident Export**: Instantly dumps all mission telemetry into formatted CSV files for rescue coordinators.

---

### 2. ACID-Compliant SQLite Telemetry Database Schema (`mission_telemetry.db`)
Every packet received by the ground station USB-TTL receiver is committed to SQLite:

```sql
CREATE TABLE IF NOT EXISTS telemetry_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    packet_id INTEGER,
    battery_v REAL,
    mq4_raw INTEGER,
    mq7_raw INTEGER,
    mq135_raw INTEGER,
    dht_temp REAL,
    dht_hum REAL,
    bmp_temp REAL,
    bmp_press REAL,
    bmp_alt REAL,
    gps_fix INTEGER,
    latitude REAL,
    longitude REAL,
    satellites INTEGER,
    hdop REAL,
    speed_kmh REAL
);
```

---

### 3. Procedural "RoboEyes" OLED HUD Engine
The 1.3" OLED on Layer 2 does not use static text. It executes procedural mathematical rendering:
* **Idle State**: Generates organic blinking, eye widening, and squinting using trigonometric parametric curves (`sin()` and `cos()`).
* **Telemetry Flip Mode**: Automatically cycles between 3 comprehensive diagnostic pages every 10 seconds:
  - *Page 1 (Atmosphere)*: Gas ADC counts (MQ4, MQ7, MQ135) with graphical warning bars.
  - *Page 2 (Microclimate)*: BMP280 Altitude, Pressure in hPa, DHT11 Temperature, and Humidity.
  - *Page 3 (Navigation & Power)*: GPS Fix status, Lat/Lon coordinates, Sat count, and 3S Battery Voltage with fill gauge.

---

### 4. YOLO AI Computer Vision Pipeline (Phase 3 Python Stack)
* **Model Choices**: YOLOv8n, YOLOv8s, YOLOv5n, YOLOv5s, YOLOv8n-seg.
* **Input Feed**: RTSP / HTTP MJPEG video stream from mobile phone IP Webcam (`imgsz=320`).
* **Text-to-Speech Engine**: `win32com.client.Dispatch('SAPI.SpVoice')` on Windows.
* **Acoustic Hazard Narrator**: Evaluates detected classes against `PRIORITY_LABELS` and synthesizes spoken warnings in real time.

---

## 🎤 11. EXHIBITION HERITAGE, SPEECH SCRIPTS & JUDGES VIVA DEFENSE

### Complete 15-Minute Exhibition Script (Hinglish)
*(As delivered by Veer at the State-Level Science Exhibition in Ramgarh)*

#### Part 1: The Opening & Hook (1.5 min)
> *"Namaskar! Respected judges, teachers, aur mere saathiyon... Maan lijiye aap ek rescue team ke leader ho. Ramgarh ke kisi underground coal mine mein suddenly methane gas ka leak ho gaya hai. Andar jaana matlab — **seedha death trap**. Na aapko pata ki andar kitni gas hai, na pata ki structure collapse hone wala hai ya nahi. Aap kya karoge? Insaan ko bhejoge? **NAHI!** Aap **YE** bhejoge — **CyberRover X4.3**."*

#### Part 2: The Core Problem in Jharkhand (1.5 min)
> *"Hum Jharkhand ke coal mining belt mein rehte hain — Ramgarh, Dhanbad, Bokaro, Hazaribagh. Har saal coal mine accidents mein log marte hain — mostly methane gas explosions aur toxic carbon monoxide asphyxiation ki wajah se. Jab leak hota hai, toh canary method se insaano ko andar bhejte hain jo lethal hai. CyberRover Xisi lethal threat ko eliminate karta hai."*

#### Part 3: Segregated Architecture Defense (2.0 min)
> *"Zyaadatar student projects mein ek hi Arduino pe motors, sensors, aur display lage hote hain. Jab motors rotate karte hain, toh 20–40A current draw se EMI paida hota hai jo sensitive gas readings corrupt kar deta hai. Hamne banaya **Segregated Dual-Tier Architecture**: Layer 1 pe motors aur BTS7960 drivers hain; Layer 2 pe Arduino Nano aur 7 atmospheric sensors hain. Dono layers electromagnetically isolated hain."*

#### Part 4: Long-Range LoRa Radio Innovation (1.5 min)
> *"Normal Bluetooth ya Wi-Fi signals mine ke underground rock aur rubble ke andar 15-20 meter mein choke ho jaate hain. Isliye hamne implement kiya **Reyax RYLR998 Sub-GHz LoRa Radio** jo 868/915 MHz frequency pe operate karta hai aur **1 kilometre se zyada** distance tak thick concrete aur soil ko penetrate karke laptop dashboard pe continuous data bhejta hai."*

---

### The 10 Judges Defense Viva Questions & High-Confidence Answers

#### Q1: "Why use multiple microcontrollers instead of one powerful board?"
> **Defense**: *"Three practical reasons: First, separation of driving from telemetry. If the web server or video link experiences packet buffering, the motor driving system never freezes because it runs on an offline radio link. Second, electromagnetic isolation. High-current motor spikes never touch sensitive gas sensor ADC lines. Third, modular serviceability in the field."*

#### Q2: "What is CRC-8 CCITT (Polynomial 0x07), and why do you use it?"
> **Defense**: *"It is a fast, 1-byte mathematical check byte calculated across all joystick and button values. If radio interference alters even a single bit in the air, the rover calculates a mismatch and discards the corrupted packet within 2 microseconds, ensuring the vehicle never moves erratically."*

#### Q3: "How is your transmission rate 100 Hz, and how did you verify it?"
> **Defense**: *"Our binary packet is ultra-compact — only 9 bytes. Transmitting 9 bytes over ESP-NOW takes less than 500 microseconds. In our firmware loop, a non-blocking `millis()` timer fires every 10 milliseconds. Over Serial Monitor diagnostics, loop execution completes in ~250 microseconds and logs 100 packets every 1000 milliseconds consistently."*

#### Q4: "What happens if radio connection is lost?"
> **Defense**: *"If signal drops for more than 400 milliseconds, the S3 LED switches to solid red failsafe, and the Uno's communication watchdog immediately sets all motor PWM signals to 0, locking the rover safely in place."*

#### Q5: "What is the real cost difference between this and commercial units?"
> **Defense**: *"Commercial hazmat UGVs (FLIR PackBot, Foster-Miller TALON) cost between ₹10 Lakh to ₹50 Lakhs. CyberRover X4.3 achieves 85% of critical hazmat reconnaissance for approximately ₹20,000 INR — representing over 98% cost savings."*

---

## 🚀 12. CYBERROVER X5 IMPLEMENTATION & TRANSITION BLUEPRINT

### X4.3 ➔ X5 Architectural Evolution Matrix

| Feature / Subsystem | CyberRover X4.3 (Baseline) | CyberRover X5 (Next-Gen Platform) | Engineering Advantage in X5 |
| :--- | :--- | :--- | :--- |
| **Mobility Actuation** | 4x 37GB Geared Motors (12V) | **4x Pro-Range 12V Johnson Geared Motors** | **10x higher stall torque (300 N-cm)**, all-metal helical gearboxes |
| **Motor Mounting** | Standard aluminum brackets | **1.5mm Mild Steel EasyMech L-Clamps (33225)** | Eliminates motor shaft flexing and gear deformation |
| **Wheel Ground Clearance** | 85mm rubber tires | **100mm Diameter x 44mm Width Wheels (5884)** | **Higher ground clearance (45mm)**, rolls over rubble effortlessly |
| **Optical Reconnaissance** | Smartphone IP Webcam (Wi-Fi) | **1500TVL Mini FPV Camera + 5.8G ROTG01 UVC** | **Sub-35ms latency**, works in near total darkness (0.001 Lux) |
| **Spatial Mapping** | 3x Ultrasonic Sensors (Acoustic) | **YDLIDAR X2 2D 360° Laser LiDAR (887613)** | **Millimeter-precise 360° 2D laser SLAM mapping up to 8 meters** |
| **Motor Driver** | Dual BTS7960 43A H-Bridges | **Dual BTS7960 43A H-Bridges (Preserved)** | Proven rock-solid performance with 86A surge capacity |
| **Chassis Structure** | Layered MDF & Acrylic plates | **Dual-Deck Standoff Chassis with Angled Nose** | High impact resistance, integrated camera tilt gimbal, modular bays |
| **Primary Telemetry** | Reyax RYLR998 Sub-GHz LoRa | **Hybrid LoRa + High-Gain ESP32-S3 Antenna** | Combined long-range atmospheric telemetry with fast teleoperation |

---

### Motor Selection Analysis: Johnson 300 RPM vs 600 RPM

#### Option A: Johnson 12V 300 RPM (SKU 5728) — **RECOMMENDED FOR HAZMAT RECON**
* Rated Torque: **34.2 N-cm (3.48 kg-cm)**
* Stall Torque: **300 N-cm (30.6 kg-cm)**
* Linear Speed with 100mm Wheels:
  $$	ext{Circumference} = \pi 	imes 0.10\,	ext{m} = 0.314\,	ext{m}$$
  $$	ext{Speed} = 300\,	ext{RPM} 	imes 0.314\,	ext{m/min} = 94.2\,	ext{m/min} pprox \mathbf{1.57\,	ext{m/s} \quad (5.65\,	ext{km/h})}$$
* **Verdict**: Ideal tactical speed. Huge climbing torque ensures the rover never stalls when crawling through mud, steep rubble, or over water obstacles.

#### Option B: Johnson 12V 600 RPM (SKU 5729)
* Rated Torque: **15.1 N-cm (1.54 kg-cm)**
* Stall Torque: **122 N-cm (12.4 kg-cm)**
* Linear Speed with 100mm Wheels: $pprox \mathbf{3.14\,	ext{m/s} \quad (11.3\,	ext{km/h})}$
* **Verdict**: Faster sprint speed, but half the stall torque. Higher risk of motor stall on rough outdoor slopes.

---

### Zero-Latency 5.8GHz FPV Optical Reconnaissance Pipeline
1. **Camera**: 1/3" CMOS 1500TVL Mini FPV Camera mounted on front tilt servo.
2. **Video Transmitter (VTX)**: 5.8 GHz mini video transmitter (25mW to 600mW switchable) powered from 5V/12V rail.
3. **Ground Receiver (ROTG01)**: Plugs directly into the Ground Station Laptop or Android phone via USB Type-C.
4. **Integration**: Appears as a standard **UVC Video Device** (`/dev/video0` or Windows Media Foundation Webcam), allowing the Tactical Ground Station web HUD to embed live video with **near-zero latency (<35ms)** without loading Wi-Fi network bandwidth!

---

### Step-by-Step Construction Roadmap for CyberRover X5

```
┌────────────────────────────────────────────────────────────────────────┐
│                   CYBERROVER X5 BUILD PHASES                           │
├────────────────────────────────────────────────────────────────────────┤
│ PHASE 1: MECHANICAL FABRICATION                                        │
│ • Cut and drill lower and upper chassis decks matching CAD geometry    │
│ • Mount 4x EasyMech steel L-clamps using M4 hardware                   │
│ • Install 4x Johnson 12V geared motors and attach 100mm wheels        │
│ • Install 6x brass/nylon standoffs connecting lower and upper decks    │
├────────────────────────────────────────────────────────────────────────┤
│ PHASE 2: POWER GRID ASSEMBLY                                           │
│ • Assemble 3S 18650 battery pack with 20A/40A BMS and XT60 connector  │
│ • Install high-current master rocker switch and common star busbar     │
│ • Wire LM2596 buck converter and tune precisely to 5.00V output        │
│ • Verify zero ground loop resistance across all rails                  │
├────────────────────────────────────────────────────────────────────────┤
│ PHASE 3: ACTUATION & MOTOR DRIVER WIRING                               │
│ • Mount dual BTS7960 drivers in central bay                            │
│ • Connect motor leads to BTS7960 output terminals                      │
│ • Wire PWM control lines (D5, D6, D9, D10) to Arduino Uno              │
│ • Bench-test forward, reverse, and tank turn skid steering             │
├────────────────────────────────────────────────────────────────────────┤
│ PHASE 4: RECONNAISSANCE & SENSING SUITE INTEGRATION                    │
│ • Mount tilt micro-servo and 1500TVL FPV camera in front nose aperture │
│ • Install YDLIDAR X2 2D LiDAR on upper deck with 360° field of view    │
│ • Mount Layer 2 Environmental Deck (Nano + MQ sensors + BMP280 + GPS)   │
│ • Connect Reyax RYLR998 LoRa transceiver with external whip antenna    │
├────────────────────────────────────────────────────────────────────────┤
│ PHASE 5: TELEOPERATION & GROUND COCKPIT COMMISSIONING                  │
│ • Power on ESP32 Handheld Remote and establish 100 Hz ESP-NOW link     │
│ • Connect USB-TTL LoRa receiver to Laptop Ground Station Cockpit       │
│ • Verify real-time SQLite database insertion and live HUD oscilloscope │
│ • Conduct rough terrain field trials in local Dhanbad terrain          │
├────────────────────────────────────────────────────────────────────────┤
│ PHASE 6: POST-RUNNING STAGE — 5G WEBRTC TELEOPERATION (UNLIMITED RANGE)│
│ • Mount Poco M6 Plus 5G on upper deck via vibration-isolated clamp     │
│ • Connect USB-C OTG cable directly to STM32 USART1 (0ms wired link)    │
│ • Deploy WebRTC UDP P2P streaming to eliminate TCP buffer-bloat lag    │
│ • Achieve 55-85ms glass-to-glass latency over Jio/Airtel 5G network    │
│ • Retain NRF24 and Sub-GHz LoRa as zero-infrastructure failsafe backup │
└────────────────────────────────────────────────────────────────────────┘
```

---

*This Master Knowledge Base represents the definitive, authoritative reference for the entire CyberRover lineage. It contains all technical facts, electrical pinouts, communication packets, calibration constants, and architectural plans necessary for any future engineering session on CyberRover X5.*
