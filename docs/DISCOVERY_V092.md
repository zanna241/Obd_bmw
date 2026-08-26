# V0.9.2 BMW Discovery Logger

Questa release serve a raccogliere dati sufficienti per identificare i parametri BMW ancora mancanti senza inventare DID o formule.

## Novita

- refresh valori display: 100 ms (~10 Hz massimo lato GUI)
- web status: 500 ms
- analizzatore passivo per ogni CAN ID ricevuto
- min/max e numero cambi per ciascuno degli 8 byte
- periodo medio osservato per ID
- file `_discovery.csv` creato a fine acquisizione
- file `_events.csv` con marker manuali e snapshot dei dati veicolo
- pagina WEB `DISCOVERY`
- marker rapidi P/R/N/D, SHIFT+/-, ACCEL, RILASCIO, FRENO, STOP
- scan ECU read-only opzionale
- polling lento del responder aftertreatment 0x7EC sui PID standard che il log Car Scanner ha dichiarato supportati: 0x78, 0x85, 0x88

## Scan ECU read-only

Il pulsante SCAN ECU invia esclusivamente UDS TesterPresent `3E 00` ai request ID 0x7E0..0x7E7. Qualunque risposta 0x7E8..0x7EF viene registrata nella response mask.

Non vengono inviati coding, reset, routine, attuazioni o scritture.

## D70BX7A0 / DPF

Nel profilo diagnostico sono catalogati come CANDIDATE i membri confermati del blocco TestO/EDIABAS `D70BX7A0 -> STATUS_BLOCK_LESEN -> DPF`:

- 0x44F8
- 0x4506
- 0x4500
- 0x44BE
- 0x44C4
- 0x5308
- 0x44BC
- 0x44B7
- 0x44BB

IMPORTANTE: non vengono trasmessi automaticamente come UDS DID. Sono ID interni del job BMW e la loro associazione a soot/ash/distanza/etc. deve ancora essere estratta/validata.

## File prodotti dal logger

Per ogni sessione:

- `_raw.csv`: ogni frame TX/RX
- `_catalog.csv`: catalogo CAN finale
- `_decoded.csv`: modello dati decodificato
- `_events.csv`: marker manuali con timestamp
- `_discovery.csv`: analisi per ID e byte

## Procedura consigliata ZF8

Avvia il logger e marca gli eventi dalla pagina web:

1. P per 10 s
2. R per 10 s
3. N per 10 s
4. D fermo per 10 s
5. partenza dolce, marker ACCEL
6. durante cambiate evidenti usa SHIFT+
7. rilascio lungo, marker RILASCIO
8. frenata, marker FRENO
9. se usi manuale: M1/M2/... come marker personalizzati

Questo permette di correlare gli ID/byte passivi con marcia, input/output rpm, lock-up e slip se tali segnali sono esposti sulla presa OBD/gateway.
