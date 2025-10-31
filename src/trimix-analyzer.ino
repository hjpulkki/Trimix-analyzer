// Trimix Analyzer
// Original: Yves Caze, Savoie Plongee
// Mods: GoDive BRB (2021), Dominik Wiedmer (2021–2024), Heikki Pulkkinen (2025)
// Version 1.1  4.10.2021
// Version 1.2  7.10.2023  Change of library names, calibration goal less strict
// Version 20240601  Added calc for calibration offset, so that entered value = mV@100% He
//                   Added He 0 calib during O2 calib & message if O2 < 7mV and no calibration
// Version 20241017  Removed display blinking
// Version 20251031  Added 100% He calibration by long press, translated everything to English

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <RunningAverage.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <FlashStorage.h>

#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT     64
#define OLED_RESET        -1
#define I2C_ADDRESS     0x3C
#define SH110X_NO_SPLASH
#define MAGIC_VALUE  0xBEEFCAFE

#define BUTTON_PIN        1
#define LONG_PRESS_TIME 2000   // 2 seconds
#define N_MEASUREMENTS     10

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_ADS1115 ads;

// -------- Calibration & constants --------
float Vcalib = 0;            // mV reading for 20.9% O2 (air)
float voltage = 0;           // O2 sensor voltage (mV)
float bridge = 0;            // He sensor bridge voltage (mV)
float minVO2 = 7.00;         // Minimum valid O2 sensor voltage for air
float minVHe = 200;          // Minimum valid He sensor voltage for 100% Helium
float bridgeCalib = 0;       // Offset for He bridge
float TempComp = 0;          // Temperature compensation (time-based)
unsigned long time;

// Initial He calibration values (before user calibration)
float calibMD62 = 595.56;    
float calibMD62_corr = calibMD62 * (100 / 87.083);

FlashStorage(magicStore, uint32_t);
FlashStorage(hecorrStore, float);

RunningAverage RA0(10);  // Moving average for O2
RunningAverage RA1(10);  // Moving average for He

// ---------- Button Handling ----------
void handleButton() {
  static bool buttonPressed = false;
  static bool longPressTriggered = false;
  static unsigned long pressStartTime = 0;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;

  int reading = digitalRead(BUTTON_PIN);

  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading == LOW && !buttonPressed) {
      buttonPressed = true;
      longPressTriggered = false;
      pressStartTime = millis();
      lastDebounceTime = millis();
    }

    if (buttonPressed && reading == LOW) {
      unsigned long pressDuration = millis() - pressStartTime;
      if (!longPressTriggered && pressDuration >= LONG_PRESS_TIME) {
        longPressTriggered = true;
        calibrateHe();
      }
    }

    if (buttonPressed && reading == HIGH) {
      unsigned long pressDuration = millis() - pressStartTime;
      buttonPressed = false;
      lastDebounceTime = millis();

      if (!longPressTriggered && pressDuration < LONG_PRESS_TIME) {
        calibrateO2();
      }
    }
  }
}

// ---------- Measurement Update ----------
void updateMeasurements() {
  ads.setGain(GAIN_SIXTEEN);  // ±0.256 V range
  int16_t adc0 = ads.readADC_Differential_0_1();
  RA0.addValue(adc0);
  voltage = abs(RA0.getAverage() * (0.256 / 32768.0 * 1000));

  ads.setGain(GAIN_FOUR);     // ±1.024 V range
  int16_t adc1 = ads.readADC_Differential_2_3();
  RA1.addValue(adc1);
  bridge = RA1.getAverage() * (1.024 / 32768.0 * 1000);
}

// ---------- O2 Calibration ----------
void calibrateO2() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 0);  display.print("Calibration");
  display.setCursor(10,10);  display.print("O2 Sensor");
  display.setCursor(10,20);  display.print("(Air 20.9% O2)");
  display.display();
  delay(1000);

  if (voltage < minVO2) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10,20); display.print("Error with O2");
    display.setCursor(10,30); display.print("calibration");
    display.setCursor(10,40); display.print("V cal = ");
    display.print(voltage, 2); display.print(" mV");
    display.display();
    delay(10000);
    display.clearDisplay();
    return;
  }

  float Vavg = 0;
  for (int i = 0; i < N_MEASUREMENTS; i++) {
    updateMeasurements();
    Vavg += voltage;
    delay(200);
  }

  Vavg /= N_MEASUREMENTS;
  Vcalib = Vavg;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10,20); display.print("Calibration OK");
  display.setCursor(10,30); display.print("V cal = ");
  display.print(Vcalib, 2); display.print(" mV");
  display.display();
  delay(2000);
  display.clearDisplay();
}

// ---------- He Calibration ----------
void saveHeCalib() {
  magicStore.write(MAGIC_VALUE);
  hecorrStore.write(calibMD62_corr);
}

void loadHeCalib() {
  uint32_t magic = magicStore.read();
  if (magic == MAGIC_VALUE) calibMD62_corr = hecorrStore.read();
}

