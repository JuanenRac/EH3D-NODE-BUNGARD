# EH3D-NODE-BUNGARD: scope

Status: **host-test**. EH3D-NODE-BUNGARD is the firmware, in C++17 for the ESP32-S3, that turns the RS-232 port of the control unit of a Bungard CCD machine into a network port. Its rules are tested on a computer with host programs; the firmware builds for both boards, and it has never been flashed or run on a board. This page says what it is for, what it will not do and where it sits among the other programs of the suite.

## Purpose

The node between the network and the control unit of the drill: a program connects over TCP with an access token, arms the machine and sends the bytes of a move; the node queues them and puts them on the line at the line's own pace, reads the lamps of the machine, drives the spindle through a 0-10 V signal and run and direction lines, and offers a web panel with a menu (in 37 languages) for set-up, users, network, update and the machine's own settings.

## What it does

* Frames the control link with a checksum and a decoder that resynchronises after noise or damage, and refuses a length above the maximum at the header.
* Logs a program in with a token, arms the machine, queues the bytes of a move whole or not at all and sends them at the pace of the line.
* Stops the machine on every way out: an abort, a disarm, a silent link, the home stop and the stop of the panel all tell the hardware to switch the spindle off, whatever the session believes.
* Applies the rules of the spindle (a speed cap, no reversing, a delay) and keeps the settings that keep the machine safe closed by default.
* Keeps users, sessions, a login throttle and a network plan, and a node guard that restarts a hung task and keeps a ledger of the restarts.
* Updates itself from the release files of this repository only, over HTTPS, when an administrator asks, after the SHA-256 of the release agrees.
* Checks itself (a self-check the person can read) and publishes a contract of the link that the client of the server is tested against.

## What it will not do

* No emergency-stop input yet and no view of the limit switches except through the lamps (see below).
* No signed update: the update checks the SHA-256 that the release publishes, and the signing key is still missing.
* No drive of a machine other than the Bungard CCD control unit.
* No change of the settings that keep the machine safe without an administrator, and none that opens them by default.

## Where it sits

* **It reads:** the bytes of the control link from the network, the RS-232 line of the control unit, the lamps (HOME X, Y and Z, ENABLE, TOOL), its settings in flash and, when asked, the release files of its own repository.
* **It writes:** the bytes of a move to the RS-232 line, the 0-10 V signal and the run and direction lines of the spindle, its settings and users in flash, and the web panel.
* **It is read by:** EH3D-SERVER, the programs of the Bungard workflow (EH3D-DRILLPRO, EH3D-ROUTEPRO and EH3D-BUNGARD-GUI) and the person at the web panel.
* **It depends on:** ESP-IDF 5.5 for the firmware, a C++17 compiler and CMake for the host tests, and the shared base of the nodes of the suite.

## What it never decides

The limits of a machine, its interlocks and its emergency stop belong to the machine and to its control unit, not to this node. A move that was queued and sent is not a verified move: the node does not know where the machine stops, so a person must keep a hand on the power switch of the machine.

The requirements of the program, with the state of each and the test that shows it, are in `REQUIREMENTS.md`; the state in a paragraph is in `STATUS.md`.
