# V0.9.5 POWER / LIVE / 2H GRAPH

- Live data invalidated after 5 s without valid OBD reply (shows -- instead of stale values).
- Backlight off after 10 s offline.
- Deep sleep after 45 s offline. Logger is cleanly closed first.
- ESP32-S3 wakes through EXT1 on GPIO18 LOW. GPIO18 is RTC capable on ESP32-S3.
- MCP2562FD remains externally powered; RXD is recessive high on idle CAN and dominant low activity wakes the ESP32.
- 2-hour rolling history: 10 s sample, 720 samples. Chart refreshes while visible.
- New LIVE page: RPM, boost, rail, EGT, DPF differential pressure, coolant with 5 s peak hold.

Important: this hardware module exposes no STBY pin, so the MCP2562FD remains in normal mode. ESP32 deep sleep reduces MCU/display load substantially, but transceiver + buck quiescent current remain. For minimum parked current, a future hardware revision should expose/control STBY or use switched power.
