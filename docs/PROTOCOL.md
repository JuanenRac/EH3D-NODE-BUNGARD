# The control link

A program on the network drives the machine through one TCP connection to the node (port 5800 by default). The node holds no job and makes no decisions about motion: it checks who is asking, keeps a queue of bytes and sends them out of its UART at the line's own pace.

## Why the program sends bytes already spaced

The control unit of a Bungard CCD machine takes **one byte per motor step** at 38400 baud 8N1; the time between steps is made by the computer that drives it. A USB serial adapter delivers bytes in bursts once a millisecond, which makes the motors sound rough above about a thousand steps a second. Here the program places each step byte in a continuous stream, so that the position of the byte carries the time: a step byte is followed by filler bytes `0x00` (no axis bits, no motion) that make up the pause, and each byte takes 260.4 microseconds on the line. The node writes the stream to the UART in order and the UART spaces the bytes exactly. The Rust protocol library has `paced_stream()` for this.

## Frames

```
0xB5 | type | length (u16 LE) | payload | CRC-16/CCITT-FALSE (u16 LE)
```

The CRC covers the type, the length and the payload. The largest payload is 2048 bytes. A damaged frame is dropped and the decoder looks again from the byte after its false start.

| Type | Name | Direction | Payload |
|---|---|---|---|
| 0x01 | Hello | to node | the access token (text). Must be the first frame |
| 0x02 | Arm | to node | none. The operator allows the machine to be moved |
| 0x03 | Disarm | to node | none. Empties the queue |
| 0x04 | Dtr | to node | 1 byte: 0 = off, otherwise on |
| 0x05 | Write | to node | bytes for the control unit (only while armed) |
| 0x06 | Abort | to node | none. Empties the queue, sends `0xC0` (drivers off), disarms |
| 0x07 | Ping | to node | none. Keeps an armed session alive |
| 0x09 | StopOnHome | to node | 1 byte: axes to watch (bit 0 X, 1 Y, 2 Z; 0 cancels). When one of their HOME lamps lights, the node empties its queue once and answers HomeSeen |
| 0x08 | Spindle | to node | speed in rpm (u16), optionally 1 byte direction (0 forward, 1 reverse; left out: the last one) and 1 byte of flags (bit 0: **at the end of the queue** - the command waits among the queued bytes and is applied when those before it have gone out). 0 rpm stops it |
| 0x81 | Info | to program | major, minor, patch (3 bytes), baud (u32), queue capacity (u32) |
| 0x82 | Status | to program | queue fill (u32), bytes sent (u32), armed (1), lines (1: CTS 1, DSR 2, RI 4, DCD 8), spindle rpm (u16), spindle direction (1), lamps (1: HOME X 1, HOME Y 2, HOME Z 4, ENABLE 8, TOOL 16), flags (1: bit 0 the node is finishing the job it holds) |
| 0x83 | Rx | to program | bytes the control unit sent |
| 0x85 | HomeSeen | to program | 1 byte: the lamps when a StopOnHome emptied the queue |
| 0x84 | Error | to program | 1 byte: 1 not logged in, 2 bad token, 3 not armed, 4 queue full, 5 bad frame, 6 open control refused, 7 idle timeout, 8 no spindle output, 9 spindle must stop before reversing, 10 spindle stopped too recently to reverse, 11 the node is finishing the job it holds |

Status is sent every 200 ms to a logged-in program.

## Rules that keep the machine from moving by accident

- Nothing but Hello is accepted before a Hello with the right token. A node with no token refuses control unless the setting `machine.link.allow_open` is on (a bench setting, off by default). The token, the port, the idle time and the queue are settings of the panel, not compiled into the image.
- Bytes for the control unit are accepted only while armed, and only if the whole frame fits in the queue (`QueueFull` otherwise: the program waits for the Status to show room).
- An armed session that sends nothing for `machine.link.idle_ms` (three seconds by default) is stopped, and so is one whose connection drops: the queue is emptied and `0xC0` (stepper drivers off) is sent. A lost cable cannot leave the machine running the rest of a move. Send Ping while a long move is queued and nothing else is being sent.
- At most a few dozen bytes that the UART task had already taken still go out after a stop.
- The spindle starts only while armed, never goes above the speed the node was built for (24000 rpm by default; a higher request is cut to it), and stops with the machine: Abort, Disarm, an idle timeout and a dropped link all set it to 0 rpm. It cannot change direction while it turns, nor until the spindle has been stopped for the settling time (3 s by default): stop it, wait, then ask for the other direction.
- **Finishing the job it holds** (the setting `machine.link.finish_on_loss`, off by default): when an armed program goes quiet or drops, instead of stopping at once the node runs the bytes already in its queue to the end, with the spindle commands queued among them, and then switches the spindle and the drivers off. It takes nothing new while it does (every frame but Ping, Abort and Disarm answers `Draining`), gives up after `machine.link.max_drain_s` seconds whatever is left, and an Abort from a program that comes back, or the panel's stop button, ends it at once. The program cuts the job so that the end of what it queued is a safe place (Z up, between two holes) and puts the spindle's stop in the queue. The direction rules of the spindle apply to the queued commands too: no change of direction while the queued spindle turns, and the bytes between a queued stop and a start the other way (the pauses of the stream) must last as long as the settling time.
- One program at a time.

The web panel (ports 80 and 443) shows the status and edits the settings but never moves the machine: moving it goes only through this link. While a program holds the machine armed the panel refuses to save settings (HTTP 409 `machine_armed`) and postpones a restart, because a write to flash could stall the stream.

## What the node cannot see

The control unit does not report its limit switches on any serial line: it lights a lamp and stops the axis itself, and RI and DCD only follow DTR. A program homes an axis by sending more steps towards the switch than the travel is long, backing off a little and approaching again; the control unit swallows the surplus steps.
