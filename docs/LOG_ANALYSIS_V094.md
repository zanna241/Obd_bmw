# V0.9.4 - Analisi log e motivazioni delle modifiche

## Risultato acquisizioni V0.9.3

I log controllati mostrano che la modalita TWAI `ACCEPT_ALL` funziona, ma sulla presa OBD la vettura espone passivamente quasi esclusivamente `0x130`; il resto del traffico presente e quello diagnostico generato dal monitor (`0x7E0/0x7E8` e `0x7E4/0x7EC`). I marker P/N/R/D e SHIFT non producono nuovi ID passivi e, nei log V0.9.3 controllati, `0x130` rimane `05 F1 FC FF FF`. Per questo non viene attribuito alcun significato ZF8 a `0x130`.

Nel log con ECU scan, lo scan inizia a circa 64093 ms e termina a circa 66946 ms (circa 2.85 s). `0x7E0` riceve risposta positiva da `0x7E8`, `0x7E1` da `0x7E9`; gli altri indirizzi non rispondono a TesterPresent. Subito dopo la fine dello scan riprende il normale polling e `0x7EC` continua a rispondere alle richieste standard indirizzate a `0x7E4`. Il logger continua a registrare per molto tempo dopo lo scan: non si osserva un crash CAN o un reset della board.

## Perche la pagina Web poteva sembrare bloccata

Lo scan CAN e gia cooperativo e aggiunge pochissimo traffico. Il collo di bottiglia piu probabile era invece la somma di operazioni sincrone nello stesso `loop()`:

- WebServer Arduino sincrono, servito dopo CAN/LVGL;
- flush LVGL full-frame;
- scritture/flush sincroni su microSD;
- piu richieste HTTP periodiche della Web UI che potevano sovrapporsi;
- vecchio `alert()` del browser durante lo scan.

La V0.9.4 riduce questi rischi servendo il WebServer due volte per loop, spostandolo anche all'inizio del loop, aumentando il buffer RAW SD, riducendo la frequenza dei flush, eliminando il flush evento dai callback e impedendo fetch Web sovrapposti. Durante i ~3 s dello scan vengono inoltre sospesi i download pesanti di catalogo/discovery, mentre `/api/status` resta disponibile.

## Parametri reali osservati

Nelle acquisizioni di guida sono stati osservati valori plausibili per RPM, velocita, coolant, IAT, MAF, load, acceleratore, tensione, boost, rail, tre EGT, NOx e pressione differenziale DPF. Il trigger rigenerazione e rimasto a zero nella sessione esaminata e lambda non ha fornito un valore utile.

## ZF8 e BMW enhanced diagnostics

La strategia successiva deve privilegiare interrogazioni diagnostiche BMW/EGS read-only: il gateway OBD non sta inoltrando il normale broadcast PT-CAN necessario a ricostruire passivamente marcia, turbine/output RPM, slip e lock-up. Nessun mapping non verificato e stato inserito in V0.9.4.
