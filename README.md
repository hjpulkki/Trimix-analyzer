# DIY Trimix Analyser 
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC--BY--NC--SA%204.0-green)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Hardware Project](https://img.shields.io/badge/Open%20Hardware-Yes-blue)]()
> ⚠️ Non-commercial open hardware/software project: if you plan to build or sell analyzers commercially, contact the original authors for licensing.

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

## Code C++ (Arduino)

- [Sketch Trimix-analyzer.ino](https://github.com/captainigloo/Trimix-analyzer/blob/master/src/trimix-analyzer.ino)
- [Further developed version](https://www.printables.com/model/677331-trimix-analyser-v2)
- Any changes after that are made by [me](https://github.com/hjpulkki/) and can be found from [this repository](https://github.com/hjpulkki/Trimix-analyzer/blob/master/src/trimix-analyzer.ino)

## Bill of materials

<img src="https://github.com/captainigloo/Trimix-analyzer/blob/master/images/fritzing.png" height="500">

- LCD 20x4 [HD44780 20x4 I2C Interface](https://www.amazon.com/s/ref=nb_sb_noss?url=search-alias%3Daps&field-keywords=HD44780+20x4+I2C+Interface) or [compatible I²C](https://fr.aliexpress.com/item/Free-shipping-LCD-module-Blue-screen-IIC-I2C-2004-5V-20X4-LCD-board-provides-library-files/1873368596.html)
- [Oxygen Sensor AO2 ptb-18.10](https://fr.aliexpress.com/item/City-sensor-ao2-ptb-18-10-oxygen-sensor/1258183473.html)
- [ADS1115 16-Bit ADC](https://fr.aliexpress.com/item/ADS1115-ADC-ultra-compact-16-precision-ADC-module-development-board/32309705230.html)
- [Carbon Dioxide gas sensor MD62](https://fr.aliexpress.com/item/heat-conduction-CO2-Carbon-Dioxide-gas-sensor-MD62/32808216273.html)
- [Flow reducer DIN 5/8](https://www.innodive.com/store/analyseurs-o2-et-he-48/reducteur-de-flux-921.html)
- [LM2596 step down power module DC-DC converter(Board based LM2596)](https://www.amazon.com/s/ref=nb_sb_noss?url=search-alias%3Daps&field-keywords=LM2596) 

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

All changes published under the terms of the
**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.**
