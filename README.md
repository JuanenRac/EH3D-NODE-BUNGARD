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
  <img src="https://img.shields.io/badge/Version-0.0.3-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Honesty check - what actually runs today:** firmware in C++17 for the ESP32-S3 (ESP-IDF 5.5) that turns the RS-232 port of the control unit of a Bungard CCD machine into a network port: a program connects over TCP with an access token, arms the machine and sends the bytes of a move, which the node queues and puts on the line at the line's own pace. It also reads the machine's lamps (HOME X, Y and Z, ENABLE, TOOL), drives a spindle through a 0-10 V speed signal and run/direction lines, and has a web panel with a side menu (eight languages) for set-up, users, network, update and the machine's own settings. The rules of the link and the settings are tested on a computer (9 test programs, 2,209 checks counted) and the firmware builds without warnings for both boards. **It has not been flashed or run on a board yet**, nothing has been connected to a machine, and the carrier board that brings the RS-232 converter, the lamp inputs and the spindle's drive together is a design on paper.

---

> **Safety and security status:** this is not a safety system. A node that is wired wrongly, or a spindle drive that is wrongly set, can move a machine or spin a tool. The node refuses to be controlled without an access token, takes bytes for the machine only while a program has armed it, and stops (queue emptied, stepper drivers and spindle off) when the session goes quiet or the connection drops (unless the optional setting that lets it finish the job it holds is on). It does not see the machine's limit switches except through the lamps, and it has no emergency stop of its own yet (the carrier board has a place for a push-button input). The spindle's drive is on mains voltage: wire and set it with the maker's manual. The panel never moves the machine, and it refuses to save settings while a program holds the machine armed. The project is independent: it is not made, endorsed or supported by Bungard Elektronik, and product names appear only to say which machines and drives it is meant to work with.

## 📋 At a glance

| | |
|---|---|
| Version | `0.0.3` |
| Maturity | Scaffolding |
| Tests | 9 host test programs (2,209 checks counted) |
| Stack | C++17, ESP-IDF 5.5, ESP32-S3 |
| On real hardware | No: both firmware images build, but nothing has been flashed or run on a board, and nothing has been connected to a machine |

