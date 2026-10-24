# The carrier board of the node

One small board of **100 x 100 mm** that takes the place of the PC's serial port, reads the lamps of the control unit and drives the spindle's variable-frequency drive (VFD). The ESP32-S3 board plugs into it on pin headers (a Waveshare ESP32-S3-ETH for the cable version, an ESP32-S3-WROOM-1 module board for the Wi-Fi one), so the same carrier serves both. This page is the design to draw the schematic from; **nothing here has been built or tested yet**, and the pin numbers must be checked against the header of the board you hold.

## Blocks

```
 box 5 V ──► [fuse + TVS] ──┬──► ESP32-S3 board (5 V pin; or PoE on the ETH board, never both)
                            ├──► 3.3 V (from the ESP32 board) ──► MAX3232
                            └──► isolated DC-DC 5 V → 12 V ──► speed stage (op-amp)  ┐ isolated side
                                                                                      │  (the VFD's ground)
 ESP32 ──UART/DTR──► MAX3232 ◄──► DB9 male (to the control unit, like a PC port)      │
 lamps (5) ──► NPN buffers ──► ESP32 inputs                                           │
 PWM ──► opto-coupler ──► RC filter ──► op-amp ×2 ──► 0-10 V ──► VFD  VI / ACM  ──────┤
 FWD ──► opto-coupler ──► VFD  FOR / DCM                                              │
 REV ──► opto-coupler ──► VFD  REV / DCM  ────────────────────────────────────────────┘
```

## Power

- The node has its **own small 230 V AC to 5 V supply** (a sealed module such as a Hi-Link HLK-PM01 or a Mean Well IRM-10-5, 5 V 2 A) inside the box, so it does not load the control unit's supply. It is fed from the **same mains switch** as the machine: one switch turns on both.
- **Mains wiring is the dangerous part**: take the module's input from the machine's switched side after its fuse (or give it its own 1 A slow-blow fuse), keep the mains wires short, fixed and away from the low-voltage side with strain relief, cover the module's terminals, and keep 6 mm of clearance to everything else. Earth is not needed for a module of class II, but follow the box's own wiring. Have the connection checked by someone qualified if in doubt.
- On the board: a 500 mA resettable fuse and a TVS diode (SMAJ5.0A) after the 5 V, then the 5 V pin of the ESP32 board and the isolated DC-DC for the speed stage. An ESP32-S3-ETH board takes about 150-250 mA (more while transmitting on Wi-Fi), the DC-DC about 100 mA more.
- **The ETH board without the PoE module**: the Waveshare ESP32-S3-ETH is sold with and without the PoE part; the one without it is the one for this box, fed from the 5 V pin, and the Ethernet cable carries data only. If a board with the PoE module is used, do not feed it from PoE and from the 5 V at the same time.
- The grounds: the node's ground and the control unit's are joined through pin 5 of the RS-232 cable and the lamp wires. Leave a footprint for a 100 nF + 1 MΩ link to the board's ground at the DB9 shell in case of noise.

## RS-232 (DB9 male, wired like a PC: DTE)

| Part | Function |
|---|---|
| MAX3232 (SOIC-16), four 0.1 µF capacitors, supply 3.3 V | 2 drivers (TXD, DTR) and 2 receivers (DCD, RI) |
| DB9 male, straight cable to the control unit | pin 3 TXD, pin 4 DTR, pin 1 DCD, pin 9 RI, pin 5 GND |
| ESD protection on pins 1, 3, 4, 9 (a TVS array such as SM712 or SP3003) | the cable is plugged and unplugged |

ESP32 pins: GPIO 17 TX → T1IN, GPIO 15 DTR → T2IN, R1OUT → GPIO 16 (DCD), R2OUT → GPIO 21 (RI). GPIO 18 (UART RX) is left free: the control unit sends nothing on the data line, so no receiver is wired to DB9 pin 2. Leave a 2-pin jumper to join pins 7 and 8 of the DB9 if a future unit needs it.

## Lamps (HOME X, HOME Y, HOME Z, ENABLE, TOOL)

Five identical inputs, taken from the **anode** of each lamp on the control unit's board (a wire to a test pad or the lamp's lead):

