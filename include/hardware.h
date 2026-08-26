#pragma once

#define UI_WIDTH   480
#define UI_HEIGHT  320

// JC3248W535 P3 connector
// P3: GND / 3V3 / IO17 / IO18
// Physical wiring confirmed on real hardware: GPIO17 -> transceiver CTX/TXD,
// GPIO18 <- transceiver CRX/RXD. Do not swap without also re-wiring the
// physical connector, or the TWAI driver will fight the transceiver's own
// output driver on the RX line (bus contention).
#define CAN_TX_PIN 17
#define CAN_RX_PIN 18

#define CAN_BITRATE 500000
