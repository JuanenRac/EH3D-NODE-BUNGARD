# Wiring the node to the machine's RS-232 port

The node takes the place of the PC: it is the **DTE** end of the cable, the one that sends on TXD and drives DTR, and the machine's control unit is the **DCE** end. Use the same cable that connected the PC to the control unit (a straight DB9 cable, not a null-modem cable).

**An ESP32 pin is 3.3 V logic. RS-232 is plus and minus 12 V. Never connect an ESP32 pin to the machine without an RS-232 level converter.** A MAX3232 (the 3.3 V version of the MAX232, 2 drivers and 2 receivers) does it with four small capacitors; there are ready-made modules with a DB9 socket. The control unit itself uses a MAX232 for these same four signals.

## What the control unit's RS-232 connector does

Read from the manual of the control unit's interface card:

| DB9 pin | Signal | What the control unit does with it |
|---|---|---|
| 5 | GND | ground |
| 3 | TXD of the PC | received (MAX232 R2IN): every byte for the motors, the latches and the spindle |
| 4 | DTR | not through the MAX232: a 4.7 k pull-up to 5 V and two 1N4148 diodes (anodes on the DTR node, cathodes on the ERR input and on the TOOL input of the interface chip). DTR driven negative pulls ERR and TOOL low; positive (or open) lets the pull-up raise them |
| 1 | DCD | **driven by the control unit** (MAX232 T1OUT): the state of the ERR line |
| 9 | RI | **driven by the control unit** (MAX232 T2OUT): the state of the TOOL line |
| 7 and 8 | RTS and CTS | tied together inside the control unit's connector, nowhere else |
| 6 | DSR | not connected |
| 2 | RXD of the PC | not mentioned: the control unit does not seem to send anything on the data line |

This explains what the PC sees on a machine with no tool sensor: DCD and RI follow DTR (the diodes make DTR drive the ERR and TOOL lines the control unit reports back), and the limit switches do not appear on any line: the interface card watches them itself (the HOME X, HOME Y and HOME Z lamps) and stops an axis at its switch, so a program homes an axis by sending more steps towards the switch than the travel is long, backing off a little and approaching again.

## What the node uses

| DB9 pin | RS-232 signal | Direction (seen from the node) | Needed | MAX3232 | ESP32 pin (default) |
|---|---|---|---|---|---|
| 3 | TXD | out (to the control unit) | yes | T1: GPIO into T1IN, DB9 from T1OUT | GPIO 17 (UART TX) |
| 4 | DTR | out | yes | T2: GPIO into T2IN, DB9 from T2OUT | GPIO 15 |
| 1 | DCD | in | yes | R1: DB9 into R1IN, GPIO from R1OUT | GPIO 16 |
| 9 | RI | in | yes | R2: DB9 into R2IN, GPIO from R2OUT | GPIO 21 |
| 5 | GND | common | yes | GND | GND |
| 2 | RXD | in | no (nothing arrives) | spare | GPIO 18 (UART RX), not wired |
| 7, 8 | RTS, CTS | | no | tied together by the control unit | not wired |
| 6 | DSR | | no | | not wired |

- **TXD** carries every byte for the control unit (one byte per motor step, 38400 baud, 8 data bits, no parity, 1 stop bit).
- **DTR** and **DCD** are the connection handshake of the original program: it lowers DTR and expects DCD low, raises DTR, sends the bytes 0x00 and 0x10 and expects DCD high. DCD and RI must both be wired: the control unit reports its error and tool states on them.
- With a MAX3232 both pairs of control lines are inverted: a logic high on a GPIO becomes a negative RS-232 voltage (off). The firmware knows it (the setting `uart.rs232_inverted`, on by default): it drives the DTR pin low to assert DTR and reads the DCD and RI pins inverted. If you use a converter that does not invert, switch that option off.
- A MAX3232 runs from 3.0 to 5.5 V: power it from the board's 3.3 V.
- The four signals in use are exactly the two drivers and two receivers of one MAX3232.

## The two boards

Both boards use the same pins by default, so the same wiring serves either; the pins are settings of the node's panel (page Machine, card Control unit), and the names to look for on the board's header are GPIO numbers, not pin positions.

| | s3-eth (Waveshare ESP32-S3-ETH) | s3-wifi (ESP32-S3-WROOM-1 N16R8, no Ethernet) |
|---|---|---|
| Way in | W5500 Ethernet over SPI (GPIO 9 to 14, already wired on the board), PoE | Wi-Fi (the network and its password are set from the panel, through the set-up network) |
| Power | PoE from the cable, or USB-C | USB-C or the 5 V pin |
| RS-232 pins | GPIO 17 TX, 15 DTR, 16 DCD, 21 RI | the same |
| Do not use | GPIO 9 to 14 (the W5500), 19 and 20 (USB), 0 and 3 and 45 and 46 (boot configuration) | GPIO 19 and 20 (USB), 0 and 3 and 45 and 46, 26 to 37 (flash and PSRAM) |

Check the pins against the pin table of the board you hold before powering it; change them in the panel if the board brings any of them out differently.

## The lamps (HOME X, HOME Y, HOME Z, ENABLE, TOOL)

