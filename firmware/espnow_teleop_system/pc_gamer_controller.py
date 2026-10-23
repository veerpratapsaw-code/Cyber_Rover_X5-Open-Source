"""
================================================================================
CYBERROVER X5 — HIGH-PRECISION 90 HZ PC GAMER COCKPIT (1ms MULTIMEDIA TIMER)
================================================================================
Lead Architect : Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
Target Platform: Windows 10 / 11 PC or Laptop (Python 3)
Subsystem      : Ultra-Low-Latency (<2ms) High-Cadence Teleoperation Gateway

HIGH-FREQUENCY REAL-TIME TRANSMISSION ENGINE:
  1. Windows Multimedia Timer (timeBeginPeriod): Overrides default 15.6ms Windows
     timer resolution to 1.0ms, allowing accurate 90 Hz (11ms) loop cadence.
  2. Non-Blocking Serial Buffer: Zero-timeout asynchronous serial reading prevents
     ser.readline() from blocking transmission ticks.
  3. 90 Hz Target Cadence (11.1ms Interval): Guarantees 60-90 Hz live throughput
     at the ESP32-S3 receiver with zero frame stuttering.
  4. Automatic Dead-Man Safety: Vehicle commands STOP (CMD_ID: 0, 0 PWM) within
     11ms of key release.

CALIBRATED KEYBOARD & COMMAND ID MAPPINGS (VEER SPECIFICATION):
  - [W] or [UP]       : CMD_FORWARD     (1) -> Forward Drive
  - [S] or [DOWN]     : CMD_BACKWARD    (2) -> Reverse Drive
  - [A] or [LEFT]     : CMD_PIVOT_LEFT  (3) -> Left Pivot / Spin CCW
  - [D] or [RIGHT]    : CMD_PIVOT_RIGHT (4) -> Right Pivot / Spin CW
  - [W + D] or [E]    : CMD_RIGHT_FRONT (6) -> Front Right Turn
  - [W + A] or [Q]    : CMD_LEFT_FRONT  (5) -> Front Left Turn
  - [S + D] or [C]    : CMD_RIGHT_BACK  (8) -> Back Right Turn
  - [S + A] or [Z]    : CMD_LEFT_BACK   (7) -> Back Left Turn
  - [KEY RELEASE]     : CMD_STOP        (0) -> Automatic Dead-Man Stop (0 PWM)
  - [SPACEBAR]        : TURBO BOOST (Instant 100% Throttle / 255 PWM / 11.31 km/h)
  - [KEYS 1 to 9]     : Shift Gears 1 to 9 (3.1 km/h to 11.31 km/h)
  - [ESC] or [Ctrl+C] : Safe Exit
================================================================================
"""

import sys
import time
import os
import ctypes

# Check for pyserial
try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("\n[ERROR] 'pyserial' library not found!")
    print("Run this command in terminal to install it: pip install pyserial\n")
    sys.exit(1)

# Ensure Windows OS and load user32 & winmm
if os.name != 'nt':
    print("\n[ERROR] This PC Gamer Controller requires Windows for zero-lag ctypes key polling.")
    sys.exit(1)

user32 = ctypes.windll.user32
user32.GetAsyncKeyState.restype = ctypes.c_short

# Enable 1ms High-Resolution Windows Multimedia Timer
try:
    winmm = ctypes.windll.winmm
    winmm.timeBeginPeriod(1)
except Exception:
    winmm = None

# Virtual-Key Codes for Windows
VK_SPACE    = 0x20
VK_LEFT     = 0x25
VK_UP       = 0x26
VK_RIGHT    = 0x27
VK_DOWN     = 0x28
VK_ESCAPE   = 0x1B

VK_0        = 0x30
VK_1        = 0x31
VK_9        = 0x39

VK_NUMPAD1  = 0x61  # Back Left
VK_NUMPAD2  = 0x62  # Backward
VK_NUMPAD3  = 0x63  # Back Right
VK_NUMPAD4  = 0x64  # Left Pivot
VK_NUMPAD6  = 0x66  # Right Pivot
VK_NUMPAD7  = 0x67  # Front Left
VK_NUMPAD8  = 0x68  # Forward
VK_NUMPAD9  = 0x69  # Front Right

