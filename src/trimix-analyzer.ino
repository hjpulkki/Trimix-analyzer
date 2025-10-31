// Trimix Analyzer
// Original: Yves Caze, Savoie Plongee
// Mods: GoDive BRB (2021), Dominik Wiedmer (2021–2024), Heikki Pulkkinen (2025)
// Version 1.1 4.10.2021
// Version 1.2 7.10.2023 Change of library names, Calibration goal less strict
// Version 20240601 Added calc for calibration offset, so that the value to be entered is =mV@100% He
// added He 0 calib during O2 calib & Message if O2 < 7mV and no calibration
// Version 20241017 remove blinking of display
// Version 20251031 add option for 100% He calibration by long press, translate everything to Enlish etc.

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
#define N_MEASUREMENTS 10

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_ADS1115 ads;

// -------- Calibration & constants --------
float Vcalib = 0;          // mV reading for 20.9% O2 (air)
float voltage = 0;         // O2 sensor voltage (mV)
float bridge = 0;          // He sensor bridge voltage (mV)
float minVO2 = 7.00;       // Minimum valid O2 sensor voltage for air
float minVHe = 200;        // Minimum valid He sensor voltage for 100% Helium
float bridgeCalib = 0;     // Offset for He bridge
float TempComp = 0;        // Temperature compensation (time-based)
unsigned long time;

// Initial values which are used before first He calibration
float calibMD62 = 595.56;  // Measured mV @ 100% He (user-calibrated)
float calibMD62_corr = calibMD62 * (100 / 87.083);   // adjust so user enters real mV@100%He

FlashStorage(magicStore, uint32_t);
FlashStorage(hecorrStore, float);

char user[] = "Kaasuvelho v0.9 beta";

RunningAverage RA0(10);     // Moving average for O2
RunningAverage RA1(10);     // Moving average for He

int16_t adc0, adc1;

void handleButton() {
  static bool buttonPressed = false;
  static bool longPressTriggered = false;
  static unsigned long pressStartTime = 0;

  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && !buttonPressed) {
    // Button just pressed
    buttonPressed = true;
    longPressTriggered = false;
    pressStartTime = millis();
  }

  if (buttonPressed && buttonState == LOW) {
    // Button is still being held
    unsigned long pressDuration = millis() - pressStartTime;

    if (!longPressTriggered && pressDuration >= LONG_PRESS_TIME) {
      longPressTriggered = true;
      calibrateHe();  // trigger long press immediately
    }
  }

  if (buttonPressed && buttonState == HIGH) {
    // Button released
    unsigned long pressDuration = millis() - pressStartTime;
    buttonPressed = false;

    // Only trigger short press if long press wasn’t already handled
    if (!longPressTriggered && pressDuration < LONG_PRESS_TIME) {
      calibrateO2();
    }
  }
}

void updateMeasurements() {
  // --- Channel 0–1: 0–50 mV ---
  ads.setGain(GAIN_SIXTEEN);                 // ±0.256 V range
  int16_t adc0 = ads.readADC_Differential_0_1();
  RA0.addValue(adc0);
  voltage = RA0.getAverage() * (0.256 / 32768.0 * 1000);

  // Voltage is negative only if the sensor is plugged in the wrong way
  voltage = abs(voltage)

  // --- Channel 2–3: 0–650 mV ---
  ads.setGain(GAIN_FOUR);                    // ±1.024 V range
  int16_t adc1 = ads.readADC_Differential_2_3();
  RA1.addValue(adc1);
  bridge = RA1.getAverage() * (1.024 / 32768.0 * 1000);
}

// ---------- O2 Calibration ----------
void calibrateO2() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.print("Calibration");
  display.setCursor(10, 10);
  display.print("O2 Sensor");
  display.setCursor(10, 20);
  display.print("(Air 20.9% O2)");
  display.display();
  delay(1000);
  

// Check if O2 cell is too weak
if (voltage < minVO2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.print("Error with O2");
  display.setCursor(10, 30);
  display.print("calibration");
  display.setCursor(10, 40);
  display.print("V cal = ");
  display.print(voltage, 2);
  display.print(" mV");
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

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.print("Calibration OK");
  Vavg = Vavg / N_MEASUREMENTS;
  Vcalib = Vavg; // store reference voltage for 20.9% O2

  display.setCursor(10, 30);
  display.print("V cal = ");
  display.print(Vcalib, 2);
  display.print(" mV");
  display.display();
  delay(2000);
  display.clearDisplay();
}

