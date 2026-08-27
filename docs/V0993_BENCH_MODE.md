# V0.9.9.3 BENCH MODE

## Uso dal display

Aprire IMPOSTAZIONI e premere `DEEP SLEEP CAN`.

- `AUTO CAN`: comportamento normale in auto; spegnimento retroilluminazione e
  deep sleep quando DDE e bus CAN restano inattivi per i tempi previsti.
- `BANCO`: deep sleep e spegnimento automatico retroilluminazione disabilitati.

La scelta e salvata nelle Preferences ESP32 e rimane valida dopo un riavvio o
un aggiornamento dell'alimentazione.

## Sicurezza dati

La modalita banco non inventa dati CAN. Dopo 5 secondi senza risposta DDE i
valori live vengono comunque invalidati. La modifica riguarda soltanto la
politica di alimentazione e deep sleep.

Prima di usare il dispositivo in auto riportare il comando su `AUTO CAN`.