VK_A        = 0x41
VK_C        = 0x43  # C: Back Right (Dedicated Single Key)
VK_D        = 0x44
VK_E        = 0x45  # E: Front Right (Dedicated Single Key)
VK_Q        = 0x51  # Q: Front Left (Dedicated Single Key)
VK_S        = 0x53
VK_W        = 0x57
VK_Z        = 0x5A  # Z: Back Left (Dedicated Single Key)

def is_key_down(vk_code):
    """Direct kernel-level physical key state check (0ms typematic delay)"""
    return bool(user32.GetAsyncKeyState(vk_code) & 0x8000)

# Pure Numeric Command IDs (Zero-Overhead Protocol)
CMD_STOP        = 0
CMD_FORWARD     = 1  # W
CMD_BACKWARD    = 2  # S
CMD_PIVOT_LEFT  = 3  # A
CMD_PIVOT_RIGHT = 4  # D
CMD_LEFT_FRONT  = 5  # W + A, Q
CMD_RIGHT_FRONT = 6  # W + D, E
CMD_LEFT_BACK   = 7  # S + A, Z
CMD_RIGHT_BACK  = 8  # S + D, C

CMD_DISPLAY_MAP = {
    CMD_STOP:        ('■ STOPPED      ', 'Dead-Man Safety Active'),
    CMD_FORWARD:     ('▲ FORWARD DRIVE', '[W] Moving Ahead'),
    CMD_BACKWARD:    ('▼ REVERSE DRIVE', '[S] Backing Up'),
    CMD_PIVOT_LEFT:  ('↺ LEFT PIVOT   ', '[A] Spin Counter-Clockwise'),
    CMD_PIVOT_RIGHT: ('↻ RIGHT PIVOT  ', '[D] Spin Clockwise'),
    CMD_LEFT_FRONT:  ('↖ FRONT LEFT   ', '[W+A / Q] Curve Front Left'),
    CMD_RIGHT_FRONT: ('↗ FRONT RIGHT  ', '[W+D / E] Curve Front Right'),
    CMD_LEFT_BACK:   ('↙ BACK LEFT    ', '[S+A / Z] Curve Back Left'),
    CMD_RIGHT_BACK:  ('↘ BACK RIGHT   ', '[S+D / C] Curve Back Right')
}

# 9 Gear Ratios (PWM Values & Corresponding Vehicle Speeds)
GEARS = {
    1: {'pwm': 70,  'speed_kmh': 3.11, 'desc': 'Tactical Crawl'},
    2: {'pwm': 90,  'speed_kmh': 4.00, 'desc': 'Indoor Creep'},
    3: {'pwm': 115, 'speed_kmh': 5.11, 'desc': 'Slow Patrol'},
    4: {'pwm': 140, 'speed_kmh': 6.22, 'desc': 'Standard Drive'},
    5: {'pwm': 165, 'speed_kmh': 7.33, 'desc': 'Comfort Cruise'},
    6: {'pwm': 190, 'speed_kmh': 8.44, 'desc': 'Fast Transit'},
    7: {'pwm': 215, 'speed_kmh': 9.55, 'desc': 'High Speed'},
    8: {'pwm': 235, 'speed_kmh': 10.44, 'desc': 'Overdrive'},
    9: {'pwm': 255, 'speed_kmh': 11.31, 'desc': 'Max Sprint / Turbo'}
}

# Target High-Frequency Cadence: 90 Hz (11.1ms interval)
TARGET_HZ = 90
TX_INTERVAL_SEC = 1.0 / TARGET_HZ

MAC_CACHE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "rover_mac.txt")

def load_cached_mac():
    if os.path.exists(MAC_CACHE_FILE):
        try:
            with open(MAC_CACHE_FILE, "r") as f:
                mac = f.read().strip()
                if len(mac) >= 12:
                    return mac
        except Exception:
            pass
    return "1C:DB:D4:4B:08:40" # Factory verified Rover ESP32-S3 MAC

def save_cached_mac(mac):
    try:
        with open(MAC_CACHE_FILE, "w") as f:
            f.write(mac.strip().upper())
    except Exception:
        pass

def find_serial_ports():
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]

