# ESP32 MP3 Player
Here is the code and wiring schematic for my MP3 player, based on the ESP32-C3 Super Mini.
The system reads SD cards up to 32GB (FAT32 format only), organizes files and folders in alphabetical order, and plays all the tracks within a selected folder. It features a navigation manager, recognizes artist and track names (if available), keeps track of elapsed playback time, and includes volume, play, pause, next, and back controls.

Key limitations to keep in mind:
- **No seeking:** The system does not support fast-forwarding or rewinding; tracks play from start to finish;
- **Resource usage:** The file manager is somewhat resource-heavy, so I paused the audio buffer during menu browsing to ensure smooth and pleasant navigation;
- **File compatibility:** Please place only MP3 files in the SD card folders. Other file extensions may cause unexpected behavior.

![MP3 Player Hardware](/MP3_player.png)

 
## Components
- ESP32 C3 Super Mini;
- DAC PCM5102a;
- Monocrome OLED Display I2C 128x64px;
- Standard SPI Micro SD Reader Module;
- TP4056 Batery Recharger;
- Li-Po Rechargeble Batery;
- 3 Tactical Switches;
- 1 Toggle or Slide Switch;
- 1 Linear Potentiometer (100kΩ);
- 2 Ceramic Capacitors(100nF);
- 1 Eletrolytic Capacitor (10μF);

## Libraries
- ESP8266Audio (by Earle F. Philhower, III): Used to read the MP3 data and manage the RAM buffer;
- GyverOLED (by AlexGyver): The lightest ADA Fruit based library to manage the OLED display I could find;
- OneButton (by Matthias Hertel): Used to manage the buttons in a precise and easy way;

## Wiring Schematics
![Wiring Schematics](/Schematics.png)

**Controls**
- Pin 0: Potenciometer
- Pin 1: Button Down
- Pin 2: Button Up
- Pin 8: Button Enter

**DAC**
- Pin 3: DIN
- Pin 20: BCK
- Pin 21: LRCK
- Jumper H4L on LOW State
- XSMT (3) on VIN

**SD Card Module**
- Pin 4: SCK
- Pin 5: MISO
- Pin 6: MOSI
- Pin 7: CS

**OLED Display**
- Pin 9: SCK
- Pin 10: SDA
