# 🤖 AGENT INSTRUCTIONS & WORKSPACE PROTOCOL: CYBERROVER X5

> **CRITICAL DIRECTIVE FOR ALL AI AGENTS & ASSISTANTS ENTERING THIS WORKSPACE**:
> This file is automatically loaded by AI coding assistants. Read this before taking any action or answering user prompts.

---

## 👤 1. USER & PROJECT IDENTITY
* **Lead Architect & Developer**: **Veer Pratap Saw** (Veer).
* **Academic Level**: **Class 10 Student** (Secondary School), Katras / Dhanbad / Ramgarh, Jharkhand, India.
* **Project Heritage**: CyberRover X1 $\rightarrow$ X2 $\rightarrow$ X3 $\rightarrow$ X4 $\rightarrow$ X4.2 $\rightarrow$ X4.3 (1st Prize State-Level Science Championship Winner, Ramgarh).
* **Current Mission**: Building **CyberRover X5** — an industrial heavy-duty hazardous reconnaissance UGV.
* **Current Physical State**: Vehicle and previous remote are completely disassembled. Building fresh.
* **Language Requirement**: **STRICTLY STANDARD ENGLISH**. The user has explicitly forbidden Hinglish.

---

## ⚡ 2. MANDATORY OPERATING RULES FOR AI

### Rule 1: Zero Redundant Questioning
Do NOT ask Veer to re-explain his project background, awards, previous rover iterations, or why certain parts were chosen. Everything is exhaustively documented in this repository.

### Rule 2: Mandatory Context Ingestion on Turn 1
Before answering any complex architectural question, read:
1. [`AI_CONTEXT_HANDOFF.txt`](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/AI_CONTEXT_HANDOFF.txt) — Contains the current session state, memory cache, and exact next steps.
2. [`CYBERROVER_MASTER_KNOWLEDGE_BASE.md`](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/CYBERROVER_MASTER_KNOWLEDGE_BASE.md) — 861-line technical encyclopedia.
3. [`CYBERROVER_X5_REMOTE_CONTROLLER_MASTER_GUIDE.md`](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/CYBERROVER_X5_REMOTE_CONTROLLER_MASTER_GUIDE.md) — Handheld transmitter blueprint and firmware.

### Rule 3: The "Continuous Intelligence Update" Mandate (Before Leaving)
**BEFORE YOU CONCLUDE YOUR SESSION OR TURN:**
If you and Veer make any new design decisions, establish new pinouts, calculate new mechanical/electrical numbers, write new firmware modules, or debug hardware:
* **YOU MUST UPDATE [`AI_CONTEXT_HANDOFF.txt`](file:///c:/Users/sanjay/Desktop/Folders/Programs/cyber_rover/cyberrover%20X5/AI_CONTEXT_HANDOFF.txt)** with the new intelligence!
* This guarantees that the next AI conversation begins with 100% continuity without making Veer repeat himself.

---

## 📐 3. ESTABLISHED ARCHITECTURAL TRUTHS (DO NOT CONTRADICT)
1. **Drivetrain**: 4x 12V 600 RPM Johnson motors (555 core, 12.44 kg-cm stall torque) + 100mm wheels + dual BTS7960 43A MOSFET drivers. Top speed: $11.31\text{ km/h}$.
2. **Anti-Flip Stability**: Static Stability Factor = 2.36 ($67^\circ$ static tip angle). Safe turning radius at full throttle $R \ge 0.43\text{ m}$. Speed-sensitive steering rate limiter in firmware prevents rollovers.
3. **Dual-Radio Topology**:
   * Primary: NRF24L01+PA+LNA (2.4 GHz, 50 Hz loop, 10–15ms latency) for real-time driving.
   * Secondary: Reyax RYLR998 LoRa (915 MHz, 5 Hz loop, 200ms latency) for 400m concrete penetration and automated "Limp-Home" $\le 1.8\text{ km/h}$ failsafe.
4. **Remote Controller**:
   * Enclosure: $170\text{ mm} \times 125\text{ mm} \times 42\text{ mm}$ made from 2mm MDF + 6mm plywood internal corner posts. Zero 3D-printing required.
   * MCU: STM32F103C8T6 (Blue Pill) running 100% native 3.3V logic (zero level converters).
   * Power: 1S 18650 cell + TP4056 USB-C + mini 5V Boost converter (MT3608) to STM32 5V pin (prevents 1.1V LDO dropout and radio brownouts).
   * Rear Hatch: Flush USB-C charging port, 5-pin TTL flash header, and BOOT0 slide switch.
   * Gimbals: 2x 5-Pin Dual-Axis XY Joystick breakout modules with dedicated `SW` digital pins.
   * Displays: 1.3" I2C OLED on Remote (pilot HUD); 0.96" I2C OLED on Rover (diagnostics).
5. **Optical & LiDAR Strategy**:
   * 1500TVL starlight FPV camera + micro-servo tilt + 5.8 GHz VTX (with mandatory cooling fan).
   * For deep building penetration: 2D LiDAR downsampled to 36 sectors (576 bps) + 2 FPS compressed JPEG slow-scan video over Sub-GHz radio.
