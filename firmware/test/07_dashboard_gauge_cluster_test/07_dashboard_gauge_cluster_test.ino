/*
 * ===============================================================================
 * CYBERROVER X5 — STAGE 7: DUAL-GAUGE MOVING ARC DASHBOARD CLUSTER (PAGE 1)
 * ===============================================================================
 * Lead Architect: Veer Pratap Saw (Veer) | Class 10 Student, Katras / Ramgarh
 * Display Target: 1.3" I2C OLED (SH1106, 128x64) on Remote Controller Carrier
 *                 (Also fully compatible with 0.96" SSD1306 via top-level toggle)
 *
 * Visual Reference:
 *   Recreating upiir's digital automotive instrument cluster HUD identically:
 *   - Left Gauge : Curved moving arc for Vehicle Speed (0 - 314 cm/s / 11.3 km/h)
 *                  with outer dotted track + Left Blinker Arrow
 *   - Right Gauge: Curved moving arc for Motor RPM (0 - 600 RPM)
 *                  with outer dotted track + Right Blinker Arrow
 *   - Center Top : Coolant / Engine Temp icon + 3 decorative arch dots + Heartbeat
 *   - Center Mid : Dynamic Drive Mode (N / D / R) + Colon ':' + Power hatch bars
 *   - Center Btm : Fuel pump icon + Curved Fuel / Battery gauge arc (E <-> F)
 *   - Bottom Rgt : Authentic UPiR / X5 watermark signature
 *
 * Real-time Hardware Inputs (Native STM32 12-bit ADC: 0 - 4095):
 *   - Left Stick Y  (PB0) : Live Throttle input -> Drives Speed & RPM arcs
 *   - Left Stick X  (PB1) : Live Steering input -> Triggers Left/Right Blinkers
 *   - Pot Left      (PA0) : Battery / Fuel sweep simulation (Empty to Full)
 *   - I2C Bus       (PB6/PB7) : Hardware I2C at 400 kHz fast mode
 * ===============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ===============================================================================
// 1. HARDWARE DISPLAY CONFIGURATION
// ===============================================================================
// Set to 1 for Veer's 1.3" SH1106 OLED (Remote Controller)
// Set to 0 for 0.96" SSD1306 OLED (Rover Chassis)
#define USE_SH1106 1

#if USE_SH1106
  // U8g2 Full Framebuffer mode for SH1106 I2C (Fast 72 MHz ARM Cortex-M3)
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#else
  // U8g2 Full Framebuffer mode for SSD1306 I2C
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#endif

#define OLED_SCL_PIN  PB6
#define OLED_SDA_PIN  PB7

// ===============================================================================
// 2. PIN ASSIGNMENTS (MATCHING CYBERROVER X5 REMOTE CARRIER)
// ===============================================================================
#define PIN_STICK_LX    PB1   // Steering (Turn Blinkers)
#define PIN_STICK_LY    PB0   // Throttle (Speed & RPM)
#define PIN_STICK_RX    PA4   // Right Stick X
#define PIN_STICK_RY    PA3   // Right Stick Y
#define PIN_POT_LEFT    PA0   // Pot Left (Fuel / Battery sweep test)
#define PIN_POT_CENTER  PA1   // Pot Center (Camera Tilt)
#define PIN_POT_RIGHT   PA2   // Pot Right (Speed Governor)

// Deadband and calibration values
#define ADC_CENTER      2048
#define ADC_DEADBAND    140   // Deadband window: 1908 to 2188 reads zero

// Mechanical Rover Limits (Johnson 12V 600 RPM motors + 100mm wheels)
#define MAX_SPEED_CMPS  314   // 314 cm/s = 11.31 km/h top sprint
#define MAX_MOTOR_RPM   600   // 600 RPM gearhead output

// ===============================================================================
// 3. BITMAP GRAPHICS ASSETS (AUTHENTIC UPIIR DASHBOARD STYLE)
// ===============================================================================

// Left Turn Blinker Arrow (8x7 px)
static const unsigned char PROGMEM bitmap_arrow_left[] = {
  0x10, // ...█....
  0x30, // ..██....
  0x70, // .███....
  0xFF, // ████████
  0x70, // .███....
  0x30, // ..██....
  0x10  // ...█....
};

// Right Turn Blinker Arrow (8x7 px)
static const unsigned char PROGMEM bitmap_arrow_right[] = {
  0x08, // ....█...
  0x0C, // ....██..
  0x0E, // ....███.
  0xFF, // ████████
  0x0E, // ....███.
  0x0C, // ....██..
  0x08  // ....█...
};

// Coolant / Engine Temperature Thermometer Icon (9x9 px)
static const unsigned char PROGMEM bitmap_temp_icon[] = {
  0x10, 0x00, // ...█.....
  0x10, 0x00, // ...█.....
  0x10, 0x00, // ...█.....
  0x38, 0x00, // ..███....
  0x38, 0x00, // ..███....
  0x54, 0x00, // .█.█.█...
  0xFE, 0x00, // ███████..
  0xFE, 0x00, // ███████..
  0x7C, 0x00  // .█████...
};

// Gas Pump / Fuel Icon (8x8 px)
static const unsigned char PROGMEM bitmap_fuel_icon[] = {
  0x70, // .███....
  0x5A, // .█.██.█.
  0x7A, // .████.█.
  0x5E, // .█.████.
  0x52, // .█.█..█.
  0x72, // .███..█.
  0x70, // .███....
  0xFE  // ███████.
};

// Authentic UPiR Logo Watermark (16x4 px)
static const unsigned char PROGMEM bitmap_upir_logo[] = {
  0x15, 0xD7, // B00010101, B11010111
  0x15, 0x45, // B00010101, B01000101
  0x15, 0x96, // B00010101, B10010110
  0x19, 0x15  // B00011001, B00010101
};

// ===============================================================================
// 4. FILTERING & SMOOTHING STATE VARIABLES
// ===============================================================================
float speed_smooth = 0.0f;
float rpm_smooth   = 0.0f;
float fuel_smooth  = 85.0f;

uint32_t last_oled_draw_ms = 0;
const uint16_t OLED_FRAME_INTERVAL_MS = 33; // ~30 FPS smooth animation

// ===============================================================================
// 5. MATH HELPER: DRAW SOLID THICK CIRCULAR ARCS
// ===============================================================================
// Draws an anti-aliased solid arc between start_deg and end_deg.
// Angles in degrees: 0 = East (+X), 90 = North (-Y), 180 = West (-X), 270 = South (+Y).
void drawThickArc(int cx, int cy, int r_in, int r_out, float start_deg, float end_deg) {
  if (start_deg > end_deg) {
    float temp = start_deg;
    start_deg = end_deg;
    end_deg = temp;
  }

  // Draw radial lines at 1.0 degree increments (smooth fill without gaps)
  for (float a = start_deg; a <= end_deg; a += 1.0f) {
    float rad = a * 0.0174532925f; // deg to rad
    float cos_a = cos(rad);
    float sin_a = sin(rad);

    int x1 = cx + (int)(r_in  * cos_a + 0.5f);
    int y1 = cy - (int)(r_in  * sin_a + 0.5f);
    int x2 = cx + (int)(r_out * cos_a + 0.5f);
    int y2 = cy - (int)(r_out * sin_a + 0.5f);

    u8g2.drawLine(x1, y1, x2, y2);
  }
}

// Draws the outer track tick marks and dots around the gauge dial
void drawDialTrack(int cx, int cy, int r_track, float start_deg, float end_deg) {
  for (float a = start_deg; a <= end_deg; a += 10.0f) {
    float rad = a * 0.0174532925f;
    float cos_a = cos(rad);
    float sin_a = sin(rad);

    // Major tick every 30 degrees
    int step_idx = (int)((a - start_deg) / 10.0f + 0.5f);
    if (step_idx % 3 == 0) {
      int x_t1 = cx + (int)((r_track - 1) * cos_a + 0.5f);
      int y_t1 = cy - (int)((r_track - 1) * sin_a + 0.5f);
      int x_t2 = cx + (int)((r_track + 2) * cos_a + 0.5f);
      int y_t2 = cy - (int)((r_track + 2) * sin_a + 0.5f);
      u8g2.drawLine(x_t1, y_t1, x_t2, y_t2);
    } else {
      // Small dotted track
      int x_dot = cx + (int)(r_track * cos_a + 0.5f);
      int y_dot = cy - (int)(r_track * sin_a + 0.5f);
      u8g2.drawPixel(x_dot, y_dot);
    }
  }
}

// ===============================================================================
// 6. SETUP ROUTINE
// ===============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n======================================================="));
  Serial.println(F("  CYBERROVER X5 — DUAL-GAUGE INSTRUMENT CLUSTER (PAGE 1)"));
  Serial.println(F("  Architecture: STM32F103C8T6 | U8g2 Full Framebuffer   "));
  Serial.println(F("======================================================="));

  // Configure STM32 12-bit ADC (0 - 4095)
  #if defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__)
    analogReadResolution(12);
    Wire.setSCL(OLED_SCL_PIN);
    Wire.setSDA(OLED_SDA_PIN);
  #endif

  Wire.begin();
  Wire.setClock(400000); // 400 kHz Fast I2C for 40+ FPS refresh

  // Initialize U8g2
  u8g2.begin();
  u8g2.setContrast(255); // Maximum crisp OLED contrast
  u8g2.clearBuffer();

  // Startup Splash Indicator
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(16, 28, "CYBERROVER X5");
  u8g2.drawStr(24, 42, "INITIALIZING...");
  u8g2.sendBuffer();
  delay(600);

  Serial.println(F("[STATUS] U8g2 Cluster Initialized. Starting Live Loop..."));
}

// ===============================================================================
// 7. MAIN LOOP & INSTRUMENT CLUSTER RENDERING
// ===============================================================================
void loop() {
  uint32_t current_ms = millis();

  // -----------------------------------------------------------------------------
  // A. READ LIVE SENSORS & ANALOG GIMBALS (12-bit: 0 - 4095)
  // -----------------------------------------------------------------------------
  int raw_ly  = analogRead(PIN_STICK_LY);  // Throttle
  int raw_lx  = analogRead(PIN_STICK_LX);  // Steering
  int raw_pot = analogRead(PIN_POT_LEFT);  // Pot Left (Fuel level test)

  // Calculate target speed and RPM from Throttle Stick (PB0)
  float target_speed = 0.0f;
  float target_rpm   = 0.0f;
  char  drive_mode   = 'N'; // N = Neutral, D = Drive, R = Reverse

  if (raw_ly > (ADC_CENTER + ADC_DEADBAND)) {
    // Pushing FORWARD -> Drive Mode 'D'
    drive_mode = 'D';
    float ratio = (float)(raw_ly - (ADC_CENTER + ADC_DEADBAND)) / (4095.0f - (ADC_CENTER + ADC_DEADBAND));
    if (ratio > 1.0f) ratio = 1.0f;
    target_speed = ratio * MAX_SPEED_CMPS;
    target_rpm   = ratio * MAX_MOTOR_RPM;
  } else if (raw_ly < (ADC_CENTER - ADC_DEADBAND)) {
    // Pulling REVERSE -> Reverse Mode 'R'
    drive_mode = 'R';
    float ratio = (float)((ADC_CENTER - ADC_DEADBAND) - raw_ly) / (ADC_CENTER - ADC_DEADBAND);
    if (ratio > 1.0f) ratio = 1.0f;
    target_speed = ratio * (MAX_SPEED_CMPS * 0.75f); // Reverse speed capped at 75%
    target_rpm   = ratio * (MAX_MOTOR_RPM * 0.75f);
  } else {
    // Inside Deadband -> Neutral 'N'
    drive_mode = 'N';
    target_speed = 0.0f;
    target_rpm   = 0.0f;
  }

  // Calculate simulated Fuel / Battery percentage from Pot Left (PA0)
  float target_fuel = ((float)raw_pot / 4095.0f) * 100.0f;
  if (target_fuel < 0.0f) target_fuel = 0.0f;
  if (target_fuel > 100.0f) target_fuel = 100.0f;

  // Exponential Moving Average (EMA) for buttery-smooth gauge motion
  speed_smooth += (target_speed - speed_smooth) * 0.22f;
  rpm_smooth   += (target_rpm   - rpm_smooth)   * 0.22f;
  fuel_smooth  += (target_fuel  - fuel_smooth)  * 0.15f;

  // Turn signal state (Active when steering left or right)
  bool left_blinker_active  = (raw_lx < (ADC_CENTER - 500));
  bool right_blinker_active = (raw_lx > (ADC_CENTER + 500));
  bool blink_state = ((current_ms / 250) % 2 == 0); // 2 Hz flash cadence

  // Heartbeat pulse dot (500 ms period)
  bool heartbeat_state = ((current_ms / 300) % 2 == 0);

  // -----------------------------------------------------------------------------
  // B. RENDER FRAME AT 30 FPS NON-BLOCKING
  // -----------------------------------------------------------------------------
  if (current_ms - last_oled_draw_ms >= OLED_FRAME_INTERVAL_MS) {
    last_oled_draw_ms = current_ms;

    u8g2.clearBuffer();

    // ===========================================================================
    // 1. LEFT GAUGE CLUSTER: SPEEDOMETER (cm/s)
    // ===========================================================================
    // Geometry: Center (30, 34), Outer Radius = 25, Inner Radius = 21 (Thickness = 4px)
    const int cx_l = 30;
    const int cy_l = 34;
    const int r_in_l = 21;
    const int r_out_l = 24;
    const int r_track_l = 26;

    // Outer Track: Sweeps clockwise from 215 deg (bottom-left) to 35 deg (top-right)
    drawDialTrack(cx_l, cy_l, r_track_l, 35.0f, 215.0f);

    // Active Moving Arc:
    float speed_frac = speed_smooth / (float)MAX_SPEED_CMPS;
    if (speed_frac > 1.0f) speed_frac = 1.0f;
    if (speed_frac < 0.02f) speed_frac = 0.02f; // Minimum baseline stub

    // Arc fills from 215 deg downwards towards 35 deg (215 - frac * 180)
    float speed_current_deg = 215.0f - (speed_frac * 180.0f);
    drawThickArc(cx_l, cy_l, r_in_l, r_out_l, speed_current_deg, 215.0f);

    // Speed Digital Readout (Modern Tall Digital Digits)
    int display_speed_val = (int)(speed_smooth + 0.5f);
    char speed_str[8];
    itoa(display_speed_val, speed_str, 10);

    u8g2.setFont(u8g2_font_logisoso16_tn);
    int speed_str_w = u8g2.getStrWidth(speed_str);
    int speed_x = cx_l - (speed_str_w / 2);
    u8g2.drawStr(speed_x, 40, speed_str);

    // Speed Unit Sub-label
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(cx_l - 8, 48, "CM/S");

    // Left Turn Blinker Arrow (Top-Left)
    if (left_blinker_active && blink_state) {
      u8g2.drawXBMP(4, 3, 8, 7, bitmap_arrow_left);
    } else {
      u8g2.drawPixel(4, 6);
      u8g2.drawPixel(11, 6);
    }

    // ===========================================================================
    // 2. RIGHT GAUGE CLUSTER: MOTOR TACHOMETER (RPM)
    // ===========================================================================
    // Geometry: Mirrored Center (98, 34), Outer Radius = 25, Inner Radius = 21
    const int cx_r = 98;
    const int cy_r = 34;
    const int r_in_r = 21;
    const int r_out_r = 24;
    const int r_track_r = 26;

    // Outer Track: Sweeps from -35 deg (325 deg bottom-right) up to 145 deg (top-left)
    drawDialTrack(cx_r, cy_r, r_track_r, -35.0f, 145.0f);

    // Active Moving Arc:
    float rpm_frac = rpm_smooth / (float)MAX_MOTOR_RPM;
    if (rpm_frac > 1.0f) rpm_frac = 1.0f;
    if (rpm_frac < 0.02f) rpm_frac = 0.02f; // Minimum baseline stub

    // Arc fills from -35 deg upwards towards 145 deg (-35 + frac * 180)
    float rpm_current_deg = -35.0f + (rpm_frac * 180.0f);
    drawThickArc(cx_r, cy_r, r_in_r, r_out_r, -35.0f, rpm_current_deg);

    // RPM Digital Readout (Modern Tall Digital Digits)
    int display_rpm_val = (int)(rpm_smooth + 0.5f);
    char rpm_str[8];
    itoa(display_rpm_val, rpm_str, 10);

    u8g2.setFont(u8g2_font_logisoso16_tn);
    int rpm_str_w = u8g2.getStrWidth(rpm_str);
    int rpm_x = cx_r - (rpm_str_w / 2);
    u8g2.drawStr(rpm_x, 40, rpm_str);

    // RPM Unit Sub-label
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(cx_r - 6, 48, "RPM");

    // Right Turn Blinker Arrow (Top-Right)
    if (right_blinker_active && blink_state) {
      u8g2.drawXBMP(116, 3, 8, 7, bitmap_arrow_right);
    } else {
      u8g2.drawPixel(116, 6);
      u8g2.drawPixel(123, 6);
    }

    // ===========================================================================
    // 3. CENTER COLUMN: STATUS & TELEMETRY HUD
    // ===========================================================================
    // A. Top Center: Coolant / Temperature Icon & Heartbeat
    u8g2.drawXBMP(60, 4, 9, 9, bitmap_temp_icon);

    // Curved temperature decorative arch dots above temp icon
    u8g2.drawPixel(58, 2);
    u8g2.drawPixel(64, 1);
    u8g2.drawPixel(70, 2);

    // Heartbeat Activity Dot
    if (heartbeat_state) {
      u8g2.drawDisc(64, 15, 1);
    }

    // B. Middle Center: Central Colon & Drive Mode Indicator
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(62, 29, ":");

    // Drive Mode Pill (N / D / R)
    char mode_str[2] = { drive_mode, '\0' };
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2.drawStr(61, 39, mode_str);

    // Dynamic Power Hashes (The angled "///" status bars from upir's photo)
    int num_hashes = (int)(speed_frac * 4.0f + 0.5f);
    if (num_hashes < 1) num_hashes = 1;
    for (int h = 0; h < num_hashes; h++) {
      int hx = 72 + h * 3;
      int hy = 43 - h * 2;
      u8g2.drawLine(hx, hy, hx + 2, hy - 4);
    }

    // C. Bottom Center: Fuel Pump Icon & Curved Gauge Arc
    u8g2.drawXBMP(60, 46, 8, 8, bitmap_fuel_icon);

    // Curved Fuel Bar Geometry: Center (64, 40), Radius = 20, Angle 245 to 295
    const int cx_fuel = 64;
    const int cy_fuel = 40;
    const int r_fuel  = 20;

    // Fuel Track Boundary Arc with 'E' and 'F' End Ticks
    for (float fa = 245.0f; fa <= 295.0f; fa += 2.0f) {
      float frad = fa * 0.0174532925f;
      int fx = cx_fuel + (int)(r_fuel * cos(frad) + 0.5f);
      int fy = cy_fuel - (int)(r_fuel * sin(frad) + 0.5f);
      u8g2.drawPixel(fx, fy);
    }
    // E and F tick marks
    u8g2.drawLine(52, 57, 52, 60);
    u8g2.drawLine(76, 57, 76, 60);

    // Active Fuel Level Fill (Curved thick bar along the arc)
    float fuel_frac = fuel_smooth / 100.0f;
    if (fuel_frac > 1.0f) fuel_frac = 1.0f;
    if (fuel_frac < 0.0f) fuel_frac = 0.0f;

    float fuel_fill_deg = 245.0f + (fuel_frac * 50.0f);
    for (float fa = 245.0f; fa <= fuel_fill_deg; fa += 1.0f) {
      float frad = fa * 0.0174532925f;
      float cos_fa = cos(frad);
      float sin_fa = sin(frad);
      int fx1 = cx_fuel + (int)((r_fuel - 1) * cos_fa + 0.5f);
      int fy1 = cy_fuel - (int)((r_fuel - 1) * sin_fa + 0.5f);
      int fx2 = cx_fuel + (int)((r_fuel + 1) * cos_fa + 0.5f);
      int fy2 = cy_fuel - (int)((r_fuel + 1) * sin_fa + 0.5f);
      u8g2.drawLine(fx1, fy1, fx2, fy2);
    }

    // D. Bottom Right: Authentic UPiR / X5 Logo Watermark
    u8g2.drawXBMP(110, 58, 16, 4, bitmap_upir_logo);

    // ===========================================================================
    // SEND FRAMEBUFFER TO PHYSICAL OLED GLASS
    // ===========================================================================
    u8g2.sendBuffer();
  }

  // -----------------------------------------------------------------------------
  // C. SERIAL TELEMETRY STREAM (10 Hz FOR MONITOR DIAGNOSTICS)
  // -----------------------------------------------------------------------------
  static uint32_t last_serial_ms = 0;
  if (current_ms - last_serial_ms >= 100) {
    last_serial_ms = current_ms;
    Serial.print(F("MODE:"));
    Serial.print(drive_mode);
    Serial.print(F(" | SPD:"));
    Serial.print((int)speed_smooth);
    Serial.print(F(" cm/s | RPM:"));
    Serial.print((int)rpm_smooth);
    Serial.print(F(" | FUEL:"));
    Serial.print((int)fuel_smooth);
    Serial.print(F("% | LY_RAW:"));
    Serial.print(raw_ly);
    Serial.print(F(" | LX_RAW:"));
    Serial.println(raw_lx);
  }
}
