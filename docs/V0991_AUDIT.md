# V0.9.9.1 AUDIT

- Fix backend route `/api/diag/bmw-ext-scan`.
- Central firmware version: 0.9.9.1 / V0991.
- Log filenames include V0991.
- Power engine-active state now follows DDE replies (`lastDdeReplyMs`), not any OBD responder.
- Display brightness applies immediately when the active day/night slider changes.
- AP starts first; STA is delayed 2.5 s so SoftAP/DHCP can settle first.
- Wi-Fi scan converted to asynchronous polling.
- HOME AP indicator: yellow with zero clients, green with one or more associated clients.
- Settings includes RIAVVIA SCHEDA and stops logger before restart.
- RAW logger requests its 32 KiB buffer from PSRAM, with 4 KiB internal-RAM fallback.
- BMW Extended scan remains deliberately read-only (TesterPresent only) for first routing validation.
