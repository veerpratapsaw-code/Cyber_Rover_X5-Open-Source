# 🚀 CyberRover X5: Heavy-Duty Hazardous Reconnaissance UGV
### *Industrial Disaster & Autonomous Environmental Inspection Ground Vehicle*

**Lead Developer & Architect**: Veer Pratap Saw (Veer), Class 10 Student  
**Location**: Katras, Dhanbad / Ramgarh, Jharkhand, India  
**Platform Status**: Phase 7 Architecture & Production Transition (X1 ➔ X4.3 ➔ X5)

---

## 📚 Core Documentation & Intelligence Assets

| File | Purpose | Contents |
| :--- | :--- | :--- |
| 📄 [`CYBERROVER_MASTER_KNOWLEDGE_BASE.md`](CYBERROVER_MASTER_KNOWLEDGE_BASE.md) | **Master Engineering Encyclopedia** | 861-line single-source-of-truth containing the complete evolution from X1 to X4.3, formulas, pinouts, protocols, and exhibition viva defense scripts. |
| 📄 [`CYBERROVER_X5_REMOTE_CONTROLLER_MASTER_GUIDE.md`](CYBERROVER_X5_REMOTE_CONTROLLER_MASTER_GUIDE.md) | **Transmitter Production Blueprint** | Complete engineering guide for the dual-radio handheld transmitter: $170 \times 125 \times 42\text{ mm}$ MDF chassis, STM32 pinouts, 1S boost power fix, and full C++ firmware. |
| 📄 [`AI_CONTEXT_HANDOFF.txt`](AI_CONTEXT_HANDOFF.txt) | **Continuous AI Memory Dossier** | A high-density summary file that transfers all past context and decisions to any new AI chat session without repeated explanations. |
| 📄 [`AGENTS.md`](AGENTS.md) | **Autonomous Agent Directives** | Mandatory rules for any AI agent entering the repository (strict English, context ingestion on turn 1, and the continuous update mandate). |

---

## 🖼️ Media & Visual Design Archives

* 📁 [`remote_designs/`](remote_designs/) — High-resolution concept renders and workshop photos of the CyberRover X5 transmitter:
  * `raw_mdf_remote_front_*.jpg`: Front & isometric view of the unpainted, unwrapped MDF transmitter.
  * `raw_mdf_remote_rear_*.jpg`: Rear panel view showing the service hatch, USB-C port, and TTL flash header.
  * `acrylic_sandwich_remote_*.jpg`: Acrylic + brass standoff concept.
  * `printed_gamepad_remote_*.jpg`: Ergonomic 3D-printed gamepad concept.
  * `abs_project_box_remote_*.jpg`: Heavy-duty ABS project box concept.
* 📁 [`past_reference_images/`](past_reference_images/) — 257 authentic historical photos categorized into:
  * `01_cad_and_parts_shortlist_x5/`: CAD Tinkercad models, parts screenshots, and component specifications.
  * `02_evolution_and_prototypes/`: Historical X1 combat robot, X2 BIT Sindri, X3 YOLO vision rover.
  * `03_remote_controller/`: Legacy handcrafted X4.3 remote hero shots and internal wiring.
  * `04_rover_x43_studio_and_field/`: Award-winning X4.3 studio poses, field tests, and gas sensor cards.
  * `05_original_camera_photos/`: Raw build photos from the September 2026 state championship run.

---

## ⚡ Quick Specs: CyberRover X5
* **Drivetrain**: 4x 12V 600 RPM Johnson motors (555 core, 12.44 kg-cm stall torque) + 100mm rubber wheels.
* **Top Speed**: $11.31\text{ km/h}$ ($3.14\text{ m/s}$) with software speed governor scaling down to $1.3\text{ km/h}$ indoor crawl.
* **Motor Drivers**: Dual BTS7960 43A MOSFET H-Bridges.
* **Chassis Stability**: Static Stability Factor = 2.36 ($67^\circ$ tipping angle). Rollover-proof with software speed-sensitive steering rate limiter.
* **Control Links**: Dual-Radio Hybrid (NRF24L01 2.4 GHz for 10ms sprint driving + Reyax RYLR998 915 MHz LoRa for 400m concrete penetration & limp-home mode).
* **Optical & Spatial**: 1500TVL starlight FPV camera on micro-servo tilt + 5.8 GHz VTX (cooling fan equipped) + 2D LiDAR 36-sector downsampled radar stream.
