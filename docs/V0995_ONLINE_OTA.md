# V0.9.9.5 ONLINE OTA

## Interfaccia touch

IMPOSTAZIONI e organizzata in due colonne e tre righe, con pulsanti da 212 x
70 pixel per migliorare l'uso del touch:

Il titolo e il sottotitolo sono spostati a destra per lasciare libera l'area
del logo BMW.

| Riga | Colonna sinistra | Colonna destra |
|---|---|---|
| 1 | WI-FI | DATA LOGGER |
| 2 | DISPLAY | DEEP SLEEP CAN |
| 3 | AGGIORNAMENTO OTA | RIAVVIA SCHEDA |

Premendo AGGIORNAMENTO OTA il monitor legge il manifest pubblico. Se la
versione online e maggiore di quella installata mostra versione, note e
dimensione, poi richiede `SCARICA E INSTALLA` oppure `ANNULLA`.

## Controlli di integrita

- connessione Wi-Fi STA richiesta;
- logger fermato e chiuso prima della scrittura;
- dimensione HTTP confrontata con il manifest;
- SHA-256 calcolato durante il download e confrontato con il manifest;
- `Update.end(true)` richiesto prima del riavvio;
- in caso di errore la partizione attiva non viene sostituita.

## Canale pubblico

La GitHub Action pubblica dopo ogni build riuscita di `main`:

- `firmware/manifest.json`;
- `firmware/BMW_520xd_MONITOR_latest.bin`;
- `firmware/BMW_520xd_MONITOR_latest.bin.sha256`.

Il commit automatico modifica soltanto `firmware/` ed e escluso dai trigger di
build, evitando un ciclo ricorsivo.
