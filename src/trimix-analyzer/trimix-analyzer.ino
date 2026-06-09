// License: CC BY-NC-SA 4.0
// https://creativecommons.org/licenses/by-nc-sa/4.0/
// Trimix Analyzer
// Original: Yves Caze, Savoie Plongee
// Mods: GoDive BRB (2021), Dominik Wiedmer (2021–2024), Heikki Pulkkinen (2025)

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <RunningAverage.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <FlashStorage.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define i2c_Address 0x3c
#define SH110X_NO_SPLASH
#define MAGIC_VALUE    0xBEEFCAFE
#define CALIBRATION_BUTTON_PIN 1

// Toggle this flag to switch between the advanced and master branch features.
// 0 = Simplified firmware. Only linear calibration.
// 1 = Advanced firmware (quadratic O2 calibration, o2 compensation for helium).
#define ADVANCED_FEATURES 1

#define LONG_PRESS_TIME 1200
#define N_MEASUREMENTS 20

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_ADS1115 ads;

// -------- Public variables shared by both modes
float o2_voltage = 0;         // O2 sensor voltage (mV)
float he_voltage = 0;         // He sensor bridge voltage (mV)

// Assume quadratic equation. o2 = a*V + b*V*V (b = 0 for linear mode)
float o2_calib_21 = 0;
float o2_calib_a = 0;
float o2_calib_b = 0;
float he_zero = 0;            // Offset for He bridge
float pure_he_mv = 551;       // Measured mV @ 100% He (user-calibrated)
float he_span = pure_he_mv * (100 / 87.083);   // adjust so user enters real mV@100%He
float o2_comp_k = ADVANCED_FEATURES ? 6.0f : 0.0f;          // Compensate He measurements based on gas oxygen content

// Limits for calibration sanity checks
const float MIN_O2_CALIB = 7.00;       // Minimum valid O2 sensor voltage for air
const float MAX_O2_CALIB = 12.00;      // Maximum valid O2 sensor voltage for air
const float MIN_PURE_HE_MV = 200;      // Minimum valid He sensor voltage for 100% Helium
const float MAX_HE_ZERO = 50.0f;       // Maximum absolute value for the zero point of He sensor
const float MAX_VO2_FOR_HE = 1;        // Maximum O2 sensor reading for 100% Helium (Mv)

enum CalibMenuState { MENU_OFF, MENU_ON };
CalibMenuState calibMenu = MENU_OFF;

const char* calibItems[] = {
  "Air",
  "Helium",
#if ADVANCED_FEATURES
  "Oxygen",
#endif
};
constexpr int CALIB_ITEMS = sizeof(calibItems) / sizeof(calibItems[0]);
int calibIndex = 0; // 0=Air,1=He,2=O2
const unsigned long MENU_TIMEOUT = 8000;
unsigned long menuTimer = 0;

#if ADVANCED_FEATURES
const float MAX_CURVATURE_RATIO = 0.5f;   // |b| <= 0.5*a
const float EPS = 1e-9f;                  // tiny number to avoid div-zero
#endif

bool sensor_warning = false;    // if raised, stays on until power cycle


FlashStorage(magicStore, uint32_t);
FlashStorage(hecorrStore, float);
#if ADVANCED_FEATURES
FlashStorage(o2MagicStore, uint32_t);
FlashStorage(o2bStore, float);
FlashStorage(o2CompStore, float);
#endif

RunningAverage ra_o2(N_MEASUREMENTS);     // Moving average for O2
RunningAverage ra_he(N_MEASUREMENTS);     // Moving average for He

void update_top_display() {
  // --- Always show O2mV and He mV on top of screen ---
  display.fillRect(0, 0, 128, 16, SH110X_BLACK);
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.print("O2mV ");
  display.print(o2_voltage, 2);
  display.setCursor(71, 0);
  display.print("HemV ");
  display.print(he_voltage, 0);
  display.display();
}

void show_bottom_message(const String &line1, const String &line2 = "", const String &line3 = "") {
  // --- Utility for bottom-area messages ---
  display.fillRect(0, 18, 128, 46, SH110X_BLACK);
  display.setTextSize(1);
  if (line1.length()) { display.setCursor(10, 20); display.print(line1); }
  if (line2.length()) { display.setCursor(10, 30); display.print(line2); }
  if (line3.length()) { display.setCursor(10, 40); display.print(line3); }
  display.display();
  display.setCursor(5, 50);
}