**Jump to:** [Overview](#-overview) · [Architecture](#-architecture) · [Quick start](#-quick-start) · [Testing](#-testing) · [Structure](#-repository-structure) · [Related](#-related-projects)

## 🚦 Project status

Each line is one part of this repository and the light says how far it is today. It is written from what the repository can show (tests, builds, runs), not from what is planned.

🟢 Done and verified · 🟡 Works, with known limits · 🟠 Partly done or not verified yet · 🔴 Not done or not working

| Area | State | What it means here |
|---|---|---|
| Software | 🟡 | The firmware for the ESP32-S3 (ESP-IDF 5.5, C++17) builds without warnings for both boards, Ethernet and Wi-Fi; no image has been flashed. |
| Tests | 🟢 | 9 host test programs pass (`ctest`): 2,209 checks counted in the link, settings, machine rules and the shared base, plus the check of the panel texts (315 rows in 8 languages) and of the task stacks (10 tasks). |
| Control link | 🟡 | The framed protocol (CRC, token login, arming, byte queue, stop when the session goes quiet) is covered by the host tests; no program has talked to a real node. |
| Web panel | 🟡 | The web panel (side menu, eight languages: set-up, machine, network, users, update, log, system) has its texts checked and is exercised against a mock of the node in a browser; it has never been served from a board. |
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
* The **panel** (HTTP and HTTPS, eight languages, the menu on the left of a wide screen and on top of a phone): first-run set-up with a code shown on the USB console, users, Ethernet or Wi-Fi, the control link, the RS-232 pins, the lamps and the spindle, a log, and updates by upload into one of two firmware slots with rollback. Nothing secret is compiled into the image.
* The **self-protection**: a guard that watches the tasks and the memory, a gate at the door of the panel when memory is short, and the confirmation of a new image once the node can be reached.
* The **boards**: the Waveshare ESP32-S3-ETH (W5500 Ethernet) and an ESP32-S3-WROOM-1 N16R8 (Wi-Fi), one image for each. The design of the carrier board (100 x 100 mm) is in `docs/BOARD.md`.
* The **machines**: the Bungard CCD MTC and CCD/2 MTC, with the serial-port control unit of the DOS software 4.6. The two ATC models are not covered; the XL models and later machines have a modern control unit of their own.
* The **autonomous finish** (off by default, a setting): when the program or the server goes away, the node runs the bytes already in its queue to the end - the spindle commands queued among them included - then switches the spindle and the drivers off. The queue lives in the external RAM (up to 4 MB, minutes of motion), a time limit ends it and so do an Abort and the panel's stop button. **Planned, not built yet**: a push-button input for a physical stop.

## 🧩 Role in EH3D

EH3D-NODE-BUNGARD is the last stage between the EH3D programs and the machine: EH3D-DRILLPRO and EH3D-ROUTEPRO connect to it today, and EH3D-SERVER sits in front of it for the other clients. It is the first node of the EH3D family and its settings, panel and update machinery are meant to be shared with the nodes that follow (the common base lives in EH3D-COMMON).

## 🏛️ Architecture

<p align="center">
  <img src="images/architecture.svg" alt="EH3D-NODE-BUNGARD architecture: who uses it, what it holds and what it depends on" width="100%">
</p>

How the parts of this repository fit together: who uses it, what it holds and what it depends on. The picture is drawn from `docs/architecture.json` and checked against the code.

## 🖼️ Screenshots

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="status page of the web panel" width="100%">
</p>

<p align="center"><em>The Status page of the web panel, served by the stand-in node that is used to develop it: no board has run the firmware yet.</em></p>

<p align="center">
  <img src="images/screenshots/panel-machine.png" alt="machine page of the web panel" width="100%">
</p>

<p align="center"><em>The Machine page: the control link, the RS-232 pins, the lamps and the access token.</em></p>

## ✨ Features

* **A panel with a side menu:** seven pages (Status, Machine, Network, Users, Firmware, Log, System), the menu on the left of a wide screen and on top of a phone, with the language, the theme and the sign-out beside it.
* **One image, no secrets:** everything that differs from node to node is one settings document in flash edited from the panel; no password, token or network is compiled in.
* **Bytes with the right timing:** the program places each step and the node's UART sends the bytes at the line's own pace, which a USB adapter cannot keep.
* **Home seen live:** the five lamps are read and a request drops the rest of a search the moment an axis is home.
* **Spindle with guard rails:** speed capped at the top speed the node was built for, no reversing while turning or right after stopping, a stop with the machine.
* **Update with rollback:** an image goes into the slot that is not running and is on probation until the node can be reached; the boot loader goes back when it cannot.
* **Looks after itself:** a guard watches the tasks and the memory, and a node that has no address after ninety seconds opens the set-up network again so it cannot be left unreachable.

## 🔒 Security & Safety

| Threat | Defense |
|---|---|
| Someone on the network driving the machine | the link refuses everything but Hello until the access token (8 to 64 printable characters) is right; a node with no token refuses control unless the bench setting `allow_open` is on (off by default); one program at a time |
| A program that moves the machine by accident | bytes are accepted only while armed, and only if the whole frame fits the queue; a spindle command is cut to the top speed and refused when it would reverse a turning spindle |
| A cable that falls out or a program that dies mid-move | an armed session that is quiet for `idle_ms` (3000 ms by default) or whose connection drops empties the queue and sends the drivers-off byte `0xC0`; the spindle goes to 0 rpm |
| Someone reaching the panel | the first administrator is made with a set-up code that appears only on the USB console; two roles (administrator, viewer); a wrong password is slowed down and a source address that keeps failing is blocked for a while; HTTP, HTTPS or both |
| Secrets in the firmware or in the repository | nothing secret is compiled in: the token, the network and the pins are settings in flash; the token is write-only in the panel and the API |
| A settings write that stalls a move | while a program holds the machine armed the panel refuses to save settings (HTTP 409 `machine_armed`) and postpones a restart |
| A wrong pin assignment | pins that the board reserves (flash, PSRAM, USB, the Ethernet chip) are refused and no pin may do two jobs |
| A bad firmware image | an image of another project or an empty slot is refused; a new image is on probation and the boot loader goes back to the previous one if the panel does not come up or the node cannot be reached within five minutes |
| A node that runs out of memory or hangs | a guard watches the tasks and the internal memory and restarts without making a loop; a gate at the door of the panel when memory is short |
| Someone with the node in their hands | holding BOOT for eight seconds in the first thirty after power-up erases the settings and the users (physical access is a reset by design) |

**What is not defended:** the node has no emergency-stop input yet and does not see the limit switches except through the lamps; the update is not signed (the update from GitHub is not built: it needs a signing key); the self-made certificate of the HTTPS panel makes a browser warn; nothing has run on a board, and there has been no independent review. Report a vulnerability privately (see [SECURITY.md](SECURITY.md)).

## 🔌 Interfaces

| Interface | What it is | Where |
|---|---|---|
| Control link | TCP, port 5800 by default: frames `0xB5, type, length, payload, CRC-16`, Hello with the token, Arm, Write, Abort, Ping, StopOnHome, Spindle; the node answers Info, Status (every 200 ms), Rx, HomeSeen and Error | [docs/PROTOCOL.md](docs/PROTOCOL.md), `core/frame.hpp`, `main/link_server.cpp` |
| Web panel | HTTP on port 80 and HTTPS on port 443 (the mode is a setting), eight languages, API under `/api/v1/` (`GET`/`PUT /api/v1/machine`, `GET /api/v1/machine/status`, and the shared routes of the base) | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md), `main/web_server.cpp`, `panel/` |
| RS-232 to the control unit | UART at 38400 baud 8N1 through a MAX3232, with DTR (out), DCD and RI (in) | [docs/WIRING.md](docs/WIRING.md), `main/central_uart.cpp` |
| Lamps | HOME X, HOME Y, HOME Z, ENABLE and TOOL, five inputs through a buffer transistor | [docs/WIRING.md](docs/WIRING.md), `main/lamps.cpp` |
| Spindle | a speed PWM for a 0-10 V converter and run forward / reverse lines | [docs/BOARD.md](docs/BOARD.md), `main/spindle.cpp` |
| USB console | shows the set-up code and takes Improv Wi-Fi over serial | `main/improv_serial.cpp` |
| Set-up network | the node opens the Wi-Fi `EH3D-SETUP-xxxxxx` while it has no user | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md) |

