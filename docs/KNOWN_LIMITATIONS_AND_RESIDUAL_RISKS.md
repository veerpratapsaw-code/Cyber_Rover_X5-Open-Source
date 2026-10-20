# ⚠️ CYBERROVER X5 — KNOWN LIMITATIONS & RESIDUAL RISKS (FIRMWARE v1.5)

**Document Classification:** Safety Auditing & Failure Modes Analysis (FMEA)  
**Lead Architect & Developer:** Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh, Jharkhand  

---

## 1. Engineering Realism Disclaimer

No embedded firmware or wireless protocol is completely immune to failure, noise, or physical damage. The objective of CyberRover X5 engineering is to **identify potential failure modes, bound their worst-case consequences, and demonstrate reliable mitigation through test evidence**.

---

## 2. Failure Modes & Effects Analysis (FMEA)

| Failure Mode | Root Cause | Observable Symptom | Firmware Mitigation | Residual Risk |
| :--- | :--- | :--- | :--- | :--- |
| **RF Swamping / LNA Saturation** | Transmitter and receiver operating $< 30\text{ cm}$ apart with PA+LNA | Intermittent packet loss or false link drop on the bench | Operate modules $> 1.0\text{ m}$ apart; firmware set to `RF24_PA_LOW` | Low on the bench; zero risk at operational ranges $> 5\text{ m}$ |
| **DuPont Solder Fatigue** | Mechanical vibration loosening female jumper leaves on perfboard | SPI lines floating HIGH (`0xFF`), packet rate spiking | 12-bit ADC sanity gatekeeper immediately rejects `0xFFFF` frames and flushes FIFO | Medium until permanent soldered perfboard wiring replaces loose jumpers |
| **3.3V LDO Voltage Sag** | NRF24 PA transmit burst exceeding USB-TTL 3.3V regulator current limit | Radio registers reset, radio stops responding | Powered via 5V pin into Blue Pill onboard regulator; 10uF cap across NRF24 | Low when powered from 1S 18650 Li-ion + MT3608 5V boost converter |
| **2.4 GHz Congestion** | Heavy Wi-Fi traffic on channels 1–13 (2.412 – 2.472 GHz) | Increased packet drop rate | Radio parked on **Channel 108 (2.508 GHz)**, above all standard Wi-Fi | Minimal; microwaves or rogue FHSS devices can still introduce noise |
| **Stale Packet Execution** | Remote powered off while vehicle is moving forward at full speed | Vehicle continues driving autonomously into obstacles | **Autonomous 500ms Receiver Failsafe** clamps motor PWM to zero immediately | Zero runaway risk; motor stops within 500ms of signal loss |
| **MCU Software Lockup** | Memory corruption, unhandled ISR, or endless loop | Remote controls freeze; vehicle maintains last command | **Hardware Independent Watchdog (IWDG)** forces MCU reset within 1500ms | 1.5 second latency before watchdog reset occurs |

---

## 3. Hardware Boundaries & Constraints

1. **Non-Cryptographic Protocol:** The NRF24L01+ hardware CRC-16 ensures payload bit integrity, but does not provide cryptographic authentication. Rogue 2.4 GHz transmitters on Channel 108 using pipe `0xE8E8F0F0E1` could technically inject packets. In hostile tactical environments, secondary Sub-GHz LoRa with rolling HMAC is recommended.
2. **Current Bench Receiver Role:** The receiver sketch `cyberrover_x5_rover_rx.ino` currently acts as a verified bench telemetry and failsafe monitor. When interfacing with actual BTS7960 motor driver hardware, the `RoverMotorCommand` struct values (`throttle_norm`, `steering_norm`) must be mapped to PWM timer output pins.
