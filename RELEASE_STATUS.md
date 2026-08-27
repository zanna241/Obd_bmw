# Stato release V0.9.9.4 OTA RECOVERY

- Sorgente: pronto
- Versione: 0.9.9.4
- Ambiente PlatformIO: `jc3248w535`
- Pin CAN: TX GPIO17 / RX GPIO18
- BIN GitHub Actions: generato e verificato
- Dimensione BIN: 1.945.056 byte
- SHA-256 BIN: `0be7be94b089bbebc093d81d40fd2e2af49e8cf004071ce1bac8c1944c98642d`
- Build validata: GitHub Actions run 33054146703
- Modalita banco: comando touch persistente `DEEP SLEEP CAN` (`AUTO CAN` / `BANCO`)
- Build locale non necessaria: il core Arduino ESP32 e stato risolto da GitHub Actions
- Build automatica GitHub: `.github/workflows/release.yml`

Non rinominare un file qualsiasi in `.bin`: il firmware installabile deve essere
prodotto da PlatformIO o dalla GitHub Action inclusa.
