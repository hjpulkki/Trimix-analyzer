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
#define BUTTON_PIN 1  // Calibration button
#define LONG_PRESS_TIME 2000  // 2 seconds
#define N_MEASUREMENTS 20

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_ADS1115 ads;

// -------- Public variables
float o2_voltage = 0;         // O2 sensor voltage (mV)
float he_voltage = 0;         // He sensor bridge voltage (mV)

float o2_calib = 0;           // mV reading for 20.9% O2 (air)
float he_zero = 0;            // Offset for He bridge
float he_calib = 595.56;      // Measured mV @ 100% He (user-calibrated)
float he_calib_corr = he_calib * (100 / 87.083);   // adjust so user enters real mV@100%He

// Limits for calibration sanity checks
float min_o2_calib = 7.00;       // Minimum valid O2 sensor voltage for air
float min_he_calib = 200;        // Minimum valid He sensor voltage for 100% Helium
float max_he_zero = 50;          // Maximum absolute value for the zero point of He sensor
float max_vo2_for_he = 1;        // Maximum O2 sensor reading for 100% Helium (Mv)

FlashStorage(magicStore, uint32_t);
FlashStorage(hecorrStore, float);

RunningAverage ra_o2(N_MEASUREMENTS);     // Moving average for O2
RunningAverage ra_he(N_MEASUREMENTS);     // Moving average for He

// ---------- Helper: Temperature Compensation ----------
float get_temp_comp() {
  unsigned long t = millis();
  if (t < 30000)  return 18;
  if (t < 40000)  return 17;
  if (t < 50000)  return 16;
  if (t < 60000)  return 15;
  if (t < 70000)  return 14;
  if (t < 80000)  return 13;
  if (t < 90000)  return 12;
  if (t < 105000) return 11;
  if (t < 120000) return 10;
  if (t < 150000) return 9;
  if (t < 165000) return 8;
  if (t < 180000) return 7;
  if (t < 210000) return 6;
  if (t < 240000) return 5;
  if (t < 270000) return 4;
  if (t < 300000) return 3;
  if (t < 360000) return 2;
  return 0;
}

