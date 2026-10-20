# 📐 CYBERROVER X5 — TECHNICAL ARCHITECTURE DOCUMENT (FIRMWARE v1.5)

**Document Classification:** Mission-Critical System Architecture Specification  
**Lead Architect & Developer:** Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh, Jharkhand  
**Platform:** Dual STM32F103C8T6 (72 MHz ARM Cortex-M3) Blue Pill  
**Target Core:** `STMicroelectronics:stm32` v3.0.0 (Official ST Core)  
**Radio Subsystem:** NRF24L01+PA+LNA (2.4 GHz Native Hardware SPI1)  

---

## 1. System Overview & Engineering Paradigm

CyberRover X5 employs a **segregated, deterministic multi-rate embedded architecture** designed for hazardous reconnaissance operations. The transmitter and receiver operate as mutually synchronized, non-blocking finite state machines (FSMs) communicating over a 24-byte little-endian packed protocol.

```mermaid
graph TD
    subgraph Transmitter [Handheld Pilot Remote - STM32F103C8]
        A1[12-Bit ADC Acquisition 7 Channels] --> A2[Stage 1: Order-3 Median Filter]
        A2 --> A3[Stage 2: Single-Pole Low-Pass IIR Filter]
        A3 --> A4[Stage 3: Zero-Discontinuity Deadband & Invert]
        
        D1[Digital GPIO 6 Channels] --> D2[20ms Debounce Integrator]
        D2 --> D3[Mute & Priority Arbitrator]
        
        A4 & D3 --> P1[RoverPacket Encoder 24 Bytes]
        P1 --> TX1[RadioTxManager FSM @ 40 Hz]
        TX1 --> RF[NRF24L01+ PA+LNA 250 kbps]
        
        HUD[U8g2 SH1106 HUD @ 25 FPS]
        WD1[STM32 IWDG Watchdog 1500ms]
    end

    RF -. 2.408 GHz Channel 108 .-> RX1

    subgraph Receiver [Rover Carrier Node - STM32F103C8]
        RX1[RadioRxManager FSM Zero-Contention] --> VAL[Strict 12-Bit Sanity Filter]
        VAL -- Valid Frame --> FS[ReceiverFailsafe Supervisor]
        VAL -- Corrupt Frame --> FLUSH[Flush RX FIFO & Reject]
        
        FS --> MOT[RoverMotorCommand Clamp / Dual BTS7960]
        FS -- No Packet > 500ms --> SAFE[Autonomous Neutral Clamp]
        RX1 -- Lost > 2000ms --> REC[Controlled Background Recovery]
    end
```

---

## 2. Shared Protocol Layer (`cyberrover_protocol.h`)

* **Payload Layout:** Packed 24-byte binary structure without compiler alignment padding (`#pragma pack(push, 1)` and `#pragma pack(pop)`).
* **Compile-Time Integrity:** Guaranteed via `static_assert(sizeof(RoverPacket) == 24)`.
* **Endianness:** Little-Endian (native ARM Cortex-M3 byte ordering).
* **Sanity Validation:** `isRoverPacketValid(const RoverPacket&)` verifies that all 7 analog channels are $\le 4095$ (rejecting floating SPI `0xFFFF` noise) and all digital switches are binary ($0$ or $1$).

| Field Name | Type | Size | Valid Range | Physical Function |
| :--- | :--- | :--- | :--- | :--- |
| `stick_lx` | `uint16_t` | 2 Bytes | $0 - 4095$ | Left Stick X (Steering / Turn Blinkers) |
| `stick_ly` | `uint16_t` | 2 Bytes | $0 - 4095$ | Left Stick Y (Throttle / Speed & RPM) |
| `stick_rx` | `uint16_t` | 2 Bytes | $0 - 4095$ | Right Stick X (Inverted 90° Gimbal mount) |
| `stick_ry` | `uint16_t` | 2 Bytes | $0 - 4095$ | Right Stick Y (Camera Tilt Elevation) |
| `pot_l` | `uint16_t` | 2 Bytes | $0 - 4095$ | Potentiometer Left (Speed Governor ceiling) |
| `pot_c` | `uint16_t` | 2 Bytes | $0 - 4095$ | Potentiometer Center (Camera Tilt Trim) |
| `pot_r` | `uint16_t` | 2 Bytes | $0 - 4095$ | Potentiometer Right (Auxiliary / Battery Sim) |
| `toggle_l` | `uint8_t` | 1 Byte | $0 \text{ or } 1$ | Toggle Left (Master Arm / Disarm: $1 = \text{Drive}, 0 = \text{Park}$) |
| `toggle_r` | `uint8_t` | 1 Byte | $0 \text{ or } 1$ | Toggle Right (Sound Mute: $1 = \text{Muted}, 0 = \text{Sound Active}$) |
| `push_l` | `uint8_t` | 1 Byte | $0 \text{ or } 1$ | Push Left ($1 = \text{Pressed}$) [PA8] |
| `push_r` | `uint8_t` | 1 Byte | $0 \text{ or } 1$ | Push Right ($1 = \text{Pressed}$) [PA15] |
| `stick_clk_l`| `uint8_t` | 1 Byte | $0 \text{ or } 1$ | Left Stick Click ($1 = \text{Pressed}$) [PA10] |
| `stick_clk_r`| `uint8_t` | 1 Byte | $0 \text{ or } 1$ | Right Stick Click ($1 = \text{Pressed}$) [PA11] |
| `frame_id` | `uint32_t` | 4 Bytes | $0 - 2^{32}-1$ | Rolling 32-bit sequence counter |
| **TOTAL** | | **24 Bytes** | | **Zero Padding Overhead** |

---

## 3. Multi-Rate Deterministic Scheduling

The main control loops avoid blocking delays (`delay()`) during operational states. Execution is scheduled via monotonic, rollover-safe elapsed time comparisons (`now - lastTime >= INTERVAL`):

1. **RF Command Transmission:** $40\text{ Hz}$ ($25\text{ ms}$ interval). Guarantees control latency $< 25\text{ ms}$.
2. **Virtual Cockpit HUD Refresh:** $25\text{ FPS}$ ($40\text{ ms}$ interval). Non-blocking framebuffer rendering over hardware I2C1 ($400\text{ kHz}$).
3. **Serial Telemetry Stream:** $10\text{ Hz}$ ($100\text{ ms}$ interval). Clean, rate-limited diagnostics without serial buffer congestion.
4. **Heartbeat LED Activity:** $1\text{ Hz}$ ($500\text{ ms}$ toggle).
5. **Hardware Watchdog Reload:** Executed on every successful, unblocked pass of the main control loop.

---

## 4. Hardware Watchdog Subsystem (IWDG)

* **Architecture:** Dedicated independent hardware watchdog clocked from the internal $40\text{ kHz}$ low-speed internal oscillator (LSI).
* **Timeout Window:** Configured to $1500\text{ ms}$.
* **Safety Philosophy:** The watchdog is fed **only** when the main loop completes all critical processing tasks. If firmware hangs in an infinite loop, brownouts, or suffers an unhandled hard fault, the IWDG resets the MCU within $1.5\text{ seconds}$.
* **Boot Diagnostics:** On power-up, `watchdogMgr.wasResetByWatchdog()` interrogates `RCC_CSR` to determine if the reboot was an intentional power cycle or a watchdog recovery event, logging the incident immediately to serial telemetry.