## ⚙️ Configuration

Everything is a setting in flash, edited from the panel (page Machine), so one image serves every node. The values below are those of a node that has never been configured; a value out of range is refused when it is saved.

| Setting | Default | Range | What it does |
|---|---|---|---|
| `machine.link.port` | 5800 | 1024 to 65535 | the TCP port of the control link |
| `machine.link.token` | none | 8 to 64 printable characters; write-only | the access token; without one the node refuses to be controlled |
| `machine.link.allow_open` | off | on or off | a bench setting: accept control with no token |
| `machine.link.idle_ms` | 3000 | 200 to 60000 | how long an armed session may be quiet before the node stops |
| `machine.link.queue_bytes` | 262144 | 1024 to 4194304 | the byte queue for the control unit, in the external RAM |
| `machine.link.finish_on_loss` | off | on or off | finish the job already in the queue when the program goes away |
| `machine.link.max_drain_s` | 600 | 10 to 3600 | the most seconds the node may go on alone in that case |
| `machine.uart.*` | tx 17, rx 18, dtr 15, dcd 16, ri 21, cts and dsr not wired; transceiver inverts | a free pin of the board, or -1 where optional | the pins of the serial line |
| `machine.lamps.*` | HOME X 1, Y 2, Z 7, ENABLE 8, TOOL 38; active low | a free pin | the five lamp inputs |
| `machine.spindle.max_rpm` | 24000 | 0 to 65535 (0: no spindle output) | the speed at full signal |
| `machine.spindle.pwm_hz` | 2000 | 100 to 40000 | the frequency of the speed signal |
| `machine.spindle.reverse_delay_ms` | 3000 | 0 to 60000 | how long the spindle must have been stopped before it may turn the other way |
| `web.mode` | `both` | `http`, `both` or `https` | whether the panel is served on port 80, on 443 or on both |

