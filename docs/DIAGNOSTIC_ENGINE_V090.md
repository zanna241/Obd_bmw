BMW 520xd Monitor V0.9.0 - Diagnostic Engine

IMPLEMENTATO
- CAN 11-bit 500 kbit/s su MCP2562FD / TWAI ESP32-S3.
- Richieste OBD fisiche alla DDE 0x7E0, risposta primaria 0x7E8.
- Discovery iniziale funzionale 0x7DF / PID 0100.
- ISO-TP Single Frame e Multi Frame con Flow Control.
- Unico VehicleData condiviso da display, web, history, trip e allarmi.
- Parametri OBD standard avanzati diesel supportati dalla vettura:
  04 load, 05 coolant, 0C RPM, 0D speed, 0F IAT, 10 MAF,
  11 throttle, 1F runtime, 33 BARO, 42 ECU voltage, 45 throttle,
  46 ambient, 69 EGR, 6D fuel rail, 70 boost pressure,
  78 EGT bank 1, 7A DPF differential pressure, 8B diesel aftertreatment.
- DPF regen status/type, normalized trigger, average regen time/distance
  quando i relativi support bit SAE sono presenti.
- Web e display leggono gli stessi campi.
- History 24h estesa a RPM, speed, rail, DPF differential pressure, EGT.
- /api/diag/profile espone il catalogo parametri e il livello di confidenza.

ANCORA DA MAPPARE CON DATI BMW D70BX7A0 / EGS
- temperatura olio motore (PID standard 5C non dichiarato dalla support map dell'auto)
- soot mass e ash mass
- distanza esatta dall'ultima rigenerazione e vita DPF residua
- temperatura DPF specifica ingresso/uscita se non identificabile con certezza dagli EGT
- marcia ZF8, ATF, input/output RPM, converter slip, lock-up, torque EGS

Questi campi sono gia presenti nel modello dati, display e Web API ma restano NAN/--:
nessun valore viene inventato e nessun DID BMW sconosciuto viene trasmesso automaticamente.
