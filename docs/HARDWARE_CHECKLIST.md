# Hardware checklist - EH3D-NODE-BUNGARD

Run the automatic part first, then the steps by hand. Write the result next to each step (PASS / FAIL and what you saw) and keep the report file with the notes.

## Automatic part (the read-only run needs no machine)

```
python tools/selftest.py --host NODE --token TOKEN --user admin --password PASS
```

The token and the password can also come from `EH3D_TOKEN` and `EH3D_PASSWORD`; they are never printed. The report `selftest_bungard_<time>.txt` lands in the current folder. Without `--active` nothing is written to the node and nothing moves.

With the node wired to the machine, the spindle free to turn and the emergency stop in reach:

```
python tools/selftest.py --host NODE --token TOKEN --user admin --password PASS --active
```

It asks you to type `SI` before it arms the link, turns the spindle (1000 rpm for 2 s by default) and times three stops. The machine does not move on any axis. Every active test ends with the spindle stopped and the link disarmed, and the end of the run checks it again.

## By hand

| Step | What to do | What must happen | Result |
|---|---|---|---|
| 1 | Open the panel, change a setting | The Save bar is visible on every page and lights up only when something changed | |
| 2 | System page: export the settings, then import the same file | The forms show the same values; nothing is saved until you press Save | |
| 3 | Network page: add two backup Wi-Fi networks, Save, reload | Both are still listed; switch the main network off and the node joins a backup | |
| 4 | Generate a token, press the copy button | The token is on the clipboard, also over plain HTTP | |
| 5 | Start the spindle from the program, press the stop button of the panel | The spindle stops at once and the panel shows it stopped | |
| 6 | Start the spindle, press the hardware emergency stop | Spindle and drivers off; the node refuses to move until the stop is released | |
| 7 | Queue a move with a spindle start and a spindle stop inside the queue, then press Abort in the middle | The spindle stops, the queue is empty, the drivers are off | |
| 8 | Pull the network cable while the spindle turns | The spindle stops within the idle time (3 s by default) | |
| 9 | From the program send a homing command (`$H` in the GRBL-style G-code) | The axes search their switches and stop; the lamps show HOME | |
| 10 | Restart the node | It comes back disarmed with the spindle off | |

A FAIL on steps 5 to 8 means: do not run a job on this node.