The control unit lights HOME X, HOME Y and HOME Z when the axis is on its limit switch, ENABLE when the machine is active and TOOL (never seen lit so far). Each lamp is driven by a PNP transistor to 5 V from the interface chip, and the lamp's anode sits at about 2 V when lit (an LED drop) with a 1 k resistor from its cathode to ground. The node reads them, so it knows **which axis is home, live**, without reading RI through the USB adapter (which cuts the step stream).

- **Read the anode through an NPN transistor** (2N3904, BC547) or a 2N7002: base through **10 k** from the lamp's anode, emitter to ground, collector to the ESP32 pin, which has its internal pull-up. A lit lamp pulls the pin low (the setting `lamps.active_low`, on by default). The buffer takes a few tens of microamps and leaves the lamp as it was.
- **Do not put an opto-coupler's LED straight across the lamp**: its 1.2 V forward drop would hold the lamp at 1.2 V and put it out. If you want an opto-coupler, give its LED a series resistor of about 2.7 k from the anode to ground (it takes about 0.3 mA) and use one with a high current transfer ratio at low current. The ground is shared with the node anyway (RS-232 pin 5), so the isolation gains little.
- Pins (panel, card Lamps): HOME X GPIO 1, HOME Y GPIO 2, HOME Z GPIO 7, ENABLE GPIO 8, TOOL GPIO 38. Check them against your board's header; GPIO 4, 5 and 6 are the spindle.
- The node takes the lamps from the board that carries them; power the node from the box's 5 V (the ESP32-S3 boards take 5 V on their power pin) and join the grounds.
- With `StopOnHome` (frame 0x09) the program asks the node to drop the rest of a search the moment the lamp of an axis lights; the control unit has already stopped the axis, so this only saves the wait. The lamps are also in every Status frame and on the web panel.

## The spindle (speed 0-10 V and direction)

The node can drive the spindle of the machine as well: a speed signal of 0-10 V and the run or direction lines of the spindle's control unit (24000 rpm at 10 V by default). The unit in this machine is a **Huanyang HY02D223B** variable-frequency drive (2.2 kW, single-phase 220 V in, 400 Hz out) driving a **GDZ 80-2.2** spindle (24000 rpm = 400 Hz, 2.2 kW, 220 V, 8.8 A). The wiring below is the general scheme; the values are settings of the panel (page Machine, card Spindle).

| Output | ESP32 pin (default) | What it does |
|---|---|---|
| Speed PWM | GPIO 4 | PWM at 2000 Hz, 0 % = 0 rpm, 100 % = the maximum speed. Feed it to a **PWM to 0-10 V converter module** (its own supply, usually 12-30 V); the module's output goes to the speed input of the spindle's control unit |
| Run forward | GPIO 5 | on while the spindle turns forward |
| Run reverse | GPIO 6 | on while the spindle turns in reverse (never together with forward) |

- With a control unit that wants **one run line and one direction line** instead, turn on the setting `spindle.dir_single`: GPIO 5 becomes the run line and GPIO 6 the direction line (on for reverse). `spindle.outputs_active_low` and `spindle.pwm_inverted` flip the logic of the lines and of the PWM.
- **For the HY02D223B:** speed goes to the drive's analog input **VI** and **ACM** (the 0-10 V from the converter module; the link beside the screw terminals must be on VI, not on the front potentiometer). Run forward and run reverse are the drive's **FOR** and **REV** inputs, each closed to the common terminal **DCM**: use two relay contacts or opto-couplers, one per input, driven by GPIO 5 and GPIO 6 (the default two-line mode). The drive has to be set for external control. The usual values reported by other owners of this drive, to be checked against the drive's own manual and the spindle's nameplate before the first start: run command from the terminals (`PD001` = 1), frequency from the analog input (`PD002` = 1), analog range 0-10 V (`PD070` = 0), and the base, maximum and main frequency (`PD003`, `PD004`, `PD005`, `PD072`) at 400 Hz for this spindle, with the motor current (`PD142`) from its nameplate (8.8 A). A wrong frequency setting can damage a 400 Hz spindle. Reverse on a spindle with a collet can loosen the tool: use it only if the tool holder allows it.
- The outputs are 3.3 V logic and can give a few milliamps. The inputs of a spindle's control unit are usually 24 V opto-isolated: put a transistor, an opto-coupler or a relay between the pin and the unit, never the pin directly.
- Common ground between the node, the converter module and the control unit's speed input.
- All three outputs are off at power-up, on any stop of the machine and whenever the speed is 0, so a node that restarts or loses its link never leaves the spindle turning. A control unit with its own enable input should still be wired so that it cannot run without the machine's emergency stop circuit.
- The maximum speed, the PWM pin and frequency, the settling time between directions and the pins are all settings of the panel (card Spindle). Set `spindle.max_rpm` to 0 for a node with no spindle.

## Before the first move

1. Wire the MAX3232 and check the voltages with the control unit off and on: TXD idle must be negative at the DB9, and DTR positive only after the node asserts it.
2. Flash the node and set its access token in the panel (without one it refuses to be controlled).
3. Connect from the computer with `node_probe` (the Rust library, feature `node`): it logs in and shows the status lines. Nothing moves until the machine is armed.
4. Try one short move on one axis with the tool removed and a hand near the machine's power switch.
