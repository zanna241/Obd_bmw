# V0.9.9 BMW EXTENDED DIAGNOSTIC DISCOVERY

Base: V0.9.7 consolidated.

## New read-only BMW diagnostic scan

BMW F-series extended 11-bit ISO-TP addressing is added as a separate diagnostic
path. The tester uses source address F1, CAN ID 6F1. Byte 0 of the CAN payload is
the destination ECU address.

Presence targets:
- 0x12 DDE/DME -> expected responder CAN ID 0x612
- 0x18 EGS/ZF8 -> expected responder CAN ID 0x618
- 0x5E GWS     -> expected responder CAN ID 0x65E
- 0x60 KOMBI   -> expected responder CAN ID 0x660

For this first vehicle test the firmware sends ONLY:
    UDS TesterPresent 3E 00

Single-frame wire example for EGS:
    CAN 6F1: 18 02 3E 00 00 00 00 00

A positive EGS response is expected in the form:
    CAN 618: F1 02 7E 00 ...

A negative response to service 3E also counts as proof that the target ECU is
reachable, but is logged raw for later analysis.

No reset, coding, programming session, actuator routine, DTC clear or write
service is sent by this scan.

## Web
Discovery now has two independent buttons:
- SCAN OBD READ-ONLY
- BMW EXT 6F1 READ-ONLY

The BMW scan reports a four-bit responder mask:
bit0 DDE 0x12
bit1 EGS 0x18
bit2 GWS 0x5E
bit3 KOMBI 0x60

All TX/RX frames continue to be stored in RAW/discovery logs and BMW responder
events are added to the event log.

## Why this step
The existing 7E0/7E1/7E4 paths are standard OBD-style diagnostics. BMW F-series
also routes proprietary diagnostics through the central gateway using extended
11-bit ISO-TP addressing. This scan tests that route on the actual vehicle
before adding any proprietary ReadDataByIdentifier/job requests.

The next stage, only after responder confirmation, is a full BMW extended
ISO-TP transport layer with multi-frame RX/TX and carefully selected read-only
identification/data requests for DDE and EGS.