// ---------- Display Handling ----------
void update_top_display() {
  // --- Always show O2mV and He mV on top of screen ---
  display.fillRect(0, 0, 128, 16, SH110X_BLACK);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("O2mV ");
  display.print(o2_voltage, 2);
  display.setCursor(66, 0);
  display.print("He mV ");
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

// ---------- Button Handling ----------
void handle_button() {
  static bool button_pressed = false;
  static bool long_press_triggered = false;
  static unsigned long press_start_time = 0;
  static unsigned long last_debounce_time = 0;
  const unsigned long debounce_delay = 50; // 50 ms debounce

  int reading = digitalRead(BUTTON_PIN);

  // Debounce: only accept changes after stable state
  if (millis() - last_debounce_time > debounce_delay) {
    if (reading == LOW && !button_pressed) {
      // Button just pressed
      button_pressed = true;
      long_press_triggered = false;
      press_start_time = millis();
      last_debounce_time = millis();
    }

    if (button_pressed && reading == LOW) {
      // Button is being held
      unsigned long press_duration = millis() - press_start_time;
      if (!long_press_triggered && press_duration >= LONG_PRESS_TIME) {
        long_press_triggered = true;
        calibrate_he();  // Long press triggers He calibration
      }
    }

    if (button_pressed && reading == HIGH) {
      // Button released
      unsigned long press_duration = millis() - press_start_time;
      button_pressed = false;
      last_debounce_time = millis();

      if (!long_press_triggered && press_duration < LONG_PRESS_TIME) {
        calibrate_o2();  // Short press triggers O2 calibration
      }
    }
  }
}

// ---------- Measurement Update ----------
void update_measurements() {
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
  he_voltage -= get_temp_comp();

  // Always display the new results
  update_top_display();
}

// ---------- Air Calibration ----------
void calibrate_o2() {
  show_bottom_message("Calibration", "O2 Sensor", "(Air 20.9% O2)");
  delay(1000);

  for (int i = 0; i < N_MEASUREMENTS; i++) {
    update_measurements();
    delay(100);
    display.print(".");
    display.display();
  }

  // Check if O2 cell is too weak
  if (o2_voltage < min_o2_calib) {
    show_bottom_message("O2 cell o2_voltage", "is too weak.", "V cal = " + String(o2_voltage, 2) + " mV");
    delay(10000);
    show_bottom_message("");
    return;
  }

  if (abs(he_zero) > max_he_zero) {
    show_bottom_message("He zero error", "Adjust calib screw", "He Zero = " + String(he_zero, 2) + " mV");
    delay(10000);
    show_bottom_message("");
    return;
  }

  o2_calib = o2_voltage; // store reference o2_voltage for 20.9% O2
  he_zero = he_voltage;

  show_bottom_message("Calibration OK", "V cal = " + String(o2_calib, 2) + " mV", "He Zero = " + String(he_zero, 0) + " mV");
  delay(2000);
  show_bottom_message("");
}

// ---------- He Calibration ----------
void save_he_calib() {
  magicStore.write(MAGIC_VALUE);
  hecorrStore.write(he_calib_corr);
}

void load_he_calib() {
  uint32_t magic = magicStore.read();
  if (magic == MAGIC_VALUE) {
    he_calib_corr = hecorrStore.read();
    show_bottom_message("Loaded He calib", "Source: EEPROM", "mV@100%He = " + String(he_calib_corr, 1));
  } else {
    show_bottom_message("Loaded He calib", "Source: default", "mV@100%He = " + String(he_calib_corr, 1));
  }
  delay(2000);
}

void calibrate_he() {
  show_bottom_message("Calibration", "He Sensor", "(100% He)");
  delay(1000);

  for (int i = 0; i < N_MEASUREMENTS; i++) {
    update_measurements();
    delay(100);
    display.print(".");
    display.display();
  }

  if (he_voltage < min_he_calib) {
    show_bottom_message("Error with He", "calibration", "he_calib = " + String(he_voltage, 0) + " mV");
    delay(10000);
    return;
  }
  if (o2_voltage > max_vo2_for_he) {
    show_bottom_message("Oxygen present", "not 100% He.", "V O2 = " + String(o2_voltage, 2) + " mV");
    delay(10000);
    return;
  }

  he_calib = he_voltage;  // Measured mV @ 100% He
  he_calib_corr = he_calib * (100 / 87.083);   // adjust so user enters real mV@100%He
  save_he_calib(); // Save value in EEPROM.

  show_bottom_message("He Calibration OK", "he_calib = " + String(he_voltage, 2) + " mV", "V O2 = "+ String(o2_voltage, 2) + " mV");
  delay(2000);
}

// ---------- Setup ----------
void setup(void) {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(400000L);
  pinMode(1, INPUT_PULLUP); // Button for manual calibration

  display.begin(i2c_Address, true);
  display.clearDisplay();

  delay(1000);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  show_bottom_message("Kaasuvelho", "v0.9 beta");
  delay(4000);

  ads.begin();
  update_measurements();

  // wait until He sensor warms up
  while (he_voltage > 10) {
    update_measurements();
    show_bottom_message("Preheating", "Helium sensor...", "V he_voltage= " + String(he_voltage, 0) + " mV");
    delay(50);
  }
  delay(5000);
  show_bottom_message("Helium sensor ready");
  delay(1000);

  calibrate_o2(); // run O2 calibration on startup
  load_he_calib();

  show_bottom_message("Analyzer ready");
  delay(1000);
}

// ---------- Main Loop ----------
void loop() {
  // Read raw ADC values
  update_measurements();

  // --- O2 calculation ---
  float nitrox = o2_voltage * (20.9 / o2_calib);  // scale by calibration value

  // --- He percentage calculation ---
  float helium = 100 * (he_voltage-he_zero) / he_calib_corr;        // linear %He estimate
  if (helium > 50)
    helium = helium * (1 + (helium - 50) * 0.4 / 100);
  if (helium < 2) helium = 0;

  // --- Bottom gas mix display ---
  display.fillRect(0, 18, 128, 46, SH110X_BLACK);  // clear full lower area
  display.setCursor(10, 25);
  display.setTextSize(2);
  if (helium > 0) {
    display.print("Trimix ");
    display.setCursor(10, 45);
    display.print(nitrox, 0);
    display.print(" / ");
    display.print(helium, 0);
  } else {
    display.print("Nitrox ");
    display.setCursor(10, 45);
    display.print(nitrox, 0);
  }
  display.display();

  // Manual recalibration when button is pressed
  handle_button();
  delay(100);
}