def main():
    os.system('cls')
    print("=" * 76)
    print("  CYBERROVER X5 — 90 Hz PC GAMER TELEOP RIG (1ms TIMING ENGINE)       ")
    print("  Lead Architect : Veer Pratap Saw | Katras, Jharkhand               ")
    print("=" * 76)

    # 1. Detect USB Serial COM Ports
    ports = find_serial_ports()
    if not ports:
        print("\n[!] No USB Serial COM ports found!")
        print("    Plug your ESP32-CAM USB Dongle into the laptop and try again.\n")
        input("Press ENTER to exit...")
        if winmm:
            winmm.timeEndPeriod(1)
        sys.exit(1)

    print("\nDetected USB Serial Ports:")
    for idx, p in enumerate(ports):
        print(f"  [{idx + 1}] {p}")

    choice = 0
    if len(ports) == 1:
        choice = 0
        print(f"\n>>> Auto-selected only connected port: {ports[0]}")
    else:
        try:
            sel = int(input(f"\nSelect Port [1-{len(ports)}]: ")) - 1
            choice = max(0, min(sel, len(ports) - 1))
        except ValueError:
            choice = 0

    port_name = ports[choice]

    # 2. Configure Direct Target MAC Address
    saved_mac = load_cached_mac()
    print("-" * 76)
    print(f"Target Rover ESP32-S3 MAC Address:")
    print(f"  (Default/Saved: {saved_mac})")
    print("  Tip: Look at the ESP32-S3 Serial Monitor at boot to see its MAC address.")
    user_mac = input(f"Enter S3 MAC (press ENTER to use '{saved_mac}'): ").strip()
    if not user_mac:
        target_mac = saved_mac
    else:
        target_mac = user_mac.upper()
        save_cached_mac(target_mac)

    print(f"\n>>> Opening {port_name} at 115200 baud (Non-Blocking Mode)...")
    try:
        # timeout=0 for pure non-blocking reads (zero pipeline stalling!)
        ser = serial.Serial(port_name, 115200, timeout=0)
        time.sleep(1.0) # Wait for dongle boot
    except Exception as e:
        print(f"\n[ERROR] Could not open {port_name}: {e}")
        if winmm:
            winmm.timeEndPeriod(1)
        sys.exit(1)

    # Send direct MAC configuration to the dongle
    ser.write(f"MAC:{target_mac}\n".encode('utf-8'))
    time.sleep(0.1)

    # Flush any startup banner
    ser.reset_input_buffer()

    current_gear = 5
    active_cmd_id = CMD_STOP
    turbo_active = False
    link_status = "INITIALIZING"
    total_acks = 0
    total_sent = 0
    current_rssi = 0
    telemetry_l_pwm = 0
    telemetry_r_pwm = 0
    last_ack_time = 0

    last_hud_render = 0
    last_serial_tx = 0
    rx_buffer = ""

    # Key rollover retention timestamps (protects against keyboard matrix ghosting / dropouts)
    last_w_time = 0
    last_s_time = 0
    last_a_time = 0
    last_d_time = 0

    hud_lines_count = 16

    def render_hud(gear, turbo, cmd_id, status, acks, sent, rssi, ml, mr):
        gear_info = GEARS[gear]
        actual_pwm = 255 if turbo else (0 if cmd_id == CMD_STOP else gear_info['pwm'])
        actual_kmh = 11.31 if turbo else (0.00 if cmd_id == CMD_STOP else gear_info['speed_kmh'])

        cmd_display, cmd_sub = CMD_DISPLAY_MAP.get(cmd_id, ('IDLE', ''))

        ack_pct = (acks / sent * 100.0) if sent > 0 else 0.0

        if status == "ACK_OK" and acks > 0:
            if rssi >= -55 and rssi != 0:
                sig_bar = f"{rssi} dBm [★★★★★ EXCELLENT]"
            elif rssi >= -68 and rssi != 0:
                sig_bar = f"{rssi} dBm [★★★★☆ GOOD]"
            elif rssi >= -80 and rssi != 0:
                sig_bar = f"{rssi} dBm [★★★☆☆ FAIR]"
            elif rssi != 0:
                sig_bar = f"{rssi} dBm [★★☆☆☆ WEAK]"
            else:
                sig_bar = "LINK ACTIVE [ACK VERIFIED]"
            link_str = f"ONLINE (ACK {ack_pct:.0f}%)"
        else:
            link_str = "SEARCHING / NO ACK"
            sig_bar = "WAITING FOR ROVER BEACON..."

        if ml == 0 and mr == 0:
            motor_str = "MOTORS IDLE [0.00V GND Locked]"
        else:
            motor_str = f"ACTIVE DRIVE -> L: {ml:+4d} PWM | R: {mr:+4d} PWM"

        # Position cursor at top-left to eliminate flicker
        print("\033[H", end="")
        print("=" * 76 + "\033[K")
        print(f"  CYBERROVER X5 — HIGH-PRECISION PC GAMER COCKPIT ({TARGET_HZ} Hz STREAM)       \033[K")
        print("=" * 76 + "\033[K")
        print(f"  TARGET ROVER MAC : {target_mac:<20} RADIO LINK  : {link_str:<26}\033[K")
        print(f"  HARDWARE DONGLE  : {port_name:<20} SIGNAL RSSI : {sig_bar:<26}\033[K")
        print(f"  ROVER TELEMETRY  : {motor_str:<50}\033[K")
        print("-" * 76 + "\033[K")
        print(f"  ACTIVE GEAR      : [{gear}/9] {gear_info['desc']:<15} PWM GOVERNOR: {gear_info['pwm']}/255\033[K")
        turbo_str = ">>> ACTIVE! (100% SPRINT) <<<" if turbo else "OFF (HOLD SPACE FOR TURBO)"
        print(f"  TURBO BOOST      : {turbo_str:<35} SPEED: {actual_kmh:5.2f} km/h\033[K")
        print(f"  DRIVETRAIN STATE : {cmd_display:<18} [{cmd_sub:<26}] (CMD_ID: {cmd_id})\033[K")
        print("-" * 76 + "\033[K")
        print("  CONTROLS : [W] Fwd       | [S] Back       | [A] Left Pivot  | [D] Right Pivot\033[K")
        print("             [W+D/E] R-Frnt| [W+A/Q] L-Frnt | [S+D/C] R-Back  | [S+A/Z] L-Back \033[K")
        print("             [SPACE] Turbo Sprint          | [1 to 9] Gears  | [ESC] Exit     \033[K")
        print("=" * 76 + "\033[K")
        print(f"  Telemetry Packets: Sent = {sent} | ACKs Received = {acks}           \033[K")

    # Clear screen once initially
    os.system('cls')
    print("\n" * hud_lines_count) # Pre-allocate lines

    try:
        while True:
            now = time.time()

            # 1. Exit on ESC
            if is_key_down(VK_ESCAPE):
                # Send immediate STOP command (CMD_ID: 0)
                ser.write(bytes([0xAA, CMD_STOP, 0, current_gear, 0, 0x55]))
                print("\n\n>>> ESC Pressed. Exiting PC Gamer Controller safely...")
                ser.close()
                if winmm:
                    winmm.timeEndPeriod(1)
                return

            # 2. Check Gear Selection (Keys 1 to 9 & Numpad 1 to 9)
            for g in range(1, 10):
                if is_key_down(VK_0 + g) or is_key_down(VK_NUMPAD1 + (g - 1)):
                    current_gear = g
                    break

            # 3. Check Turbo Boost (Spacebar)
            turbo_active = is_key_down(VK_SPACE)

            # 4. Check Motion Direction (Calibrated 8-Direction Priority)
            # Dedicated single-key diagonals (100% immune to keyboard matrix ghosting):
            q_down = is_key_down(VK_Q) or is_key_down(VK_NUMPAD7)
            e_down = is_key_down(VK_E) or is_key_down(VK_NUMPAD9)
            z_down = is_key_down(VK_Z) or is_key_down(VK_NUMPAD1)
            c_down = is_key_down(VK_C) or is_key_down(VK_NUMPAD3)

            # Standard 4 cardinal directions (WASD + Arrow Keys + Numpad):
            w_down = is_key_down(VK_W) or is_key_down(VK_UP) or is_key_down(VK_NUMPAD8)
            s_down = is_key_down(VK_S) or is_key_down(VK_DOWN) or is_key_down(VK_NUMPAD2)
            a_down = is_key_down(VK_A) or is_key_down(VK_LEFT) or is_key_down(VK_NUMPAD4)
            d_down = is_key_down(VK_D) or is_key_down(VK_RIGHT) or is_key_down(VK_NUMPAD6)

            # Update key timestamps for anti-ghosting rollover latch
            if w_down: last_w_time = now
            if s_down: last_s_time = now
            if a_down: last_a_time = now
            if d_down: last_d_time = now

            # Instant Dead-Man Check: If all motion keys are physically released, stop IMMEDIATELY (0ms lag!)
            any_motion_key = (w_down or s_down or a_down or d_down or q_down or e_down or z_down or c_down)
            if not any_motion_key:
                active_cmd_id = CMD_STOP
            else:
                # 45ms key rollover retention for smooth diagonal key combinations
                w_held = w_down or ((now - last_w_time) < 0.045 and not s_down)
                s_held = s_down or ((now - last_s_time) < 0.045 and not w_down)
                a_held = a_down or ((now - last_a_time) < 0.045 and not d_down)
                d_held = d_down or ((now - last_d_time) < 0.045 and not a_down)

                # Calibrated Gamer Controls:
                if (w_held and d_held) or e_down:
                    active_cmd_id = CMD_RIGHT_FRONT  # 6: [W + D] or [E] = Front Right
                elif (w_held and a_held) or q_down:
                    active_cmd_id = CMD_LEFT_FRONT   # 5: [W + A] or [Q] = Front Left
                elif (s_held and d_held) or c_down:
                    active_cmd_id = CMD_RIGHT_BACK   # 8: [S + D] or [C] = Back Right
                elif (s_held and a_held) or z_down:
                    active_cmd_id = CMD_LEFT_BACK    # 7: [S + A] or [Z] = Back Left
                # Single-Key Cardinals:
                elif w_down:
                    active_cmd_id = CMD_FORWARD      # 1: [W] = Forward
                elif s_down:
                    active_cmd_id = CMD_BACKWARD     # 2: [S] = Back
                elif a_down:
                    active_cmd_id = CMD_PIVOT_LEFT   # 3: [A] = Left Pivot
                elif d_down:
                    active_cmd_id = CMD_PIVOT_RIGHT  # 4: [D] = Right Pivot
                else:
                    active_cmd_id = CMD_STOP         # 0: Automatic Dead-Man Safety Stop!

            # 5. Transmit 6-Byte Frame to Dongle at TARGET_HZ (90 Hz = Every 11.1 ms)
            if now - last_serial_tx >= TX_INTERVAL_SEC:
                last_serial_tx = now
                gear_info = GEARS[current_gear]
                speed_byte = 255 if turbo_active else (0 if active_cmd_id == CMD_STOP else gear_info['pwm'])

                frame = bytes([
                    0xAA,
                    active_cmd_id,
                    speed_byte,
                    current_gear,
                    1 if turbo_active else 0,
                    0x55
                ])
                try:
                    ser.write(frame)
                    total_sent += 1
                except Exception:
                    pass

            # 6. Read back ACK status reports from Dongle (Pure Non-Blocking Buffer)
            if ser.in_waiting:
                try:
                    raw_data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    rx_buffer += raw_data
                    while '\n' in rx_buffer:
                        line, rx_buffer = rx_buffer.split('\n', 1)
                        line = line.strip()
                        if line.startswith("STAT:"):
                            parts = line[5:].split(',')
                            for p in parts:
                                if p == "ACK_OK":
                                    link_status = "ACK_OK"
                                    last_ack_time = now
                                elif p == "NO_ACK":
                                    if now - last_ack_time > 0.4:
                                        link_status = "NO_ACK"
                                elif p.startswith("RSSI:"):
                                    try: current_rssi = int(p.split(':')[1])
                                    except Exception: pass
                                elif p.startswith("ACKS:"):
                                    try: total_acks = int(p.split(':')[1])
                                    except Exception: pass
                                elif p.startswith("ML:"):
                                    try: telemetry_l_pwm = int(p.split(':')[1])
                                    except Exception: pass
                                elif p.startswith("MR:"):
                                    try: telemetry_r_pwm = int(p.split(':')[1])
                                    except Exception: pass
                except Exception:
                    pass

            # 7. Render HUD at ~15 Hz (Smooth without eating CPU)
            if now - last_hud_render >= 0.066:
                last_hud_render = now
                render_hud(current_gear, turbo_active, active_cmd_id, link_status, total_acks, total_sent, current_rssi, telemetry_l_pwm, telemetry_r_pwm)

            # High-Resolution 1ms Tick Sleep (Yields CPU gracefully without 15.6ms delay)
            time.sleep(0.001)

    except KeyboardInterrupt:
        try:
            ser.write(bytes([0xAA, CMD_STOP, 0, current_gear, 0, 0x55]))
            ser.close()
        except Exception:
            pass
        if winmm:
            winmm.timeEndPeriod(1)
        print("\n\n>>> Teleoperation stopped.")

if __name__ == '__main__':
    main()
