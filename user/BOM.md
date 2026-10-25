# 🧾 CYBERROVER X5 — OFFICIAL BILL OF MATERIALS (BOM) & PRICING
### *Authentic Component Breakdown, Verified Robu.in Costs, Transmitter Budget & Tooling*

> **Lead Architect:** Veer Pratap Saw (Katras / Ramgarh / Dhanbad, Jharkhand, India)  
> **Documentation Standard:** Real-world verified component prices reflecting authentic workshop purchases and Robu.in shortlist specifications.  
> **Commercial Comparison:** A commercial tactical combat ground robot with an off-the-shelf RadioMaster transmitter costs **₹25,000 to ₹45,000+**.  
> **CyberRover X5 Total Real Build Cost:** **~₹9,800 INR** for the complete turn-key machine (Rover + Custom 50 Hz Handheld Transmitter + Misc Hardware).

> [!IMPORTANT]
> ### 🛒 Where to Buy All Items: 100% Sourced from [Robu.in](https://robu.in)
> **You can buy all mechanical and electrical items directly from [Robu.in](https://robu.in) at the exact prices listed below!**  
> Veer personally purchased all core motors, steel mounting brackets, 100mm wheels, BTS7960 motor drivers, STM32 & ESP32 microcontrollers, and power buck converters directly from **[Robu.in](https://robu.in)**. The SKU codes in the table below match their catalog so you can search and add them directly to your cart.

> ### 📐 Interactive 3D CAD Blueprint (Public on Tinkercad)
> Want to inspect how all these components fit together on the chassis before purchasing?  
> Rotate, zoom, and inspect every standoff and cutout in your browser:  
> 👉 **[Tinkercad 3D Model: fighter cyberrover x5](https://www.tinkercad.com/things/4SGPQKu9c76-fighter-cyberrover-x5)**

---

## 🚜 1. Rover Vehicle Platform & Powertrain

| Component Description | Robu.in / Part Reference | Qty | Unit Price (INR) | Total Cost (INR) | Architectural Role & Technical Notes |
|:---|:---:|:---:|:---:|:---:|:---|
| **12V 600 RPM Johnson Geared DC Motor** | SKU 5729 | 4 | **₹611.00** | **₹2,444.00** | Heavy-duty 555-core armature, 12.44 kg-cm stall torque each, 6mm D-shaft. Delivers 11.88 km/h sprint velocity. |
| **EasyMech Advanced Johnson Motor L-Clamp** | SKU 33225 | 4 | **₹181.00** | **₹724.00** | 1.5mm mild steel bracket. Absorbs high-G ramp landing impacts and eliminates motor shaft flex. |
| **Robot Wheel 100mm Dia x 44mm Width** | SKU 5884 | 4 | **₹149.00** | **₹596.00** | Deep-groove ribbed rubber tread on 6mm bore. Provides 40mm ground clearance and high traction on concrete. |
| **Double BTS7960 43A MOSFET H-Bridge** | SKU 456144 | 2 | **₹329.00** | **₹658.00** | 43A peak current capacity per driver, 16 mΩ path resistance. Low-side GND braking with zero thermal throttling. |
| **STM32F103C8T6 Blue Pill MCU** | Rover Motor Supervisor | 1 | **₹250.00** | **₹250.00** | 72 MHz ARM Cortex-M3. Manages 100 Hz kinematic loop, dynamic turn ratio ('T'), 800 PWM/s punch, and 650ms watchdog. |
| **ESP32 / ESP32-S3 Dev Board** | Rover Wireless Bridge | 1 | **₹500.00** | **₹500.00** | Dual-core wireless transceiver. Translates 50 Hz ESP-NOW packets into high-speed UART in < 10 µs; zero modem sleep. |
| **3S 2200 mAh LiPo Battery (11.1V / 12.6V)** | High-Discharge Drone Pack | 1 | **₹1,800.00** | **₹1,800.00** | **Recommended.** Delivers 25C–35C high burst current for instant breakaway torque, power drifts, and high-G tank turns. |
| **5V Step-Down Buck Converter (UBEC)** | Mini Step-Down Module | 1 | **₹50.00** | **₹50.00** | Regulates 12.6V battery down to rock-solid 5.0V for the ESP32 and STM32 logic rails. |
| **Plywood Deck Plates (Base & Top Deck)** | 6mm – 8mm Marine Plywood | 2 | **₹100.00** | **₹100.00** | Recommended wood chassis. High torsional rigidity and impact absorption compared to 3D printed plastic. |
| **Threaded Steel Upright Rods (M6/M8) + Nuts**| Deck Standoff Pillars | 4 | **₹100.00** | **₹100.00** | Four threaded uprights with double locknuts. Secures 45mm deck clearance for Johnson motors. |
| **Heavy-Duty 30A Power Switch & XT60 Connector**| Master Power Harness | 1 | **₹100.00** | **₹100.00** | Genuine high-current XT60 battery plug and heavy-duty 30A toggle switch preventing contact welding under stall surges. |
| **SUBTOTAL (Rover Platform)** | | | | **₹7,322.00** | **Complete Heavy-Duty 4WD UGV Chassis** |

---

## 🎮 2. Handheld Remote Controller Transmitter (~₹2,000 All-Inclusive)

The custom handheld transmitter was built for **~₹2,000 INR total**, providing dual gimbals, 3 analog dials, OLED HUD, and long-range 50 Hz teleoperation at a fraction of commercial RC radio prices:

| Component Description | Purpose | Qty | Real Cost (INR) |
|:---|:---|:---:|:---:|
| **STM32F103C8T6 Blue Pill** | 72 MHz transmitter brain with 10x 12-bit linear ADCs (0–4095) | 1 | **₹250.00** |
| **ESP32 DevKit V1 Module** | 2.4 GHz ESP-NOW wireless transmitter gateway (or NRF24L01+PA+LNA) | 1 | **₹500.00** |
| **5-Pin Dual-Axis Thumbstick Gimbals** | Left & Right control gimbals with tactile click buttons | 2 | **₹100.00** |
| **10k Rotary Potentiometers + Knurled Dials**| Speed Governor, Dynamic Turn Ratio, and Aux/Menu selectors | 3 | **₹50.00** |
| **1.3" I2C SH1106 OLED Display Module** | Pilot cockpit HUD (400 kHz Fast I2C, 10 FPS, battery & speed gauges) | 1 | **₹250.00** |
| **1S 18650 Li-ion Cell (3.7V 2600 mAh)** | Dedicated handheld transmitter power source | 1 | **₹150.00** |
| **TP4056 USB-C Charger & Protection Board** | Standard smartphone USB-C charging interface | 1 | **₹35.00** |
| **MT3608 Mini 5V Boost Converter Module** | Steps up 3.7V to 5.05V (prevents 1.1V LDO dropout and RF brownout) | 1 | **₹40.00** |
| **2mm/3mm MDF Enclosure + Corner Posts** | Faceted transmitter body ($170 \times 125 \times 42\text{ mm}$) with rear service hatch | 1 | **₹150.00** |
| **Hardware Switches, Pushbuttons & Alerts** | ARM toggle, mode switch, horn, 5V buzzer, ERM haptic motor | Set | **₹175.00** |
| **Ribbon Cables, Solder & Internal Wiring** | Direct soldered ribbon bus and female header socket carrier | Set | **₹300.00** |
| **TOTAL (Custom Handheld Transmitter)** | | | **~₹2,000.00** |

> **Alternative Budget Remote Option:**  
> If you want to build an ultra-budget starter remote, you can use a **single ESP32 alone without the STM32**, directly reading the analog gimbals and transmitting over ESP-NOW for a reliable **~100-meter line-of-sight range**!

---

## 🔌 3. Miscellaneous Prototyping & Wiring Hardware (~₹500)

| Item Description | Included Materials | Estimated Cost (INR) |
|:---|:---|:---:|
| **Prototyping & Carrier Boards** | 2x Dotted Zero-Perfboard (large format) for Blue Pill carrier and wiring sockets | **₹120.00** |
| **Connectors & Header Pins** | 40-pin male header strips, 40-pin female header strips, JST connectors | **₹80.00** |
| **High-Current Power Wiring** | 14 AWG / 16 AWG flexible silicon copper wire for battery and motor lines | **₹150.00** |
| **Signal Ribbon Cable & Heat-Shrink** | Multi-conductor rainbow ribbon cable and assorted heat-shrink tubing | **₹150.00** |
| **TOTAL (Miscellaneous Hardware)** | | **₹500.00** |

---

## 🛠️ 4. Essential Tools Budget (~₹800 Starter Investment)

If you do not already own tools, this complete starter toolkit is all that is required:

| Essential Tool | Recommended Specification | Estimated Cost (INR) |
|:---|:---|:---:|
| **Soldering Iron (25W / 35W)** | Pointed copper bit soldering iron with safety stand | **₹200.00** |
| **Solder Wire (3M / 50g Reel)** | 60/40 rosin-core leaded or lead-free electronics solder | **₹120.00** |
| **Soldering Paste / Flux** | Rosin soldering flux paste for clean, shiny joints | **₹40.00** |
| **Hot Melt Glue Gun + 10 Glue Sticks** | 20W/40W glue gun for mechanical vibration strain relief and potting | **₹220.00** |
| **Digital Multimeter (DT-830D or similar)** | Voltage, current, resistance, and continuity buzzer probe | **₹160.00** |
| **Small Hand Tools & Strippers** | Wire stripper/cutter, small crosshead screwdriver | **₹60.00** |
| **TOTAL (Essential Tools Kit)** | | **~₹800.00** |

---

## ⚡ 5. Recommended Workbench Testing Setup

* **Variable DC Bench Power Supply (0–30V, 5A or 10A):**  
  * Highly recommended for your study/workshop table.
  * Allows you to bench-test motor spin directions, calibrate current sensors, and debug code for hours without constantly draining or recharging your 3S LiPo drone battery.
* **Firmware Flashing Interface:**  
  * Built directly into the USB ports of the ESP32 and USB-TTL / ST-Link headers accessible via the transmitter's rear service hatch.

---

## 📊 6. Grand Total Investment Summary

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ CYBERROVER X5 COMPLETE SYSTEM FINANCIAL AUDIT                                          │
├────────────────────────────────────────────────────────┬───────────────────────────────┤
│ 1. Rover Vehicle Platform (Motors, Drivers, S3, LiPo)  │ ₹7,322.00                     │
│ 2. Custom Handheld Remote Transmitter (OLED, Gimbals)  │ ₹2,000.00                     │
│ 3. Miscellaneous Header Pins, Silicon Wires & Hardware │ ₹500.00                       │
├────────────────────────────────────────────────────────┼───────────────────────────────┤
│ TOTAL TURN-KEY MACHINE BUILD COST                      │ ₹9,822.00 INR (~$118 USD)     │
├────────────────────────────────────────────────────────┼───────────────────────────────┤
│ 4. Optional One-Time Starter Tools Kit (Iron, DMM, etc)│ ₹800.00 INR                   │
├────────────────────────────────────────────────────────┼───────────────────────────────┤
│ GRAND TOTAL (Machine + Complete Tooling Kit)           │ ₹10,622.00 INR (~$127 USD)    │
└────────────────────────────────────────────────────────┴───────────────────────────────┘
```

> **Why This Value is Unbeatable:**  
> A commercial radio transmitter alone (such as a RadioMaster TX12 or FrSky Taranis) costs ₹10,000 to ₹14,000 without a vehicle. CyberRover X5 delivers a **complete 11.8 km/h heavy-duty 4WD ground combat rover, dual-MCU architecture, 50 Hz teleoperation, and a custom OLED transmitter** for under ₹10,000 INR!
