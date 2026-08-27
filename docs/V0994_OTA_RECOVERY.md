# V0.9.9.4 OTA RECOVERY

## Correzione upload iPhone

L'aggiornamento non usa piu `XMLHttpRequest` e `FormData` dalla dashboard. Il
pulsante FIRMWARE apre `/ota`, una pagina autonoma che invia il BIN tramite form
HTML multipart nativo. Uscendo dalla dashboard si interrompono anche i polling
periodici che potevano concorrere con il POST sul WebServer sincrono ESP32.

## Autenticazione per dispositivo

La password fissa e stata rimossa. Al primo avvio della V0.9.9.4 la protezione
web e disattivata. In SISTEMA / ACCESSO WEB si puo:

- abilitare la protezione impostando una password da 8 a 63 caratteri;
- cambiare la password;
- disattivare esplicitamente la protezione.

Quando attiva, il nome utente e `admin`. Password e stato sono salvati nelle
Preferences ESP32 e persistono dopo riavvio. Disattivando la protezione, la
password salvata viene cancellata.

## Procedura OTA

1. Collegarsi all'AP del monitor e aprire la dashboard.
2. Entrare in FIRMWARE e premere APRI AGGIORNAMENTO OTA.
3. Selezionare il BIN e premere AGGIORNA FIRMWARE.
4. Non cambiare rete e non chiudere il browser.
5. Attendere il messaggio di successo e il riavvio automatico.

Il logger viene fermato e chiuso prima di iniziare la scrittura flash. Il
riavvio viene programmato soltanto dopo `Update.end(true)` riuscito.
