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
#define LONG_PRESS_TIME 1200
#define N_MEASUREMENTS 20

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_ADS1115 ads;

// -------- Public variables
float o2_voltage = 0;         // O2 sensor voltage (mV)
float he_voltage = 0;         // He sensor bridge voltage (mV)

// Assume quadratic equation. o2 = a*V + b*V*V
float o2_calib_21 = 0;
float o2_calib_a = 0;
float o2_calib_b = 0;
float he_zero = 0;            // Offset for He bridge
float pure_he_mv = 551;       // Measured mV @ 100% He (user-calibrated)
float he_span = pure_he_mv * (100 / 87.083);   // adjust so user enters real mV@100%He
float o2_comp_k = 0.7;       // Compensate He measurements based on gas oxygen content

// Limits for calibration sanity checks
float min_o2_calib = 7.00;       // Minimum valid O2 sensor voltage for air

float max_o2_calib = 12.00;      // Maximum valid O2 sensor voltage for air
float min_pure_he_mv = 200;      // Minimum valid He sensor voltage for 100% Helium
float max_he_zero = 50;          // Maximum absolute value for the zero point of He sensor
float max_vo2_for_he = 1;        // Maximum O2 sensor reading for 100% Helium (Mv)

// ---- PREHEAT/ STABILITY SETTINGS ----
const float CHANGE_THRESHOLD = 1;              // Allowed change in He value after preheating
const float CHANGE_TIMESPAN  = 60;             // Duration in seconds over which the change above is allowed
const unsigned long MAX_PREHEAT_TIME = 10UL * 60UL * 1000UL;  // 10 min timeout

const float MAX_CURVATURE_RATIO = 0.5f;   // |b| <= 0.5*a
const float EPS = 1e-9f;                  // tiny number to avoid div-zero

enum CalibMenuState { MENU_OFF, MENU_ON };
CalibMenuState calibMenu = MENU_OFF;
int calibIndex = 0; // 0=Air,1=He,2=O2
unsigned long btnStart = 0;
bool btnWasDown = false;
const unsigned long MENU_TIMEOUT = 8000;
unsigned long menuTimer = 0;
bool longPressTriggered = false;
bool justCalibrated = false;

FlashStorage(magicStore, uint32_t);
FlashStorage(hecorrStore, float);

RunningAverage ra_o2(N_MEASUREMENTS);     // Moving average for O2
RunningAverage ra_he(N_MEASUREMENTS);     // Moving average for He

float get_temperature_compensation() {
  return 0;
}

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

void drawCalibMenu() {
  display.fillRect(0, 0, 128, 64, SH110X_BLACK);
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.print("Select calib gas");
  const char* items[3] = {"Air", "Helium", "Oxygen"};
  for (int i = 0; i < 3; i++) {
    display.setCursor(10, 20 + i * 10);
    if (i == calibIndex) display.print("> ");
    else display.print("  ");
    display.print(items[i]);
  }
  display.setCursor(10, 50);
  display.print("Hold = Select");

  display.display();
}