void calibrateHe() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 0);  display.print("Calibration");
  display.setCursor(10,10);  display.print("He Sensor");
  display.setCursor(10,20);  display.print("(100% He)");
  display.display();
  delay(1000);

  if (bridge < minVHe) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10,20); display.print("Error with He");
    display.setCursor(10,30); display.print("calibration");
    display.setCursor(10,40); display.print("V cal = ");
    display.print(bridge, 2); display.print(" mV");
    display.display();
    delay(10000);
    display.clearDisplay();
    return;
  }

  float Vavg = 0;
  for (int i = 0; i < N_MEASUREMENTS; i++) {
    updateMeasurements();
    Vavg += bridge;
    delay(200);
  }

  Vavg /= N_MEASUREMENTS;
  calibMD62 = Vavg;
  calibMD62_corr = calibMD62 * (100 / 87.083);
  saveHeCalib();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10,20); display.print("He Calibration OK");
  display.setCursor(10,30); display.print("V cal = ");
  display.print(bridge, 2); display.print(" mV");
  display.display();
  delay(2000);
  display.clearDisplay();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(400000L);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  display.begin(I2C_ADDRESS, true);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(10,20); display.print("Kaasuvelho");
  display.setCursor(10,40); display.print("v0.9 beta");
  display.display();
  delay(4000);

  ads.begin();
  updateMeasurements();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 0);  display.print("He Bridge");
  display.setCursor(10,10); display.print("V cell = ");
  display.print(voltage, 2); display.print(" mV");
  display.setCursor(10,20); display.print("V bridge = ");
  display.print(bridge, 2); display.print(" mV");
  display.display();
  delay(2000);

  calibrateO2();
  loadHeCalib();

  while (bridge > 10) {
    updateMeasurements();
    display.clearDisplay();
    display.setCursor(10, 0);  display.print("Preheating");
    display.setCursor(10,20); display.print("Helium sensor...");
    display.setCursor(10,40); display.print("V bridge= ");
    display.print(bridge, 0); display.print(" mV");
    display.display();
    delay(50);
  }

  display.clearDisplay();
  display.setCursor(10,10); display.print("Helium sensor OK");
  display.display();
  delay(1000);

  display.clearDisplay();
  display.setCursor(10,10); display.print("Analyzer ready");
  display.display();
  delay(1000);
  display.clearDisplay();
}

// ---------- Main Loop ----------
void loop() {
  time = millis();
  updateMeasurements();

  float nitrox = voltage * (20.9 / Vcalib);

  display.fillRect(24, 0, 36, 10, SH110X_BLACK);
  display.setTextSize(1);
  display.setCursor(0,0);  display.print("O2: ");
  display.print(nitrox, 1); display.print(" %");

  display.fillRect(36,10,30,10,SH110X_BLACK);
  display.setCursor(0,10); display.print("O2mV ");
  display.print(voltage, 2);

  display.fillRect(90,10,38,10,SH110X_BLACK);
  display.setCursor(66,10); display.print("He mV ");
  display.print(bridge, 0);
  display.display();

  bridge -= bridgeCalib;

  if (time < 480000) TempComp = 0;
  if (time < 360000) TempComp = 2;
  if (time < 300000) TempComp = 3;
  if (time < 270000) TempComp = 4;
  if (time < 240000) TempComp = 5;
  if (time < 210000) TempComp = 6;
  if (time < 180000) TempComp = 7;
  if (time < 165000) TempComp = 8;
  if (time < 150000) TempComp = 9;
  if (time < 120000) TempComp = 10;
  if (time < 105000) TempComp = 11;
  if (time < 90000)  TempComp = 12;
  if (time < 80000)  TempComp = 13;
  if (time < 70000)  TempComp = 14;
  if (time < 60000)  TempComp = 15;
  if (time < 50000)  TempComp = 16;
  if (time < 40000)  TempComp = 17;
  if (time < 30000)  TempComp = 18;

  bridge -= TempComp;

  display.fillRect(90,0,38,10,SH110X_BLACK);
  display.setCursor(66,0);
  display.setTextSize(1);
  display.print("He: ");

  float helium = 100 * bridge / calibMD62_corr;
  if (helium > 50) helium *= (1 + (helium - 50) * 0.4 / 100);

  if (helium > 2) {
    display.print(helium, 1);
    display.print(" %");
  } else {
    helium = 0;
    display.print("0 %");
  }

  display.fillRect(0,25,128,39,SH110X_BLACK);
  display.setCursor(10,25);
  display.setTextSize(2);

  if (helium > 0) {
    display.print("Trimix ");
    display.setCursor(10,45);
    display.print(nitrox, 0);
    display.print(" / ");
    display.print(helium, 0);
  } else {
    display.print("Nitrox ");
    display.setCursor(10,45);
    display.print(nitrox, 0);
  }
  display.display();

  handleButton();
  delay(100);
}