void draw_warning_icon(int x, int y) {
  display.drawTriangle(
      x, y + 10,
      x + 10, y + 10,
      x + 5, y,
      SH110X_WHITE);

  display.drawLine(x + 5, y + 3,
                   x + 5, y + 7,
                   SH110X_WHITE);

  display.drawPixel(x + 5, y + 9,
                    SH110X_WHITE);
}

void drawCalibMenu() {
  display.fillRect(0, 16, 128, 48, SH110X_BLACK);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.print("Select calib gas");

  for (int i = 0; i < CALIB_ITEMS; i++) {
    display.setCursor(10, 30 + i * 10);
    display.print(i == calibIndex ? "> " : "  ");
    display.print(calibItems[i]);
  }

  display.display();
}

void handle_calibration_button() {
  static bool btnDown = false;
  static unsigned long t0 = 0;
  static unsigned long lastDebounce = 0;
  static bool longFired = false;

  const unsigned long debounce = 40;
  int r = digitalRead(CALIBRATION_BUTTON_PIN);
  unsigned long t = millis();

  if (t - lastDebounce < debounce) return;
  lastDebounce = t;

  // ---- ENTER MENU ----
  if (calibMenu == MENU_OFF) {
    if (!btnDown && r == LOW) {
      btnDown = true;
      t0 = t;
    }

    if (btnDown && r == HIGH) {
      btnDown = false;
      calibMenu = MENU_ON;
      calibIndex = 0;
      menuTimer = t;
      drawCalibMenu();
    }
    return;
  }

  // ---- MENU MODE ----
  if (!btnDown && r == LOW) {
    btnDown = true;
    t0 = t;
    longFired = false;
  }

  if (btnDown && r == LOW) {
    if (!longFired && t - t0 > LONG_PRESS_TIME) {
      longFired = true;
      calibMenu = MENU_OFF;
      display.clearDisplay();
      if (calibIndex==0) calibrate_air();
      if (calibIndex==1) calibrate_he();
      #if ADVANCED_FEATURES
      if (calibIndex==2) calibrate_oxygen();
      #endif
      btnDown = false;
    }
  }

  if (btnDown && r == HIGH) {
    btnDown = false;

    if (!longFired) {
      calibIndex = (calibIndex + 1) % CALIB_ITEMS;
      drawCalibMenu();
    }

    menuTimer = t;
  }

  if (t - menuTimer > MENU_TIMEOUT) {
    calibMenu = MENU_OFF;
    show_bottom_message("Menu timeout");
    delay(500);
    display.clearDisplay();
  }
}

// ---------- Measurement Update ----------
void update_measurements() {
  // O2 and He channels use different gains to fit their voltage ranges and maximize accuracy.
  // --- Channel 0–1: 0–50 mV ---
  ads.setGain(GAIN_SIXTEEN);                 // ±0.256 V range
  int16_t adc0 = ads.readADC_Differential_0_1();
  ra_o2.addValue(adc0);

  // Convert the raw measurement to mV based on GAIN_SIXTEEN
  o2_voltage = ra_o2.getAverage() * (0.256 / 32768.0 * 1000);

  // Voltage is negative only if the sensor is plugged in the wrong way
  o2_voltage = fabs(o2_voltage);

  // --- Channel 2–3: 0–650 mV ---
  ads.setGain(GAIN_FOUR);                    // ±1.024 V range
  int16_t adc1 = ads.readADC_Differential_2_3();
  ra_he.addValue(adc1);

  // Convert the raw measurement to mV based on GAIN_FOUR
  he_voltage = ra_he.getAverage() * (1.024 / 32768.0 * 1000);

  update_top_display();
}

void run_calibration() {
  for (int i = 0; i < N_MEASUREMENTS; i++) {
    update_measurements();
    display.setCursor(10 + 5*i, 40);
    display.print(".");
    display.display();
    delay(100);
  }
}

float compute_nitrox_percentage() {
  return o2_calib_a * o2_voltage + o2_calib_b * o2_voltage * o2_voltage;
}

float compute_helium_percentage() {
  float o2_compensation = 0;
#if ADVANCED_FEATURES
  if (o2_calib_21 != 0) {
    o2_compensation = (o2_voltage - o2_calib_21) / o2_calib_21 * o2_comp_k;
  }
#endif

  float helium = 100 * (he_voltage - he_zero - o2_compensation) / he_span;
  if (helium > 50)
    helium = helium * (1 + (helium - 50) * 0.4 / 100);
  if (helium < 2) helium = 0;
  return helium;
}