void handle_button() {
  int r = digitalRead(CALIBRATION_BUTTON_PIN);
  unsigned long t = millis();

  static unsigned long lastRead = 0;
  const unsigned long debounce = 40;
  if (millis() - lastRead < debounce) return;
  lastRead = millis();


  // Enter menu if OFF
  if (calibMenu == MENU_OFF) {

      // ignore the first release after calibration
      if (justCalibrated) {
        if (r == HIGH) return;     // wait until button is fully released
        justCalibrated = false;
      }

      if (!btnWasDown && r == LOW) { btnWasDown=true; btnStart=t; }
      if (btnWasDown && r == HIGH) {
        btnWasDown=false; calibMenu=MENU_ON; calibIndex=0; menuTimer=t;
        drawCalibMenu();
      }
      return;
  }

  // --- MENU MODE ---
  if (!btnWasDown && r == LOW) { btnWasDown=true; btnStart=t; longPressTriggered=false; }

  if (btnWasDown && r == LOW) {
    // auto-trigger long press select
    if (!longPressTriggered && (t - btnStart >= LONG_PRESS_TIME)) {
      calibMenu = MENU_OFF;
      display.clearDisplay();

      if (calibIndex==0) calibrate_air();
      if (calibIndex==1) calibrate_he();
      if (calibIndex==2) calibrate_oxygen();
      
      justCalibrated = true;        // <-- prevent re-entering menu on release
      btnWasDown = false;
      longPressTriggered = true;
    }
  }

  if (btnWasDown && r == HIGH) {
    unsigned long d = t - btnStart;
    btnWasDown = false; menuTimer=t;

    if (d < LONG_PRESS_TIME) {
      // short press = cycle menu
      calibIndex = (calibIndex + 1) % 3;
      drawCalibMenu();
    }
  }

  // timeout
  if (t - menuTimer > MENU_TIMEOUT) {
    calibMenu = MENU_OFF;
    show_bottom_message("Menu timeout");
    delay(800);
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
  o2_voltage = abs(o2_voltage);

  // --- Channel 2–3: 0–650 mV ---
  ads.setGain(GAIN_FOUR);                    // ±1.024 V range
  int16_t adc1 = ads.readADC_Differential_2_3();
  ra_he.addValue(adc1);

  // Convert the raw measurement to mV based on GAIN_FOUR
  he_voltage = ra_he.getAverage() * (1.024 / 32768.0 * 1000);
  he_voltage -= get_temperature_compensation();

  if (calibMenu == MENU_OFF) {
    update_top_display();
  }
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

void calibrate_air() {
  // Performs O2 calibration and zero-offset calibration for the He sensor.
  show_bottom_message("Calibrating Sensors", "Ref: Air 20.9%");
  delay(900);
  run_calibration();

  if (o2_voltage < min_o2_calib) {
    show_bottom_message("Error: O2 Cell Weak",
                        "Replace cell",
                        "Measured: " + String(o2_voltage,2) + " mV");
    delay(10000);  return;
  }

  if (o2_voltage > max_o2_calib) {
    show_bottom_message("Too large O2 voltage",
                        "Check gas",
                        "Measured: " + String(o2_voltage,2) + " mV");
    delay(10000);  return;
  }

  if (abs(he_voltage) > max_he_zero) {
    show_bottom_message("Error: He Zero Offset",
                        "Adjust He Zero Screw",
                        "He Zero = " + String(he_zero,0) + " mV");
    delay(10000);
    return;
  }

  o2_calib_21 = o2_voltage; // store reference o2_voltage for 20.9% O2
  o2_calib_a = (20.9 - o2_calib_b*o2_calib_21*o2_calib_21) / o2_calib_21;

  he_zero = he_voltage;

  show_bottom_message("Air Calibration OK",
                      "O2 Ref = " + String(o2_calib_21,2) + " mV",
                      "He Zero = " + String(he_zero,0) + " mV");
  delay(2000);
  validate_nonlinear_calibration(o2_calib_a, o2_calib_b);
}

void save_he_span() {
  magicStore.write(MAGIC_VALUE);
  hecorrStore.write(he_span);
}

void load_he_span() {
  // Stores the helium calibration in flash memory; only needed occasionally.
  uint32_t magic = magicStore.read();
  if (magic == MAGIC_VALUE) {
    show_bottom_message("Loaded He Span", "Source: EEPROM",
                        "He Span=" + String(he_span,0));
  } else {
    show_bottom_message("Loaded He Span", "Source: Default",
                        "He Span=" + String(he_span,0));
  }
  delay(2000);
}

void calibrate_he() {
  // Calibrates the He sensor span using 100% helium as the reference.
  show_bottom_message("Calibrating He", "Ref: 100% He");
  delay(900);
  run_calibration();

  if (he_voltage - he_zero < min_pure_he_mv) {
    show_bottom_message("Error: He Too Low",
                        "Check gas",
                        "He mV = " + String(he_voltage,0));
    delay(10000);
    return;
  }

  if (o2_voltage > max_vo2_for_he) {
    show_bottom_message("Error: O2 Present",
                        "Not 100% He",
                        "O2 mV = " + String(o2_voltage,2));
    delay(10000);
    return;
  }

  pure_he_mv = he_voltage - he_zero;  // Measured mV @ 100% He
  he_span = pure_he_mv * (100 / 87.083);   // adjust so user enters real mV@100%He
  save_he_span(); // Save value in EEPROM.

  show_bottom_message("He Calibration OK",
                      "He mV = " + String(he_voltage,2),
                      "O2 mV = " + String(o2_voltage,2));
  delay(2000);
}

void preheat_helium_sensor() {
  show_bottom_message("Preheat He Sensor");
  unsigned long prevTime  = 0;
  float prevHeVoltage = he_voltage;
  unsigned long now = 0;

  while (now < MAX_PREHEAT_TIME) {
    run_calibration();
    now = millis();
    float dt_s = (now - prevTime) / 1000.0;
    float ratio = (fabs(he_voltage - prevHeVoltage) / he_span) / dt_s * CHANGE_TIMESPAN * 100;
    if (ratio < CHANGE_THRESHOLD) {
      break;
    }
    show_bottom_message("Preheat He Sensor",
                        "He % diff=" + String(ratio, 1));

    prevHeVoltage = he_voltage;
    prevTime = now;
  }

  show_bottom_message("He Sensor Ready");
  delay(1000);
}

inline void set_linear_fallback() {
    o2_calib_a = 20.9 / o2_calib_21;
    o2_calib_b = 0.0f;
}

// Compensate for nonlinearity with 100% oxygen
void calibrate_oxygen()
{
  show_bottom_message("Calibrating oxygen", "Ref: Oxygen 100.0%");
  delay(900);
  run_calibration(); // updates o2_voltage

  float V21  = o2_calib_21;
  float V100 = o2_voltage;
  float denom = V21 * V100 * (V100 - V21);

  if (fabs(denom) < EPS) {
      set_linear_fallback();
      show_bottom_message(
          "O2 Calib Error",
          "Check gas",
          "Using linear"
      );
      delay(10000);
      return;
  }

  o2_calib_b = (100.0 * V21 - 20.9 * V100) / denom;
  o2_calib_a = (20.9 - o2_calib_b * V21 * V21) / V21;
  validate_nonlinear_calibration(o2_calib_a, o2_calib_b);

  // Update o2 compensation for he voltage
  o2_comp_k = (he_voltage-he_zero)/(o2_voltage-o2_calib_21);
  show_bottom_message(
      "O2 compensation",
      "k = " + String(o2_comp_k, 4)
  );
  delay(2000);
  // TODO: Save o2_calib_b and o2_comp_k in flash memory.
}

void validate_nonlinear_calibration(float a, float b){
  if (b < 0) {
      set_linear_fallback();
      show_bottom_message(
          "O2 Calib Rejected",
          "b = " + String(b, 4) + " < 0",
          "Using linear"
      );
      delay(10000);
      return;
  }

  if (a <= 0 || a > (20.9 / min_o2_calib)) {
      set_linear_fallback();
      show_bottom_message(
          "O2 Sensor Weak",
          "a = " + String(a, 1),
          "Using linear"
      );
      delay(10000);
      return;
  }

  if (fabs(b) > MAX_CURVATURE_RATIO * a) {
      set_linear_fallback();
      show_bottom_message(
          "Error: param ratio",
          "a = " + String(a, 1),
          "b = " + String(b, 4)
      );
      delay(10000);
      return;
  }

  if (b != 0) {
    // Only display if actually nonlinear
    show_bottom_message(
        "O2 = a*V + b*V^2",
        "a = " + String(a, 1),
        "b = " + String(b, 4)
    );
  }
  delay(2000);
}


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
  display.print("Advanced version");
  display.display();
  delay(4000);

  ads.begin();

  load_he_span();
  preheat_helium_sensor();

  calibrate_air();
}

// ---------- Main Loop ----------
void loop() {
  // Read raw ADC values
  update_measurements();

  // --- O2 calculation ---
  float nitrox = o2_calib_a*o2_voltage + o2_calib_b*o2_voltage*o2_voltage;  // scale by calibration value

  // --- He percentage calculation ---
  float o2_compensation = (o2_voltage-o2_calib_21)*o2_comp_k;               // Compensate for o2 levels
  float helium = 100 * (he_voltage-he_zero-o2_compensation) / he_span;      // linear %He estimate
  if (helium > 50)
    helium = helium * (1 + (helium - 50) * 0.4 / 100);
  if (helium < 2) helium = 0;

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
    display.display();
  }

  // Manual recalibration when button is pressed
  handle_button();
  delay(100);
}
