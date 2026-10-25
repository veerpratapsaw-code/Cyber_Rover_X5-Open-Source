# 🧾 CYBERROVER X5 — BILL OF MATERIALS (BOM) & LOWEST COST GUIDE
### *Complete Parts List, Salvage Hacks, and Budget Breakdown*

> **Lead Architect:** Veer Pratap Saw  
> **Engineering Principle:** *"Professional industrial performance at the lowest possible out-of-pocket cost."*  
> **Commercial Alternative:** A hobby-grade RC combat vehicle with an ELRS transmitter costs **₹15,000 to ₹25,000+**.  
> **CyberRover X5 Cost:** **~₹2,400 to ₹3,200** if buying brand new, or **₹250 to ₹600** if using smart salvage hacks!

---

## 📋 1. Complete Itemized Components Table

### A. Rover Chassis & Drivetrain
| Component | Qty | Purpose | Brand New Price (INR) | Low-Cost / Salvage Alternative |
|:---|:---:|:---|:---:|:---|
| **12V 600 RPM Johnson Motors** | 4 | Heavy-duty drive motors (555 core, 12.4 kg-cm torque) | ₹960 (₹240 each) | 300 RPM Johnson motors or 37GB geared motors |
| **100mm Ribbed Wheels** | 4 | Wide high-traction drive tires | ₹360 (₹90 each) | 80mm–100mm toy car or robot wheels |
| **EasyMech Steel Motor Brackets** | 4 | Rigid motor clamps & shock armor | ₹200 (₹50 each) | Standard steel L-clamps from local hardware store (₹15 each) |
| **Dual BTS7960 43A Drivers** | 2 | High-power MOSFET H-bridges | ₹480 (₹240 each) | Single dual-driver module |
| **ESP32-S3 Dev Board** | 1 | Rover 2.4 GHz wireless bridge | ₹380 | Standard ESP32 DevKit (₹220) |
| **STM32F103C8T6 Blue Pill** | 1 | Rover low-level motor supervisor | ₹220 | Arduino Nano (with 10-bit ADC scaling) |
| **Plywood Deck Plates** | 2 | Base chassis & top deck | ₹100 | Free wood cutoffs from a local furniture carpenter |
| **Threaded Steel Rods (M6/M8) + Nuts** | 4 | Deck standoff uprights | ₹80 | Long bolts or PVC pipe spacers |
| **3S 2200 mAh LiPo Battery** | 1 | Rover high-current powertrain | ₹850 – ₹1,200 | 3x 18650 Li-ion cells in a 3S holder or 12V drill battery |
| **XT60 Connector & 14 AWG Wire** | 1 | Battery power harness | ₹40 | Direct terminal block |

---

### B. Handheld Remote Controller
| Component | Qty | Purpose | Brand New Price (INR) | Low-Cost / Salvage Alternative |
|:---|:---:|:---|:---:|:---|
| **STM32F103C8T6 Blue Pill** | 1 | 72 MHz transmitter brain (12-bit ADC)| ₹220 | Salvaged from old projects |
| **ESP32 DevKit V1 Module** | 1 | 2.4 GHz ESP-NOW gateway transmitter | ₹220 | Any standard ESP32 board |
| **5-Pin Analog Thumb Joysticks** | 2 | Left & Right control gimbals | ₹90 (₹45 each) | Salvaged from old gamepads |
| **10k Rotary Potentiometers** | 3 | Speed Gov, Turn Ratio & Aux dials | ₹30 (₹10 each) | Volume dials from old amplifiers / radios |
| **1.3" I2C SH1106 OLED Display** | 1 | Pilot telemetry cockpit HUD | ₹240 – ₹280 | 0.96" SSD1306 OLED (₹180) |
| **SPST Toggles & Pushbuttons** | 4 | ARM toggle, mode, horn, headlight | ₹50 | Salvaged tactile buttons & switches |
| **1S 18650 Li-ion Cell (3.7V)** | 1 | Remote power supply | ₹120 – ₹160 | Salvaged free from old dead laptop batteries |
| **TP4056 USB-C Charger Module** | 1 | Safe Li-ion USB charging | ₹25 | TP4056 Micro-USB version (₹18) |
| **MT3608 Mini 5V Boost Converter**| 1 | Prevents 1.1V LDO dropout | ₹35 | Essential (do not skip) |
| **2mm / 3mm MDF Craft Wood** | 1 sheet| Handheld remote enclosure box | ₹50 | Scrap cardboard or wooden packing crate |

---

## 💰 2. Total Cost Comparison

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ TOTAL BUDGET OVERVIEW                                                                  │
├─────────────────────────────────────────────┬──────────────────────────────────────────┤
│ Buying Everything Brand New Online          │ ~₹2,400 to ₹3,200 INR (~$30 - $38 USD)   │
├─────────────────────────────────────────────┼──────────────────────────────────────────┤
│ Smart Salvage Route (Veer's Method)         │ ~₹250 to ₹600 INR out-of-pocket!         │
├─────────────────────────────────────────────┼──────────────────────────────────────────┤
│ Commercial Equivalent (ELRS RC Car + Radio) │ ₹15,000 to ₹25,000+                      │
└─────────────────────────────────────────────┴──────────────────────────────────────────┘
```

---

## 💡 3. Pro Money-Saving Hacks (How to Build for Dirt Cheap)

1. **Free 18650 Li-ion Cells from Dead Laptop Batteries:**  
   Ask any local computer repair shop for an old, discarded laptop battery pack (they often give them away for free or ₹50). Inside, there are 6 standard 18650 cells. Even when a laptop battery says "dead", usually only 1 cell failed, and the other 5 cells are in near-perfect health!
2. **Free Plywood & MDF Scrap:**  
   Visit any local carpentry, furniture workshop, or picture-frame maker. They throw away buckets of 6mm plywood and 2mm MDF offcuts every single day. They will happily give you enough scrap wood to build 3 rovers for free!
3. **Use Simple L-Brackets from Hardware Stores:**  
   If EasyMech brackets are out of stock or cost too much shipping, buy standard 2-inch steel L-brackets from a local construction hardware shop for ₹10–₹15 each and drill two matching motor holes.
4. **Wiring from Old Broken Cables:**  
   You do not need to buy expensive rolls of wire. An old broken USB cable, computer ribbon cable, or discarded Ethernet LAN cable contains 8 color-coded copper wires inside that are perfect for electronics!
