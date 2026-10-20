# 🎛️ CYBERROVER X5 — NOISE FILTERING & CALIBRATION GUIDE (FIRMWARE v1.5)

**Document Classification:** Signal Processing & Sensor Calibration Manual  
**Lead Architect & Developer:** Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh, Jharkhand  

---

## 1. Analog Acquisition Pipeline Overview

The STM32F103 SAR ADC possesses high 12-bit resolution ($0 - 4095$), but bench testing reveals that breadboards, jumper wires, and switching regulators (MT3608) introduce $\pm 15$ to $\pm 35$ counts of peak-to-peak high-frequency ripple. 

To eliminate jitter without introducing control latency, CyberRover X5 applies a **3-stage conditioning pipeline**:

```mermaid
graph LR
    RAW[Raw 12-Bit ADC 0-4095] --> M[Stage 1: Order-3 Median Filter]
    M --> IIR[Stage 2: Single-Pole Low-Pass IIR]
    IIR --> DB[Stage 3: Zero-Discontinuity Deadband]
    DB --> OUT[Conditioned Output & Normalized Control]
```

---

## 2. Mathematical Filter Formulation

### Stage 1: Order-3 Moving Window Median Filter
Given consecutive samples $x[n-2], x[n-1], x[n]$:
$$y_{\text{med}}[n] = \text{median}(x[n-2], x[n-1], x[n])$$

* **Property:** An isolated impulsive spike (e.g. from an ESD discharge or motor switching transient) has a duration of 1 sample. The median filter completely eliminates it, yielding zero spike propagation.
* **Latency:** $1\text{ sample}$ ($25\text{ ms}$ at $40\text{ Hz}$).

### Stage 2: Single-Pole Low-Pass IIR Filter
$$y[n] = y[n-1] + \alpha \cdot (y_{\text{med}}[n] - y[n-1])$$

* **Joystick Channels ($\alpha = 0.70$):**
  * Time constant: $\tau \approx \frac{\Delta t}{\alpha} \approx \frac{25\text{ ms}}{0.70} \approx 35\text{ ms}$.
  * Step response reaches $95\%$ within $75\text{ ms}$.
  * Preserves rapid evasive steering and high-speed throttle response without sluggish feel.
* **Potentiometer Channels ($\alpha = 0.25$):**
  * Heavy smoothing for rock-solid dial stability.

---

## 3. Zero-Discontinuity Deadband Normalization

Traditional deadbands clamp readings within $[-D, +D]$ to zero, but cause a sudden jump in vehicle speed when crossing the threshold:

$$\text{Naive Step:} \quad y = \begin{cases} 0 & |x| \le D \\ x & |x| > D \end{cases} \implies \text{Instantaneous step of } D \text{ counts!}$$

CyberRover X5 implements **smooth piecewise linear scaling**:

$$u(x) = \begin{cases} 
0.0 & \text{if } |x - C| \le D \\
\frac{(x - C) - D}{(4095 - M) - (C + D)} & \text{if } x > C + D \\
-\frac{(C - D) - x}{(C - D) - M} & \text{if } x < C - D 
\end{cases}$$

Where:
* $C = 2048$ (Nominal Center)
* $D = 140$ (Deadband half-width: readings between $1908$ and $2188$ output $0.0$)
* $M = 80$ (Mechanical gimbal stop margin, ensuring $100\%$ full throttle is reliably reached)

**Result:** Exiting the deadband begins continuously at $0.000$, guaranteeing zero vehicle jerk.

---

## 4. Potentiometer Boundary Hysteresis

Potentiometers partition the $0 - 4095$ range into $20\text{ discrete notches}$ ($\sim 204\text{ counts/notch}$). Electrical noise near a boundary causes the integer division to toggle repeatedly ($0 \leftrightarrow 1$), causing rapid, annoying audio-haptic clicks.

**Hysteresis Rule:**
A notch transition is permitted **only if**:
$$\text{candidateNotch} \ne \text{currentNotch} \quad \text{AND} \quad |\text{reading} - \text{lastAcceptedReading}| > 65\text{ counts}$$

This enforces a dead zone around every boundary, completely eliminating boundary chatter.