void saveHeCalib() {
  magicStore.write(MAGIC_VALUE);
  hecorrStore.write(calibMD62_corr);
}

void loadHeCalib() {
  uint32_t magic = magicStore.read();

  if (magic == MAGIC_VALUE) {
    // Load only if EEPROM has been written by saveHeCalib
    calibMD62_corr = hecorrStore.read();
  }}

void calibrateHe() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.print("Calibration");
  display.setCursor(10, 10);
  display.print("He Sensor");
  display.setCursor(10, 20);
  display.print("(100% He)");
  display.display();
  delay(1000);
  
  if (bridge < minVHe){
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.print("Error with He");
    display.setCursor(10, 30);
    display.print("calibration");
    display.setCursor(10, 40);
    display.print("V cal = ");
    display.print(bridge, 2);
    display.print(" mV");
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
  Vavg = Vavg/N_MEASUREMENTS;

  calibMD62 = Vavg;  // Measured mV @ 100% He
  calibMD62_corr = calibMD62 * (100 / 87.083);   // adjust so user enters real mV@100%He
  saveHeCalib(); // Save value in EEPROM.

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.print("He Calibration OK");
  display.setCursor(10, 30);
  display.print("V cal = ");
  display.print(bridge, 2);
  display.print(" mV");
  display.display();
  delay(2000);
  display.clearDisplay();
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
  display.setCursor(10, 20);
  display.print("Kaasuvelho");
  display.setCursor(10, 40);
  display.print("v0.9 beta");
  display.display();
  delay(4000);

  ads.begin();
  updateMeasurements();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.print("He Bridge");
  display.setCursor(10, 10);
  display.print("V cell = ");
  display.print(voltage, 2);
  display.print(" mV");
  display.setCursor(10, 20);
  display.print("V bridge = ");
  display.print(bridge, 2);
  display.print(" mV");
  display.display();
  delay(2000);

  calibrateO2(); // run O2 calibration on startup

  loadHeCalib();

  // wait until He sensor warms up
  while (bridge > 10) {
    updateMeasurements();
    display.clearDisplay();
    display.setCursor(10, 0);
    display.print("Preheating");
    display.setCursor(10, 20);
    display.print("Helium sensor...");
    display.setCursor(10, 40);
    display.print("V bridge= ");
    display.print(bridge, 0);
    display.print(" mV");
    display.display();
    delay(50);
  }

  display.clearDisplay();
  display.setCursor(10, 10);
  display.print("Helium sensor OK");
  display.display();
  delay(1000);

  display.clearDisplay();
  display.setCursor(10, 10);
  display.print("Analyzer ready");
  display.display();
  delay(1000);
  display.clearDisplay();
}

// ---------- Main Loop ----------
void loop() {
  // Read raw ADC values
  time = millis();
  updateMeasurements();

  // --- O2 calculation ---
  float nitrox = voltage * (20.9 / Vcalib);  // scale by calibration value

  // --- Display raw values ---
  // --- O2 % display ---
  display.fillRect(24, 0, 36, 10, SH110X_BLACK);  // clear numeric area
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("O2: ");
  display.print(nitrox, 1);
  display.print(" %");

  // --- O2 mV display ---
  display.fillRect(36, 10, 30, 10, SH110X_BLACK);
  display.setCursor(0, 10);
  display.print("O2mV ");
  display.print(voltage, 2);

  // --- He mV display ---
  display.fillRect(90, 10, 38, 10, SH110X_BLACK);
  display.setCursor(66, 10);
  display.print("He mV ");
  display.print(bridge, 0);

  display.display();

  // --- He bridge corrections ---
  bridge -= bridgeCalib;     // subtract stored offset
  // Simple time-based temperature compensation
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

  bridge -= TempComp; // apply compensation

  // --- He percentage calculation ---
  display.fillRect(90, 0, 38, 10, SH110X_BLACK);
  display.setCursor(66, 0);
  display.setTextSize(1);
  display.print("He: ");

  float helium = 100 * bridge / calibMD62_corr;        // linear %He estimate

  // Correct nonlinearity above 50% He
  if (helium > 50)
    helium = helium * (1 + (helium - 50) * 0.4 / 100);

  if (helium > 2) {
    display.print(helium, 1);
    display.print(" %");
  } else {
    helium = 0;
    display.print("0 %");
  }
  display.display();

  // --- Bottom gas mix display ---
  display.fillRect(0, 25, 128, 39, SH110X_BLACK);  // clear full lower area
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
  handleButton();

  delay(100);
}
