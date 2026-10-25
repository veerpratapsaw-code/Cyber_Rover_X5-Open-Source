# ⚠️ WHAT NOT TO DO — THE ROOKIE TRAPS & PITFALLS GUIDE
### *Learn from 850+ Hours of Real Mistakes So You Never Get Stuck or Discouraged*

> **Lead Architect:** Veer Pratap Saw  
> **Core Advice:** *"Making mistakes in hardware is normal, but repeating avoidable mistakes wastes weeks of time and money. Read this guide before powering up your circuits!"*

---

## 🚫 Trap 1: The Generic ChatGPT "Give Me RC Car Code" Trap (The #1 Mistake!)

```
❌ DO NOT DO THIS:
"Hey ChatGPT, give me code for a 4-wheel bluetooth/RF RC car."
```

### Why This Fails & Destroys Projects:
When you give a vague prompt to generic ChatGPT, it hallucinates ancient, low-quality code from 2013 designed for a plastic toy car running an Arduino Uno with an L298N driver and `delay(1000)`.
* **The Result:** The car freezes for 1 second every time a command is sent, the motors jerk violently, the radio link drops every 2 seconds, and the builder gets frustrated, scared, and quits!
* **The Fix:** **NEVER write code from scratch with a generic prompt!** All the firmware in this repository has been tested, calibrated, and verified on real concrete. Use the exact **Magic Prompt** in [`HOW_TO_MAKE.md`](HOW_TO_MAKE.md) that tells the AI to read [`ai/AGENTS.md`](ai/AGENTS.md) and load the production firmware from [`user/stable_versions/`](user/stable_versions/).

---

## 🚫 Trap 2: The Loose DuPont Jumper Wire Trap

```
❌ DO NOT USE: Loose female-to-female jumper wires for motor power or high-vibration gimbals.
```

### Why This Fails:
DuPont jumper wires use tiny stamped metal spring clips inside their plastic housings. When 4x Johnson motors spin at full throttle or the rover lands from a ramp jump, the vibrations shake the loose pins:
* The I2C display wire blinks out for 1 millisecond, permanently freezing the OLED screen.
* The battery ground wire jiggles, resetting the microcontroller mid-sprint.
* **The Fix:** Direct-solder wires to dotted perfboard or use solid screw terminal blocks. Use hot-melt glue or zip-ties as mechanical strain relief!

---

## 🚫 Trap 3: The 1S Battery Direct-to-LDO Voltage Dropout Trap

```
❌ DO NOT DO THIS: Connecting a single 3.7V 18650 cell directly to the 5V pin of an STM32 or ESP32.
```

### Why This Fails:
Onboard linear voltage regulators (like the AMS1117-3.3) have a **dropout voltage of $1.1\text{V}$**.
* A nominal 1S Li-ion battery sits at $3.7\text{V}$.
* Subtracting the $1.1\text{V}$ dropout: $3.7\text{V} - 1.1\text{V} = \mathbf{2.6\text{V}}$!
* The instant the radio transmitter draws a $100\text{mA}$ transmission burst, the voltage sags below $2.5\text{V}$, crashing the microcontroller and dropping the radio link.
* **The Fix:** Always wire a **mini MT3608 boost converter (₹35)** between the 1S battery and the microcontroller, adjusted to deliver a rock-solid **$5.05\text{V}$**!

---

## 🚫 Trap 4: The Reverse Battery Polarity Trap (Instant Silicon Death)

```
❌ DO NOT DO THIS: Plugging red (+) to black (-) even for a fraction of a second.
```

### Why This Fails:
LiPo batteries can deliver over $50\text{ Amperes}$ of peak short-circuit current. If you accidentally touch positive to negative or reverse the power leads to a BTS7960 driver or ESP32, the silicon chips will vaporize with a pop in less than **1 millisecond**.
* **The Fix:**
  1. Always use keyed connectors like **XT60** that physically cannot be plugged in backwards.
  2. Use red heat-shrink for positive (+) and black for negative (-).
  3. Always test your power rails with a digital multimeter *before* plugging in expensive microcontrollers!

---

## 🚫 Trap 5: The "Expensive Parts" & 3D Printer Trap

```
❌ DO NOT DO THIS: Thinking you must buy a ₹10,000 RadioMaster transmitter or ₹30,000 3D printer.
```

### Why This Fails:
Many builders waste months saving up money for factory parts. 
* 3D printed plastic frames often crack when dropped from a 2-foot height.
* High-end hobby transmitters (ELRS / FrSky) cost ₹8,000–₹12,000 and don't teach you embedded programming.
* **The Fix:** 6mm scrap plywood and 2mm MDF cost almost nothing, can be cut in 10 minutes with a handsaw, and are **much stronger** than 3D-printed plastic!

---

## 🚫 Trap 6: The Premature Complexity Trap

```
❌ DO NOT DO THIS: Trying to add AI computer vision, LiDAR, robotic arms, and GPS before the base car drives!
```

### Why This Fails:
Trying to build a 10-layer robot all at once leads to massive troubleshooting headaches. When it doesn't move, you won't know if the issue is in the LiDAR, the camera, the radio, or the motor driver.
* **The Fix: Follow the Golden Rule of Robotics: Staged Verification.**
  * Stage 1: Get the 4 motors spinning with a simple battery.
  * Stage 2: Connect the motor driver and test basic forward/reverse.
  * Stage 3: Establish the 50 Hz wireless link and calibrate joysticks.
  * Stage 4: Add sensors, displays, and advanced autonomy once the drivetrain is bulletproof!

---

## 🚫 Trap 7: Landing Full-Throttle on Ramp Jumps

```
❌ DO NOT DO THIS: Holding full throttle on the joystick while the rover is flying in mid-air.
```

### Why This Fails:
While in the air, the wheels spin freely at 630 RPM with zero resistance. The instant they hit concrete, the entire inertia of the wheels and ground friction pushes back against the motor shaft, which can strip the brass spur gears inside the gearbox.
* **The Fix:** Release the throttle stick to neutral right before the wheels hit the ground! Landing in neutral allows the gearboxes to absorb the ground impact smoothly.
