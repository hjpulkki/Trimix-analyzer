# DIY Trimix Analyser 
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC--BY--NC--SA%204.0-green)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Hardware Project](https://img.shields.io/badge/Open%20Hardware-Yes-blue)]()
> ⚠️ Non-commercial open hardware/software project: if you plan to build or sell analyzers commercially, contact the original authors for licensing.

This repository now bundles both the advanced and simplified firmware in the same branch. The advanced build can be 0–5% more accurate than the simplified version, depending on the gases and your sensors. Before enabling the advanced features, consider whether the added calibration steps and complexity are worth the marginal gain; the advanced mode is best for experimenting with the algorithms yourself.

### Selecting advanced vs simplified firmware features

The firmware now has a compile-time toggle in `src/trimix-analyzer/trimix-analyzer.ino`. Set `ADVANCED_FEATURES` to `1` (default) to enable the quadratic oxygen calibration, helium compensation and calibration menu from this branch. Set it to `0` to build the simplified/master behaviour (single-button calibration flow and linear O₂ mapping).

<img src="https://github.com/captainigloo/Trimix-analyzer/blob/master/images/trimix.jpg" height="300">

## Définition of Trimix

Trimix is a breathing gas consisting of oxygen, helium and nitrogen and is often used in deep commercial diving, during the deep phase of dives carried out using technical diving techniques, and in advanced recreational diving.

The helium is included as a substitute for some of the nitrogen, to reduce the narcotic effect of the breathing gas at depth. With a mixture of three gases it is possible to create mixes suitable for different depths or purposes by adjusting the proportions of each gas. Oxygen content can be optimised for the depth to limit the risk of toxicity, and the inert component balanced between nitrogen (which is cheap but narcotic) and helium (which is not narcotic and reduces work of breathing, but is more expensive and increases heat loss).

The mixture of helium and oxygen with a 0% nitrogen content is generally known as Heliox. This is frequently used as a breathing gas in deep commercial diving operations, where it is often recycled to save the expensive helium component. Analysis of two-component gases is much simpler than three-component gases.

<img src="https://github.com/captainigloo/Trimix-analyzer/blob/master/images/IMCA_Trimix_shoulder_quartered.svg.png" width="200"><img src="https://github.com/captainigloo/Trimix-analyzer/blob/master/images/IMCA_Trimix_shoulder.svg.png" width="200">

## Calculation method

The method for evaluating the proportion (%) of Helium in the Trimix mixture is special. The MD62 sensor measures the proportion of all gases except nitrogen and oxygen. The AO2 sensor measures the proportion (%) of oxygen in the mixture. Ignoring the rare gases, we can deduce that on **[100% of Trimix] = [%He +%O2 +%N2]**

Or **[%N2] = [100% of Trimix] - [%He +%O2]**

## Genesis

- [Point of origin project](https://github.com/captainigloo/Trimix-analyzer/blob/master/genesis/readme.md)

## Bill of Materials

Updated list of materials based on lising in [Printables: Trimix Analyser V2](https://www.printables.com/model/677331-trimix-analyser-v2). I recommend starting the project by taking time to order EVERYTHING on this list, since getting them will take some time. Meanwhile, you can print the 3D parts from the printable project.

### Main Analyzer Parts

| Label              | Part Type                         | Link                                                                | Notes                                                                               |
| ------------------ | --------------------------------- | ------------------------------------------------------------------- | ----------------------------------------------------------------------------------- |
| He Sensor          | MD62                              | [AliExpress](https://www.aliexpress.com/item/32725880118.html)      | Most expensive part                                                                 |
| O₂ Sensor          | Original CITY AO2                 |                                                                     | Old cells from JJ CCR (or any other CCR probably)                                   |
| U1                 | Voltage Regulator – LM316 / LM317 | [AliExpress](https://www.aliexpress.com/item/32364424866.html)      | LM317 in the kit works                                                              |
| Seeduino           | Seeeduino XIAO SAMD21             | [AliExpress](https://www.aliexpress.com/item/1005009296423418.html) |                                         |
| ADS1115            | 16-bit I2C ADC                    | [AliExpress](https://www.aliexpress.com/item/1005005973975124.html) | —                                                                                   |
| Display            | 1.3’’ OLED 128×64                 | [AliExpress](https://www.aliexpress.com/item/32844104782.html)      | Verify correct size and 4‑pin version                                               |
| Terminals          | 3×2P & 2×3P                       | [AliExpress](https://www.aliexpress.com/item/1005001711075410.html) | Wrong part! Pin spacing must be 2.5 mm (0.1")                                       |
| Calibration Button | 12 mm, non‑LED, 220 V             | [AliExpress](https://www.aliexpress.com/item/1005002579155216.html) |                                                                                     |
| R10 & R4           | 1 kΩ trim pots (2×)               | [AliExpress](https://www.aliexpress.com/item/32956113030.html)      | Needs two                                                                           |
| C1                 | Ceramic Capacitor 100 nF          | —                                                                   | —                                                                                   |
| C2                 | Electrolytic Capacitor 1 µF       | —                                                                   | —                                                                                   |
| R5 & R7            | 2.2 kΩ resistors (2×)             | —                                                                   | Needs two                                                                           |
| R8                 | 10 kΩ resistor                    | —                                                                   | brown‑black‑black‑red‑brown                                                         |
| R9                 | 220 Ω resistor                    | —                                                                   | red‑red‑brown‑gold                                                                  |
| PCB                | PCB (Gerbers on Printables)       | Order from [JLCPCB](https://jlcpcb.com/)                            | Gerber files available on [Printables](https://www.printables.com/model/677331-trimix-analyser-v2) |
| Inserts            | M3×12 inserts or print‑in threads | —                                                                   | Alternate: updated 3D model for direct screws                                       |

### Battery & Portable Power Parts

You could use your analyzer with just an USB-C power source, but if you want to make it portable you will also need.

| Label                      | Part Type            | Link                                                                | Notes                                        |
| -------------------------- | -------------------- | ------------------------------------------------------------------- | -------------------------------------------- |
| Battery Capacity Indicator | 1S Li‑ion gauge      | [AliExpress](https://www.aliexpress.com/item/1005004641790492.html) | Ensure correct voltage                       |
| Battery Holder             | 1× 18650 holder      | [AliExpress](https://www.aliexpress.com/item/4000169485668.html)    | —                                            |
| Battery Charging Board     | USB‑C Li‑ion charger | [AliExpress](https://www.aliexpress.com/item/1005002925698704.html) | —                                            |
| ON/OFF Switch              | 2‑pin switch         | [AliExpress](https://www.aliexpress.com/item/4000896270246.html)    | —                                            |
| DC‑DC Step‑Up Converter    | MT3608               | [AliExpress](https://www.aliexpress.com/item/4001066566291.html)    |                                              |
| 18650 Battery              | Li‑ion cell          |                                                                     | —                                            |

### Wing Inflator Adapter Parts

The Wing Inflator Adapter is in alternative of analysing directly from the tank. I have not built one myself.

| Label                 | Part Type     | Link                                                                | Notes                                              |
| --------------------- | ------------- | ------------------------------------------------------------------- | -------------------------------------------------- |
| BCD Inflator Adapter  | 3/8‑24 UNF‑2A | [AliExpress](https://www.aliexpress.com/item/1005002320500288.html) |                                                    |
| Pneumatic Hose 8×5 mm | Hose (~40 cm) | [AliExpress](https://vi.aliexpress.com/item/1005004003104058.html)  |                                                    |

### Build & Usage Instructions

#### Build Steps

1. Solder all components onto the PCB.
2. Connect the Seeeduino to a USB power source.
3. Adjust the voltage regulator resistor R10 until 3 V is measured across the outer pins of the helium sensor connector.
4. Flash the Seeeduino with the firmware from this repository.
5. Connect the display and confirm the code runs correctly.
6. Connect the sensors and calibration button.
7. Adjust the helium zero trimmer resistor R4 until the He mV value reads close to 0 in air.
8. If necessary, re‑adjust the voltage regulator with all components connected.
9. Allow the MD62 helium sensor to burn in for 24 hours when new.

#### Portable Power Build Steps

1. Place power button and battery indicator inside the 3D‑printed case before soldering.
2. Solder the components together following the [wiring diagram](https://files.printables.com/media/prints/677331/other_files/8853292_c6a8bcd0-0fd8-4512-86f6-88bf55c12c94_12faf253-9704-45c0-afc2-a6fda47f07d9/powersupply_tmx_analyser.pdf).
3. Insert the 18650 battery into the holder.
4. Adjust the MT3608 boost converter output to 5 V.
5. Connect the power module output wires to the PCB.

#### Calibration

1. On boot, the device performs an air calibration: it calibrates the O₂ cell and sets the helium zero point.
2. Run air calibration. Do this if the O₂ reading has drifted, the sensor warmed further, or you want to calibrate with flowing gas instead of static gas.
3. Run He calibration. Before starting, ensure that the air calibration is done properly, helium is flowing and the readings are stable. Run this at least once — maximum MD62 span varies. The value is stored in flash and persists across power cycles.
4. Run oxygen calibration. Before starting, ensure that the air calibration is done properly, oxygen is flowing and are the readings are stable. This fits a quadratic function to account for non-linearity for older cells. The analyzer will keep using the coefficient b even with after any subsequent calibrations with air.
5. Oxygen calibration will also compensate helium readings based on O₂ values. After the calibration the sensor should read Nitrox 100%.

## Credits / License

This project is based on the original **DIY Trimix Analyzer** by  
**Sébastien Joly (2018)**  
https://github.com/captainigloo/Trimix-analyzer  

Licensed under **CC BY-NC-SA 4.0**  
https://creativecommons.org/licenses/by-nc-sa/4.0/

### Original Author
- Sébastien Joly
- Includes inspiration and prior concept reference from community sources

### Contributors / Fork History
- Yves Caze, Savoie Plongée — Original concept reference
- GoDive BRB — Modifications (2021)
- Dominik Wiedmer — Modifications (2021–2024), [3D printable case](https://www.printables.com/model/677331-trimix-analyser-v2?fbclid=IwY2xjawMk441leHRuA2FlbQIxMQABHid5aeiKY0CK5PrPJR7y1q0T8tPfb6r4xdhz_5JM70Wh1BE27sd93Ea1VcRq_aem_aEb72HYW_3OKoap3HzTuhw)
- Heikki Pulkkinen — Further development (2025 →)

### Version History
- v1.1 (2021-10-04) — Community improvements
- v1.2 (2023-10-07) — Library cleanup, calibration tolerance update
- v20240601 — Add calibration offset calc, O₂ <7mV guard message
- v20241017 — Remove display blinking
- v20251031 — Option for 100% He calibration by long press, UI to English
- v20251031b — Display refactor: always show O₂ mV & He mV at top
- Advanced version — Quadratic o2 calibration and oxygen compensation

All changes published under the terms of the
**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.**