The pins of the spindle (PWM 4, forward 5, reverse 6) are settings too. The settings every node has (name, way in, access point, address, clock, language, periodic restart) are in the base and described in [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md).

## 🚀 Quick start

You need a C++17 compiler and CMake for the host tests, and ESP-IDF 5.5 (or Docker) for the firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (or `s3-wifi`) builds an image in the ESP-IDF container; on Windows an installed ESP-IDF 5.5.5 does the same. The image `dist/eh3d-node-bungard-<version>-<board>-complete.bin` is for ONE board: flash it at address 0 with `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <image>`. Then join the Wi-Fi `EH3D-SETUP-xxxxxx` the node opens, open `http://192.168.4.1/` and enter the set-up code that the node shows on its USB console; the access token, the network and the pins are set from the panel. This path has not been followed on a real board yet.

## 🧪 Testing

The host tests (`build.bat` or `./build.sh`, then `ctest`) are 9 programs and all pass: `core_tests` (104 checks: the frames and the session rules), `test_base` (382), `test_clock` (prints ok), `test_settings` (392), `test_machine` (75), `test_shared` (627) and `test_shared_eth` (629, the Ethernet board profile) - 2,209 checks counted - plus `panel_text` (315 rows in 8 languages, 265 keys used, 36 firmware error codes with a text) and `task_safety` (10 tasks, no big copy of the settings on a small stack). They cover the CRC and the decoder that resynchronises after damage, token login, arming, the stop on silence, the spindle rules (cap, no reversing, delay), the settings and their ranges, pin conflicts and reserved pins, users and sessions, the network plan, the clock and JSON. The panel has also been exercised in a browser against `tools/panel_mock.mjs`, a stand-in node that serves it without a board. The CI runs `python -I tools/ci_validate.py` and the host tests on Linux with the warnings as errors. Building the two firmware images (ESP-IDF 5.5.5) is done by hand and is not part of the CI; nothing runs on a board.

## 🏗️ Build & Release

