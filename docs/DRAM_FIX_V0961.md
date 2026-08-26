# V0.9.6.1 DRAM FIX

Il linker della V0.9.6 falliva con:

    region `dram0_0_seg' overflowed by 67352 bytes

Causa: `LV_MEM_SIZE` era stato portato da 64 KiB a 192 KiB. Il pool statico
LVGL finisce nella `.dram0.bss`, quindi aggiunge 128 KiB di DRAM interna
statica anche se la board dispone di PSRAM.

Correzione:
- `LV_MEM_SIZE` riportato a 64 KiB.
- Rimane il lazy-loading introdotto in V0.9.6: LIVE e GRAFICI non vengono
  costruite al boot, quindi il pool LVGL non deve contenere contemporaneamente
  anche le due pagine più pesanti.
- Rimangono il fix EXT1/GPIO18, l'ordine power-before-TWAI e la diagnostica
  seriale di startup.

Questa release corregge il linker senza rimuovere le correzioni strutturali
della V0.9.6.
