# V0.9.7 CONSOLIDATED

Base: V0.9.6.2, validated on the vehicle for boot, deep sleep and CAN wake.

## Power state
- `ENGINE ACTIVE`: valid DDE replies.
- `ENGINE OFF / CAN AWAKE`: DDE no longer valid but physical CAN frames still arrive.
- `BUS QUIET`: CAN has stopped.
- `BUS SLEEP`: deep sleep is entered only after at least 45 s offline AND 10 s of real bus silence.
- Live measurements are invalidated after 5 s without DDE replies.
- Backlight turns off after 10 s offline.
- GPIO18 EXT1 CAN wake remains unchanged.

## Day/night fix
The V0.9.6.2 lazy-GUI conversion left the DISPLAY overlay unbuilt: the DISPLAY button
therefore had no object to open. V0.9.7 creates DISPLAY on first use. Wi-Fi overlay
gets the same lazy-build fix.

Modes:
- GIORNO: forced high-contrast white theme.
- NOTTE: forced BMW-inspired amber/orange theme.
- AUTO: BMW source only when a real BMW light-state mapping becomes available;
  otherwise local clock 07:00-19:00 is the explicit fallback.
The API now reports `display_theme_source` and `power_state`.

## Existing functions retained
- 2 h rolling history (720 samples at 10 s).
- LIVE + 5 s peak hold.
- unified VehicleData for display/web/logger.
- ISO-TP, DDE/7EC polling, read-only ECU scan.
- RAW/decoded/events/discovery logging.
- lazy LVGL pages to stay inside the 64 KiB LVGL pool.
- boot black -> splash -> HOME.

## BMW/EGS missing values
Oil temperature, soot/ash, regen distance/life and ZF8-specific values remain
explicitly unmapped until a verified BMW diagnostic request/decoder is available.
The firmware keeps them as NAN/-- rather than fabricating DIDs or scalings.
Discovery logging and ECU scan remain enabled to collect the evidence needed for
the next mapping step.