- 10 kΩ from the anode to the base of an NPN (BC847 or MMBT3904), emitter to ground, collector to the ESP32 pin (internal pull-up). Lit lamp → pin low. Add a 100 kΩ from the base to ground so a loose wire reads off.
- Pins: HOME X GPIO 1, HOME Y GPIO 2, HOME Z GPIO 7, ENABLE GPIO 8, TOOL GPIO 38. A 5-pin header with ground for the five wires.

## Spindle: speed 0-10 V (the VFD's VI and ACM)

The VFD's control ground may sit at a dangerous potential relative to the node: the speed signal goes through an isolator.

1. GPIO 4 (PWM, 2 kHz by default, 0 % = 0 rpm, 100 % = 24000 rpm) → a fast opto-coupler (6N137 or HCPL-2631) with 330 Ω on the LED side.
2. On the isolated side, supplied by an **isolated DC-DC 5 V → 12 V** (for example B0512S-1W or TMA0512S) and a 5 V regulator from it: the opto-coupler's output swings between 0 and 5 V (with a pull-up to that 5 V); a two-stage RC low-pass filter (a few hundred Hz, for example 10 kΩ + 1 µF twice) turns the PWM into a DC level of 0-5 V.
3. A rail-to-rail op-amp (MCP6002, or an LM358 on 12 V) in a non-inverting stage with a gain of **2** (5 V → 10 V), with a 10 kΩ + 10 kΩ divider in the feedback and a trimmer for the full-scale 10.00 V. Output through 1 kΩ to the VFD's **VI**, with **ACM** as the return. A 10 V zener (or TVS) clamp on the output protects the input of the VFD.
4. A test pad on the output for the multimeter: with the spindle command at 12000 rpm it must read 5 V.

If the 0-10 V stage is too much work for a first board, the alternative is a commercial isolated PWM to 0-10 V module on the connector: the PWM and ground from the node and the module's 0-10 V to VI/ACM.

## Spindle: run forward and reverse (the VFD's FOR, REV and DCM)

- GPIO 5 → 330 Ω → an opto-coupler LED (PC817, TLP785); the phototransistor between **FOR** and **DCM** (collector to FOR). GPIO 6 the same for **REV**. The VFD pulls those inputs up internally, so a transistor output pulling them to DCM is enough.
- The optos keep the VFD's control ground apart from the node's.
- Check on the VFD: the open voltage between FOR and DCM and the polarity, and the parameters of the VFD (`PD001`, `PD002`, `PD070` and the 400 Hz values, see [WIRING.md](WIRING.md)).
- Also useful: the VFD's own contact output for fault, back to a GPIO through another opto (not read by the firmware yet).

## Connectors

| Connector | Pins |
|---|---|
| J1 box 5 V | 5 V, GND |
| J2 DB9 male | RS-232 to the control unit |
| J3 lamps | 5 lamp wires + GND |
| J4 VFD | VI, ACM, FOR, REV, DCM (screw terminal, 5 positions) |
| J5 | Ethernet is on the ESP32-S3-ETH board itself; Wi-Fi uses its antenna; keep this edge of the board free of copper and of the VFD wires |
| JP1 | pins 7-8 of the DB9 joined |
| J6 | 2×3 programming and UART header (3.3 V, TX, RX, EN, GPIO0, GND) |

## Layout notes

- Keep the VFD side and its isolation barrier (a slot of 2 mm in the board) well away from the logic; the VFD wiring is next to a motor drive with high-frequency noise: shielded cable, the shield to ground only at the node end.
- The MAX3232 and its capacitors close to the DB9.
- Fiducials, mounting holes for the box, and a silkscreen of every connector pinout.

## Open points

- The voltage and current that the box's internal 5 V can supply.
- The exact header of the ESP32 board (pins 1, 2, 4, 5, 6, 7, 8, 38 free?) and whether to use the ETH or the Wi-Fi board for the first board.
- The VFD's real control terminals (the manual: names, polarity of FOR and REV, the open voltage).
- Whether the optional drive-fault input is wanted.
- The drawing tool (EAGLE, as the others of the suite, or another) and whether to order assembled or to solder it.
- The first runs: a Wi-Fi board on a bench with the carrier unpopulated, then the ESP32-S3-ETH in the box.
