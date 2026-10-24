<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center">🇺🇸 <b>English</b> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### An ESP32-S3 node that sits in the box of a Bungard CCD machine, plugs into the RS-232 port of its control unit and lets the EH3D programs drive the machine over Ethernet or Wi-Fi, with the machine's lamps and the spindle's drive on the same board

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Honesty check - what actually runs today:** firmware in C++17 for the ESP32-S3 (ESP-IDF 5.5) that turns the RS-232 port of the control unit of a Bungard CCD machine into a network port: a program connects over TCP with an access token, arms the machine and sends the bytes of a move, which the node queues and puts on the line at the line's own pace. It also reads the machine's lamps (HOME X, Y and Z, ENABLE, TOOL), drives a spindle through a 0-10 V speed signal and run/direction lines, and has a web panel (eight languages) for set-up, users, network, update and the machine's own settings. The rules of the link and the settings are tested on a computer, and the firmware builds without warnings for both boards. **It has not been flashed or run on a board yet**, and the carrier board that brings the RS-232 converter, the lamp inputs and the spindle's drive together is a design on paper.

---

> **Safety and security status:** this is not a safety system. A node that is wired wrongly, or a spindle drive that is wrongly set, can move a machine or spin a tool. The node refuses to be controlled without an access token, takes bytes for the machine only while a program has armed it, and stops (queue emptied, stepper drivers and spindle off) when the session goes quiet or the connection drops (unless the optional setting that lets it finish the job it holds is on). It does not see the machine's limit switches except through the lamps, and it has no emergency stop of its own yet (the carrier board has a place for a push-button input). The spindle's drive is on mains voltage: wire and set it with the maker's manual. The panel never moves the machine, and it refuses to save settings while a program holds the machine armed. The project is independent: it is not made, endorsed or supported by Bungard Elektronik, and product names appear only to say which machines and drives it is meant to work with.

## 📋 At a glance

| | |
|---|---|
| Version | `0.0.2` |
| Maturity | Scaffolding |
| Tests | 9 host test suites (about 2,200 checks) |
| Stack | C++17, ESP-IDF 5.5, ESP32-S3 |
| On real hardware | No: both firmware images build, but nothing has run on a board yet |

## 🚦 Project status

Each line is one part of this repository and the light says how far it is today. It is written from what the repository can show (tests, builds, runs), not from what is planned.

🟢 Done and verified · 🟡 Works, with known limits · 🟠 Partly done or not verified yet · 🔴 Not done or not working

| Area | State | What it means here |
|---|---|---|
| Software | 🟡 | The firmware for the ESP32-S3 (ESP-IDF 5.5, C++17) builds without warnings for both boards, Ethernet and Wi-Fi; no image has been flashed. |
| Tests | 🟢 | 9 host test programs pass (`ctest`), about 2,200 checks: the link, the settings, the machine rules, the shared base, the panel texts and the task safety. |
| Control link | 🟡 | The framed protocol (CRC, token login, arming, byte queue, stop when the session goes quiet) is covered by the host tests; no program has talked to a real node. |
| Web panel | 🟡 | The web panel (eight languages: set-up, users, network, settings, update) has its texts checked and is exercised against a mock of the node; it has never been served from a board. |
| Spindle and lamps | 🟠 | The 0-10 V speed signal, the run and direction lines (no reversing while turning) and the five lamp inputs are tested on the host; no spindle drive or machine has been connected. |
| Updates | 🟠 | Upload into one of two firmware slots with rollback and confirmation is tested on the host; an update has never run on a board. |
| Security | 🟡 | Set-up code on the USB console, access token, users and roles, optional HTTPS, and nothing secret compiled in; it stops by itself when the link drops. It has no emergency-stop input yet and no independent review. |
| Real hardware | 🔴 | No board has run this firmware, and the carrier board with the RS-232 converter, the lamp inputs and the spindle drive is only a design on paper. |
| Documentation | 🟢 | README in eight languages, the protocol, the firmware, the wiring and the carrier-board design. |

## 🎯 Overview