inline void apply_linear_calibration() {
    o2_calib_a = 20.9 / o2_calib_21;
    o2_calib_b = 0.0f;
}

void calibrate_air() {
  // Performs O2 calibration and zero-offset calibration for the He sensor.
  show_bottom_message("Calibrating Sensors", "Ref: Air 20.9%");
  delay(900);
  run_calibration();

  if (o2_voltage < MIN_O2_CALIB) {
    show_bottom_message("Error: Low O2 mV",
                        "Replace O2 Cell",
                        "Measured: " + String(o2_voltage,2) + " mV");
    delay(4000);
    sensor_warning = true;
  }

  if (o2_voltage > MAX_O2_CALIB) {
    show_bottom_message("Too large O2 voltage",
                        "Check gas",
                        "Measured: " + String(o2_voltage,2) + " mV");
    delay(4000);
    sensor_warning = true;
  }

  if (fabs(he_voltage) > MAX_HE_ZERO) {
    show_bottom_message("Error: He Zero",
                        "Adjust R4",
                        "He Zero = " + String(he_voltage,0) + " mV");
    delay(4000);
    sensor_warning = true;
  }

  o2_calib_21 = o2_voltage; // store reference o2_voltage for 20.9% O2
  he_zero = he_voltage;
  show_bottom_message(
      sensor_warning ? "Calibration WARN" : "Calibration OK",
      "O2 Ref = " + String(o2_calib_21,2) + " mV",
      "He Zero = " + String(he_zero,0) + " mV");
  delay(4000);

#if ADVANCED_FEATURES
  o2_calib_a = (20.9 - o2_calib_b*o2_calib_21*o2_calib_21) / o2_calib_21;
  validate_nonlinear_calibration(o2_calib_a, o2_calib_b);
#else
  apply_linear_calibration();
#endif

}

void save_he_span() {
  if (sensor_warning)
    return;

  magicStore.write(MAGIC_VALUE);
  hecorrStore.write(he_span);
}

void load_he_span() {
  // Stores the helium calibration in flash memory; only needed occasionally.
  uint32_t magic = magicStore.read();
  if (magic == MAGIC_VALUE) {
    he_span = hecorrStore.read();
    show_bottom_message("Loaded He Span", "Source: EEPROM",
                        "He Span=" + String(he_span,0));
  } else {
    show_bottom_message("No saved He Span", "Source: Default",
                        "He Span=" + String(he_span,0));
  }
  delay(4000);
}

#if ADVANCED_FEATURES
void save_o2_calibration() {
  // Save even if there are sensor warnings.
  
  o2MagicStore.write(MAGIC_VALUE);
  o2bStore.write(o2_calib_b);
  o2CompStore.write(o2_comp_k);
}

void load_o2_calibration() {
  uint32_t magic = o2MagicStore.read();
  if (magic == MAGIC_VALUE) {
    o2_calib_b = o2bStore.read();
    o2_comp_k = o2CompStore.read();
    show_bottom_message("Loaded O2 Calib", "Source: EEPROM",
                        "b=" + String(o2_calib_b,4) + " k=" + String(o2_comp_k,4));
  } else {
    show_bottom_message("Loaded O2 Calib", "Source: Default",
                        "b=" + String(o2_calib_b,4) + " k=" + String(o2_comp_k,4));
  }
  delay(4000);
}
#endif

void calibrate_he() {
  // Calibrates the He sensor span using 100% helium as the reference.
  show_bottom_message("Calibrating He", "Ref: 100% He");
  delay(900);
  run_calibration();

  if (he_voltage - he_zero < MIN_PURE_HE_MV) {
    show_bottom_message("Error: He Too Low",
                        "Check gas",
                        "He mV = " + String(he_voltage,0));
    delay(4000);
    sensor_warning = true;
  }

  if (o2_voltage > MAX_VO2_FOR_HE) {
    show_bottom_message("Error: O2 Present",
                        "Not 100% He",
                        "O2 mV = " + String(o2_voltage,2));
    delay(4000);
    sensor_warning = true;
  }
  pure_he_mv = he_voltage - he_zero;
  he_span = pure_he_mv * (100 / 87.083);

  save_he_span();

  show_bottom_message(
      sensor_warning ? "Calibration WARN" : "He Calibration OK",
      "He mV = " + String(he_voltage,2),
      "O2 mV = " + String(o2_voltage,2));

  delay(4000);
}

