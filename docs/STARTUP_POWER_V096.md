# V0.9.6 STARTUP / POWER / LIVE STABILITY

Correzioni principali:

- LVGL heap portato da 64 KiB a 192 KiB.
- Le pagine LIVE e GRAFICI vengono create solo quando aperte.
  Questo evita che l'aggiunta delle nuove pagine saturi l'heap LVGL durante
  `gui_init()` e lasci il pannello nero dopo lo splash.
- HOME viene renderizzata con più passate LVGL immediate prima di avviare
  Wi-Fi, CAN e logger.
- Aggiunte stampe seriali `GUI: init start` e `GUI: HOME ready` con heap/PSRAM.
- Dopo un wake EXT1 da GPIO18 viene eseguito `rtc_gpio_deinit(GPIO18)` prima
  dell'installazione TWAI. L'ESP32-S3 lascia infatti il pin di wake in modalità
  RTC IO dopo il deep sleep.
- `power_manager_begin()` viene eseguito prima di `can_init()`.
- Restano attivi: invalidazione dati dopo perdita ECU, backlight off, deep
  sleep CAN wake, grafico 2 ore, LIVE peak hold, temi giorno/notte, logger,
  discovery, Wi-Fi/web/OTA.

Diagnostica seriale attesa all'avvio:

    GUI: init start heap=... psram=...
    GUI: HOME ready heap=... psram=...
    Web server started on port 80
    SYSTEM READY

Se `GUI: HOME ready` compare ma lo schermo rimane nero, il problema non e'
piu' nella costruzione dell'albero LVGL e va verificato il flush/display.
