# V0.9.9.2 STABILITY

## Evidenza di partenza

I log V0.9.9.1 del 26 agosto 2026 mostrano due pause del ciclo di acquisizione:
circa 2,5 secondi e circa 10,5 secondi. Il CAN riprende senza reinizializzazione;
la causa piu probabile e quindi I/O sincrono SD/web, non la perdita del bus.

## Correzioni

- Il download HTTP viene rifiutato con HTTP 409 se il logger e attivo.
- I file vengono inviati in blocchi da 2048 byte, richiamando `can_update()` e
  `yield()` tra le scritture al client.
- Il flush dei tre CSV passa da 3 a 15 secondi.
- Write e flush sono cronometrati; la pagina LOGGER mostra contatore stalli,
  write massimo e flush massimo.
- All'avvio OTA il logger viene fermato e tutti i file vengono chiusi.
- Rimangono attivi ECU Scan read-only e BMW Extended Discovery per EGS/ZF8,
  GWS e KOMBI. Nessun PID proprietario viene dichiarato valido senza riscontro.

## Prova in auto

1. Avviare il logger e guidare almeno 15 minuti.
2. Non scaricare file durante la registrazione; il firmware restituisce 409.
3. Fermare il logger e scaricare i cinque CSV da iPhone.
4. Annotare i valori `Stalli SD`, `Write massimo` e `Flush massimo`.
5. Eseguire la discovery durante cambi marcia, arresto, accelerazione moderata e
   rilascio, senza distrarsi dalla guida.

## Criterio di accettazione

- UI web e touchscreen sempre responsive.
- Nessuna pausa CAN superiore a 500 ms in condizioni normali.
- Nessun CSV corrotto o troncato dopo stop logger.
- Download completo e riapribile da iPhone.