void preheat_helium_sensor() {
  show_bottom_message("Preheat He Sensor");
  delay(10000);  // Always preheat for 10 seconds
  show_bottom_message("He Sensor Ready");
  delay(1000);
}

#if ADVANCED_FEATURES
// Compensate for nonlinearity with 100% oxygen
void calibrate_oxygen()
{
  show_bottom_message("Calibrating oxygen", "Ref: Oxygen 100.0%");
  delay(900);
  run_calibration(); // updates o2_voltage

  float denom = o2_calib_21 * o2_voltage * (o2_voltage - o2_calib_21);

  if (fabs(denom) < EPS) {
    apply_linear_calibration();
    show_bottom_message(
        "O2 Calib Error",
        "Check gas",
        "Using linear"
    );
    delay(4000);
  } else{

    o2_calib_b = (100.0 * o2_calib_21 - 20.9 * o2_voltage) / denom;
    o2_calib_a = (20.9 - o2_calib_b * o2_calib_21 * o2_calib_21) / o2_calib_21;
    validate_nonlinear_calibration(o2_calib_a, o2_calib_b);

    // Update o2 compensation for he voltage
    o2_comp_k = (he_voltage-he_zero)*o2_calib_21/(o2_voltage-o2_calib_21);
    show_bottom_message(
        "O2 compensation",
        "k = " + String(o2_comp_k, 4)
    );
    delay(4000);
  }

  // Save calibration even if validation fails. In this case saves b=0.
  save_o2_calibration();
}

void validate_nonlinear_calibration(float a, float b){
  if (b < 0) {
      apply_linear_calibration();
      show_bottom_message(
          "O2 Calib Rejected",
          "b = " + String(b, 4) + " < 0",
          "Using linear"
      );
      delay(10000);
      return;
  }

  if (a <= 0 || a > (20.9 / MIN_O2_CALIB)) {
      apply_linear_calibration();
      show_bottom_message(
          "O2 Sensor Weak",
          "a = " + String(a, 1),
          "Using linear"
      );
      delay(10000);
      return;
  }

  if (fabs(b) > MAX_CURVATURE_RATIO * a) {
      apply_linear_calibration();
      show_bottom_message(
          "Error: param ratio",
          "a = " + String(a, 1),
          "b = " + String(b, 4)
      );
      delay(10000);
      return;
  }

  show_bottom_message(
      "O2 = a*V + b*V^2",
      "a = " + String(a, 1),
      "b = " + String(b, 4)
  );
  delay(4000);
}
#endif

// ---------- Setup ----------
void setup(void) {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(400000L);
  pinMode(CALIBRATION_BUTTON_PIN, INPUT_PULLUP); // Button for manual calibration

  display.begin(i2c_Address, true);
  display.clearDisplay();

  delay(1000);

  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(5, 20);
  display.setTextSize(2);
  display.print("Kaasuvelho");
  display.setCursor(10, 40);
  display.setTextSize(1);

#if ADVANCED_FEATURES
  display.print("Advanced version");
#else
  display.print("Simplified version");
#endif

  display.display();
  delay(4000);

  ads.begin();

  load_he_span();

#if ADVANCED_FEATURES
  load_o2_calibration();
#endif

  preheat_helium_sensor();
  calibrate_air();
}

// ---------- Main Loop ----------
void loop() {
  // Read raw ADC values
  update_measurements();
  float nitrox = compute_nitrox_percentage();
  float helium = compute_helium_percentage();

  // --- Bottom gas mix display ---
  if (calibMenu == MENU_OFF) {
    display.fillRect(0, 18, 128, 46, SH110X_BLACK);  // clear full lower area
    display.setCursor(10, 25);
    display.setTextSize(2);
    if (helium > 0) {
      display.print("Trimix ");
      display.setCursor(10, 45);
      display.print(nitrox, 1);
      display.print(" / ");
      display.print(helium, 0);
    } else {
      display.print("Nitrox ");
      display.setCursor(10, 45);
      display.print(nitrox, 1);
    }
    if (sensor_warning)
      draw_warning_icon(118, 25);
    display.display();
  }

  handle_calibration_button();
  delay(100);
}
