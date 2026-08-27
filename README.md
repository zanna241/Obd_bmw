# BMW 520xd Monitor V0.9.9.4 OTA RECOVERY

Release di stabilita derivata dalla V0.9.9.1 AUDIT e verificata sui log reali
del 26 agosto 2026.

La V0.9.9.3 aggiunge la modalita banco persistente: dal display touch si puo
disabilitare il deep sleep automatico legato all'assenza CAN.
In modalita banco la retroilluminazione resta attiva, mentre i dati motore non
piu aggiornati continuano a essere invalidati per evitare valori falsi.

La V0.9.9.4 sostituisce l'upload AJAX con una pagina OTA nativa dedicata,
compatibile con Chrome su iPhone e priva del polling della dashboard durante il
trasferimento. Elimina inoltre la password fissa: la protezione web si configura
per dispositivo e puo essere disattivata consapevolmente.

## Modifiche principali

- mantiene MCP2562FD / TWAI 500 kbit/s e tutto il Diagnostic Engine della V0.9.x;
- mantiene logger RAW, decoded, events, catalog e discovery;
- mantiene ECU Scan READ-ONLY cooperativo;
- WebServer servito prima e dopo il blocco CAN/LVGL per ridurre starvation;
- buffer RAW microSD portato a 32 KB e flush meno frequenti;
- nessun `flush()` microSD forzato dai callback dei marker;
- Web UI con protezione dai fetch sovrapposti e polling alleggerito;
- durante ECU scan la Web UI sospende catalog/discovery pesanti, lasciando vivo `/api/status`;
- HOME: titoli a due righe per ARIA ASPIRATA, OLIO MOTORE e DPF TRIGGER;
- ZF8: OLIO CAMBIO su due righe;
- nuova pagina touch DISPLAY con slider Giorno/Notte e pulsanti +/-;
- luminosita salvate in Preferences;
- tema AUTO / GIORNO / NOTTE;
- tema Giorno ad alto contrasto bianco, tema Notte BMW-inspired ambra/arancio su nero;
- AUTO predisposto per futuro segnale BMW; fino alla sua decodifica usa l'orario locale 07:00-19:00 quando disponibile;
- fix boot: retroilluminazione spenta, frame nero inviato prima di renderla visibile, poi splash e HOME;
- refresh valori display fino a circa 10 Hz, aggiornando solo la pagina visibile.
- download CSV consentito solo a logger fermo;
- trasferimento CSV a blocchi da 2 KB con servizio CAN tra i blocchi;
- flush periodico microSD portato a 15 secondi;
- misurazione di write/flush SD e contatore stalli oltre 250 ms;
- arresto e chiusura pulita del logger prima di un aggiornamento OTA;
- discovery BMW estesa mantenuta per EGS/ZF8, GWS e KOMBI;
- pinout CAN definitivo: TX GPIO17, RX GPIO18, MCP2562FD.
- pagina `/ota` minimale con form multipart nativo, senza XMLHttpRequest;
- autenticazione web persistente con utente `admin` e password personale;
- possibilita di disattivare l'autenticazione dalla pagina SISTEMA;
- primo avvio V0.9.9.4 senza password, per consentire la configurazione iniziale.

Vedere `docs/V0994_OTA_RECOVERY.md` per dettagli e procedura di prova.
