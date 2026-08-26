# BMW 520xd Monitor V0.9.1 - FAST LIVE

## Analisi log reale 2026-08-24

Il giro su strada della V0.9.0 ha confermato:

- comunicazione fisica stabile con MCP2562FD;
- DDE su 0x7E8;
- polling diagnostico fisico su 0x7E0;
- ISO-TP multiframe funzionante;
- PID standard diesel avanzati decodificati correttamente.

Intervalli osservati nel CSV decoded V0.9.0:

- RPM: 750.5 .. 2227.5 rpm
- velocita: 0 .. 52 km/h
- coolant: 61 .. 85 C
- MAF: 12.24 .. 65.91 g/s
- boost gauge: 0.04 .. 0.946 bar
- rail: 236.2 .. 1197.1 bar
- EGT1: 129.4 .. 271.5 C
- EGT2: 91.5 .. 236.3 C
- EGT3: 55.8 .. 188.6 C
- DPF differential pressure: -4 .. 35 hPa

Il problema principale della V0.9.0 era il rate di polling reale: nonostante gli
intervalli configurati, ogni PID veloce veniva interrogato mediamente solo ogni
~5.5 secondi durante logging/display attivi. La V0.9.1 corregge questo collo di
bottiglia.

## Migliorie V0.9.1

### Polling CAN prioritario

- RX elaborato prima di ogni nuova richiesta;
- minimo 70 ms tra richieste diagnostiche;
- scheduler basato su scadenza + priorita;
- RPM/boost/rail/velocita/MAF/DPF ricevono piu banda;
- valori lenti come BARO e temperatura ambiente restano a bassa frequenza;
- coda RX aumentata a 192 frame;
- coda TX aumentata a 24 frame;
- CAN servito prima e dopo il blocco LVGL nel main loop.

La pagina diagnostica e il Web mostrano anche richieste/s e risposte/s.

### Display

- refresh logico massimo portato da 400 ms a 200 ms (~5 Hz);
- vengono aggiornati solo i widget della pagina visibile;
- un label LVGL viene modificato solo se il testo e realmente cambiato;
- il logo BMW viene convertito da PROGMEM una sola volta invece che a ogni frame.

Queste modifiche riducono significativamente i full-frame flush inutili.

### Nuovi PID standard

Aggiunti:

- 0x49: acceleratore;
- 0x83: sensori NOx;
- 0x8C: lambda / wide-range O2.

Sono gia presenti:

- 0x70 boost;
- 0x6D rail;
- 0x78 EGT;
- 0x7A DPF differential pressure;
- 0x8B diesel aftertreatment / regen.

I valori 0xFFFF NOx e lambda raw 0 vengono trattati come non disponibili.

### GUI

Pagina MOTORE:
- RPM
- coolant
- oil (BMW pending)
- intake
- accelerator
- boost
- rail
- MAF
- load
- ECU voltage

Pagina DPF:
- regen
- EGT 1/2/3
- differential pressure
- regen trigger
- soot (BMW pending)
- ash (BMW pending)
- NOx 1
- lambda

### Web

- refresh live 750 ms;
- CAN raw refresh 1500 ms;
- nuovi campi accelerator / NOx / lambda;
- request rate / reply rate;
- fast-data age e DPF-data age disponibili nell'API status;
- display e Web continuano a leggere lo stesso VehicleData.

### Logger

Il CSV decoded passa da 1 Hz a 2 Hz e include:
- accelerator
- NOx 1/2
- lambda 1/2

Il RAW continua a registrare tutti i TX/RX.

## Parametri ancora BMW-specifici

Non vengono inventati e restano `--` fino alla validazione D70BX7A0 / EGS:

- temperatura olio motore;
- soot mass;
- ash mass;
- distanza ultima rigenerazione;
- vita residua DPF;
- temperatura ATF;
- marcia reale;
- input/output RPM ZF8;
- converter slip;
- lock-up;
- coppia cambio.