The version lives in `eh3d.project.json` and in the build, and each version has its entry in [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` build and run the host tests and `test.sh` / `test.bat` run only the tests, each in a live panel; neither changes the version. The firmware images come from `tools/build_node.sh` (or ESP-IDF 5.5.5 on Windows) and are named `eh3d-node-bungard-<version>-<board>-{app,complete}.bin`. No release has been published yet and no image has been flashed. The update from GitHub is not built: it needs a signing key.

## 🩺 Troubleshooting

| Symptom | What to do |
|---|---|
| The node cannot be reached | A node that has no address after ninety seconds opens the set-up network `EH3D-SETUP-xxxxxx` again; join it and set the way in from the panel. |
| The set-up code is lost | It appears only on the USB console, every fifteen seconds. Holding BOOT for eight seconds in the first thirty after power-up erases the settings and the users and starts the set-up again. |
| The link answers Error 1 or 2 | Not logged in, or a wrong token: the first frame must be Hello with the access token set in the panel (page Machine). |
| Error 3 or 4 | Not armed (send Arm first), or the queue is full (wait for the Status to show room). |
| Error 7 | The armed session was quiet for `idle_ms`: send Ping while a long move is queued. |
| Error 9, 10 or 11 | The spindle must stop before reversing, it was stopped too recently to reverse, or the node is finishing the job it holds. |
| The panel refuses to save (409 `machine_armed`) | A program holds the machine armed; disarm it, or press the stop button on the Status page, and save again. |
| The browser warns about the HTTPS panel | The node makes its own certificate the first time; use the `http` or `both` mode on a trusted network, or accept the certificate. |

## 📂 Repository Structure

<p align="center">
  <img src="images/repository-structure.svg" alt="Repository structure of EH3D-NODE-BUNGARD" width="100%">
</p>

<details>
<summary>Text version of the tree</summary>

```text
EH3D-NODE-BUNGARD/
├── core/
│   ├── frame.hpp session.hpp                    frames and session rules
│   ├── byte_queue.hpp drain.hpp                 byte queue, held-job finish
│   ├── bungard_config.hpp                       machine settings and pin rules
│   ├── node_settings.hpp auth.hpp               settings and users (shared base)
│   └── netplan.hpp json.hpp                     network plan and JSON (shared base)
├── main/
│   ├── app_main.cpp link_server.cpp             start and the control link
│   ├── central_uart.cpp lamps.cpp               the serial line and the lamps
│   ├── spindle.cpp machine_api.cpp              spindle outputs, machine API
│   ├── web_server.cpp network.cpp               panel server, network (base)
│   └── node_store.cpp node_guard.cpp            settings store, guard (base)
├── panel/
│   ├── index.html style.css                     the page and the side menu layout
│   └── app.js text.js                           the pages and the texts in eight languages
├── tests/                                       host tests of the link, the settings, the shared base and the panel's texts
├── docs/                                        protocol, firmware, wiring, board
├── tools/                                       image build, panel pack, checks, mock node
├── CMakeLists.txt                               the ESP-IDF project
├── partitions.csv                               the flash layout with two slots
├── sdkconfig.*                                  ESP-IDF settings and board profiles
├── images/                                      banner and the Architecture and Repository Structure pictures
├── .github/                                     CI and release workflows, issue and pull-request templates
├── build.sh build.bat                           build and run the host tests in a live panel
├── eh3d.project.json                            manifest: version, role, maturity and build command
├── CHANGELOG.md                                 what changed in each version
├── SECURITY.md SUPPORT.md                       how to report a vulnerability and where to ask
└── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   how to contribute, the code of conduct and the licence
```

</details>

## 🗺️ Roadmap & Known limits

**Known limits today**

* Nothing has run on a board: no image has been flashed, no set-up has been done on a real node, no program has talked to one and nothing has been connected to a machine.
* The carrier board with the RS-232 converter, the lamp inputs and the spindle's drive is a design on paper (`docs/BOARD.md`).
* The node has no emergency-stop input (the carrier board has a place for a push-button) and sees the limit switches only through the lamps.
* The update from GitHub is not built: it needs a signing key. The panel has been looked at in a browser against a stand-in node, not served from a board.
* The finishing of the held job has not run on a board; its rules are tested on a computer.
* Only the Bungard CCD MTC and CCD/2 MTC machines are meant; the ATC models are not covered.

**Next**

* Flash an image to each board, do the first set-up and check the link against the Rust library.
* Draw and build the carrier board, and connect a spindle drive and a machine.
* Add the push-button input for a physical stop.
* Sign the updates and fetch them from GitHub.

## 🌍 Languages

README and texts exist in English, Spanish, French, Italian, German, Chinese, Japanese and European Portuguese.

## 🔗 Related Projects

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): the drilling program; it drives the machine through this node with `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): the routing program; it shares the protocol library and the machine file
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): the common parts of the EH3D family; the base of the node's firmware lives there
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): the center of the EH3D family: it runs the jobs of the programs and of the other clients on the machine through this node

## 📚 Documentation

* [The control link](docs/PROTOCOL.md): frames, login, arming, queue, spindle, lamps and the rules that stop the machine.
* [The firmware](docs/NODE_FIRMWARE.md): set-up, settings, panel, update with rollback, memory and self-protection.
* [Wiring](docs/WIRING.md): the RS-232 port, the lamps and the spindle's drive.
* [The carrier board](docs/BOARD.md): the design to draw the schematic from.
* [Changelog](CHANGELOG.md)

## 🤝 Contributing & Community

* Before a pull request run the host tests (`./build.sh` or `build.bat`) and `python -I tools/ci_validate.py`: all must pass. See [CONTRIBUTING.md](CONTRIBUTING.md) and the [code of conduct](CODE_OF_CONDUCT.md).
* The README exists in eight languages: a change to the text is made in all eight files, and the two pictures are drawn from `docs/architecture.json` and `docs/structure.json` by `tools/make_architecture.py` and `tools/make_structure.py`.
* Nothing that sends bytes to a real control unit changes without a note of what was observed on hardware (or on a logic analyser) and a test with the simulator or the stand-in node.
* Report a vulnerability privately: [SECURITY.md](SECURITY.md). Questions, ideas and reports: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTHOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENSE

Software: GPL-3.0-or-later. See [LICENSE](LICENSE). Hardware designs of the suite use CERN OHL-S.
