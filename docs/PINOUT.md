# JC3248W535 pinout used by this project

| Function | GPIO |
|---|---:|
| LCD CS | 45 |
| LCD CLK | 47 |
| LCD D0 | 21 |
| LCD D1 | 48 |
| LCD D2 | 40 |
| LCD D3 | 39 |
| LCD Backlight | 1 |
| Touch SDA | 4 |
| Touch SCL | 8 |
| Touch INT | 3 |
| Touch I2C address | 0x3B |
| CAN TX (-> transceiver CTX/TXD) | 17 |
| CAN RX (<- transceiver CRX/RXD) | 18 |
| SD_MMC CLK | 12 |
| SD_MMC CMD | 11 |
| SD_MMC D0 | 13 |

CAN transceiver: SN65HVD230, Rs tied directly to GND (high-speed mode,
required for 500 kbit/s PT-CAN). Do not add a slope-control resistor on Rs
for this bus speed. No extra 120R termination on the module itself: the
vehicle bus is already terminated at both ends.

