# 🚀 CYBERROVER X5 — STABLE RELEASE v2.0 (HANDHELD REMOTE TELEOPERATION)

> **Release Status:** Production Verified & Ground Tested  
> **Test Profile:** Physical ground running, high-speed sprints (11.8 km/h), power drifting, jumps, and ramps on 3S 2200 mAh LiPo.  
> **Throughput & Protocol:** 50 Hz unthrottled teleoperation using 16-byte packed `CyberPacket` v3.0 with CRC-8 and Device ID `0xC5`.

---

## 📦 Firmware Modules Included

1. `01_remote_tx_stm32/`  
   Transmitter firmware for STM32 Blue Pill. Reads 13 channels in 12-bit ADC, renders 10 FPS OLED pilot HUD via 400 kHz Fast I2C, and mirrors output to both `Serial` and `Serial1`.
2. `02_remote_gateway_esp32/`  
   RF gateway firmware for standard ESP32 DevKit. Transmits 16-byte packets via ESP-NOW Smart Direct Unicast (`1C:DB:D4:4B:08:40`) with self-healing auto-discovery and zero modem sleep.
3. `03_rover_s3_bridge/`  
   Rover-side wireless bridge on ESP32-S3. Unpacks ESP-NOW frames, verifies CRC-8, generates an 80ms command heartbeat, and forwards drive packets to the STM32 Motor Supervisor in $<10\ \mu\text{s}$.
4. `04_rover_stm32_motor_supervisor/`  
   Chassis motor supervisor for STM32 Blue Pill. Drives dual BTS7960 43A MOSFET H-bridges for 4x Johnson 600 RPM motors with dynamic turn scaling (0%–75%), 800 PWM/s acceleration punch, and a 650ms dead-man watchdog.
5. `shared/`  
   Common `cyberrover_protocol.h` shared across all four MCUs.

---

## ⚡ Quick Flash Guide (3 Steps)

1. **Remote Transmitter:**  
   * Flash `01_remote_tx_stm32` to Remote STM32 Blue Pill via ST-Link or USB-TTL at 115200 baud.
   * Flash `02_remote_gateway_esp32` to Remote ESP32 DevKit via standard USB cable (Board: `ESP32 Dev Module`).
2. **Rover Chassis:**  
   * Flash `03_rover_s3_bridge` to Rover ESP32-S3 via native USB-C (Board: `ESP32S3 Dev Module`, USB CDC Enabled).
   * Flash `04_rover_stm32_motor_supervisor` to Rover STM32 Blue Pill via ST-Link or USB-TTL.
3. **Power-On Sequence:**  
   * Power on in any order (Remote first or Car first works identically).
   * Remote blue LED locks solid when paired. Flip Toggle Left (PB4) to **ARM** and drive!