* The **control link** (TCP, port 5800 by default): a framed protocol with a CRC, a login with an access token, arming, a queue of bytes for the control unit and a stop when the session goes quiet or drops. The program sends the bytes of a move already spaced and the node's UART puts them on the line exactly, which a USB serial adapter cannot do at speed.
* The **serial line to the control unit**: 38400 baud 8N1 through a MAX3232, with the DTR, DCD and RI lines of the handshake. The pins, and whether the transceiver inverts the lines, are settings.
* The **lamps**: HOME X, HOME Y, HOME Z, ENABLE and TOOL are read through a buffer transistor, shown in the status and on the panel, and a request can make the node drop the rest of a search the moment an axis is home, so the program no longer has to read the home line in the middle of the stream.
* The **spindle**: a speed PWM for a 0-10 V converter (24000 rpm at full signal by default, for a variable-frequency drive such as the Huanyang HY02D223B) and run forward / reverse lines, with no change of direction while it turns or right after it stops, and a stop whenever the machine is stopped.
* The **panel** (HTTP and HTTPS, eight languages): first-run set-up with a code shown on the USB console, users, Ethernet or Wi-Fi, the control link, the RS-232 pins, the lamps and the spindle, a log, and updates by upload into one of two firmware slots with rollback. Nothing secret is compiled into the image.
* The **self-protection**: a guard that watches the tasks and the memory, a gate at the door of the panel when memory is short, and the confirmation of a new image once the node can be reached.
* The **boards**: the Waveshare ESP32-S3-ETH (W5500 Ethernet) and an ESP32-S3-WROOM-1 N16R8 (Wi-Fi), one image for each. The design of the carrier board (100 x 100 mm) is in `docs/BOARD.md`.
* The **machines**: the Bungard CCD MTC and CCD/2 MTC, with the serial-port control unit of the DOS software 4.6. The two ATC models are not covered; the XL models and later machines have a modern control unit of their own.
* The **autonomous finish** (off by default, a setting): when the program or the server goes away, the node runs the bytes already in its queue to the end - the spindle commands queued among them included - then switches the spindle and the drivers off. The queue lives in the external RAM (up to 4 MB, minutes of motion), a time limit ends it and so do an Abort and the panel's stop button. **Planned, not built yet**: a push-button input for a physical stop.

## 🧩 Role in EH3D

EH3D-NODE-BUNGARD is the last stage between the EH3D programs and the machine: EH3D-DRILLPRO and EH3D-ROUTEPRO connect to it today, and EH3D-SERVER will sit in front of it. It is the first node of the EH3D family and its settings, panel and update machinery are meant to be shared with the nodes that follow (the common base lives in EH3D-COMMON).

## 📂 Repository Structure

```text
EH3D-NODE-BUNGARD/
├── core/         the link's frames and session rules, the byte queue and the settings: no hardware, tested on a computer
├── main/         the ESP-IDF firmware: UART, control link, lamps, spindle, network, settings store, web server
├── panel/        the web panel, packed into the firmware
├── tests/        host tests and the checks of the panel's texts
├── tools/        build_node.sh, pack_panel.py, check_sdkconfig.py, ci_validate.py, panel_mock.mjs
├── CMakeLists.txt, partitions.csv, sdkconfig.defaults, sdkconfig.board.*  the ESP-IDF project, the flash layout and the settings of each board (s3-eth, s3-wifi)
├── docs/         PROTOCOL, NODE_FIRMWARE, WIRING and BOARD
├── images/       banner
└── eh3d.project.json
```

## 🛠️ BUILD & RUN

You need a C++17 compiler and CMake for the host tests, and ESP-IDF 5.5 (or Docker) for the firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (or `s3-wifi`) builds an image in the ESP-IDF container; on Windows an installed ESP-IDF 5.5.5 does the same. The image `dist/eh3d-node-bungard-<version>-<board>-complete.bin` is for ONE board: flash it at address 0 with `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <image>`. Then join the Wi-Fi `EH3D-SETUP-xxxxxx` the node opens, open `http://192.168.4.1/` and enter the set-up code that the node shows on its USB console; the access token, the network and the pins are set from the panel.

## 🔗 Related Projects

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): the drilling program; it drives the machine through this node with `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): the routing program; it shares the protocol library and the machine file
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): the common parts of the EH3D family; the base of the node's firmware lives there
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): the center of the EH3D family: it runs the jobs of the programs and of the other clients on the machine through this node

## 📚 Documentation & Community

* [The control link](docs/PROTOCOL.md): frames, login, arming, queue, spindle, lamps and the rules that stop the machine.
* [The firmware](docs/NODE_FIRMWARE.md): set-up, settings, panel, update with rollback, memory and self-protection.
* [Wiring](docs/WIRING.md): the RS-232 port, the lamps and the spindle's drive.
* [The carrier board](docs/BOARD.md): the design to draw the schematic from.
* [Changelog](CHANGELOG.md)
* [Contributing](CONTRIBUTING.md)
* [Security policy](SECURITY.md)
* [Support](SUPPORT.md)

## 👤 AUTHOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENSE

Software: GPL-3.0-or-later. See [LICENSE](LICENSE). Hardware designs of the suite use CERN OHL-S.
