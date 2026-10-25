# 📜 CYBERROVER X5 — CHANGELOG & MIGRATION GUIDE (v1.4 $\to$ v1.5)

**Lead Architect & Developer:** Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh, Jharkhand  

---

## 1. Summary of Major Changes in Firmware v1.5

### A. Modular Architecture
* Split monolithic codebase into clean, decoupled, unit-testable C++ modules:
  * `cyberrover_protocol.h` (Shared 24-byte packet definition, bounds check, endianness).
  * `cyberrover_config.h` (Hardware pin definitions and deterministic scheduling intervals).
  * `analog_filter.h` / `analog_filter.cpp` (3-stage median + IIR + deadband pipeline).
  * `digital_inputs.h` / `digital_inputs.cpp` (Non-blocking debounce and edge detection).
  * `alert_engine.h` / `alert_engine.cpp` (Priority arbitration and sound mute FSM).
  * `radio_tx_manager.h` / `radio_tx_manager.cpp` (Transmitter RF state machine).
  * `radio_rx_manager.h` / `radio_rx_manager.cpp` (Receiver RF state machine, zero SPI contention).
  * `receiver_failsafe.h` / `receiver_failsafe.cpp` (500ms timeout and neutral motor clamp).
  * `watchdog_mgr.h` / `watchdog_mgr.cpp` (Hardware STM32 Independent Watchdog IWDG).

### B. Sound Mute Feature (User Requirement)
* **Toggle Right (`PB4`)** is now formally mapped as the **Sound Mute Switch**.
* Flipping `PB4` to Mute silences the piezo buzzer (`PB8`) completely ($0\text{ dB}$ audio emission for late-night bench development).
* Haptic motor vibration feedback (`PB9`) remains operational so the pilot retains physical confirmation of clicks and notch transitions.
* The OLED HUD displays a subtle `'M'` indicator badge in the center status column when mute is engaged.

### C. SPI Zero-Contention Receiver Engine
* Completely eliminated `radio.isChipConnected()` polling during active packet streaming.
* Incoming packets are read directly via `radio.available()`, eliminating false-positive hardware disconnect alarms.
* Bounded silicon recovery runs only if the link drops continuously for over $2000\text{ ms}$.

### D. Automated Host Unit Test Harness
* Implemented `tests/unit_tests.cpp` containing **44 automated test assertions** executed on host GCC/G++ with **100% pass rate**.

---

## 2. Compilation & Verification Evidence

### Build Commands:
```powershell
# 1. Compile Transmitter Firmware
arduino-cli compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8 "firmware/cyberrover_x5_remote_tx"

# 2. Compile Receiver Firmware
arduino-cli compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8 "firmware/cyberrover_x5_rover_rx"

# 3. Build & Run Automated Host Unit Tests
g++ -std=c++17 -Wall -Wextra tests/unit_tests.cpp firmware/cyberrover_x5_remote_tx/analog_filter.cpp firmware/cyberrover_x5_remote_tx/digital_inputs.cpp firmware/cyberrover_x5_rover_rx/receiver_failsafe.cpp -o tests/unit_tests.exe
.\tests\unit_tests.exe
```

### Measured Resource Benchmarks:
* **Transmitter (TX):**
  * Flash Memory: **$51,712\text{ bytes}$ ($78\%$ of $65,536\text{ bytes}$ limit)**
  * SRAM: **$3,496\text{ bytes}$ ($17\%$ of $20,480\text{ bytes}$ limit)**
  * Compiler Status: **0 Errors, 0 Warnings**
* **Receiver (RX):**
  * Flash Memory: **$20,960\text{ bytes}$ ($31\%$ of $65,536\text{ bytes}$ limit)**
  * SRAM: **$1,532\text{ bytes}$ ($7\%$ of $20,480\text{ bytes}$ limit)**
  * Compiler Status: **0 Errors, 0 Warnings**
* **Automated Unit Tests:**
  * **$44 / 44\text{ Tests Passed}$ ($100\%$ Success Rate)**
