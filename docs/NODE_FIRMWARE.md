# The firmware of the node

The firmware of an **ESP32-S3** node with two board profiles: `s3-eth`, the Waveshare ESP32-S3-ETH whose way in is the cable (W5500 Ethernet over SPI), and `s3-wifi`, an ESP32-S3-WROOM-1 N16R8 whose way in is Wi-Fi. An image is for ONE board. It turns the RS-232 port of the control unit of a Bungard CCD machine into a network port ([PROTOCOL.md](PROTOCOL.md)), reads the machine's lamps, drives a spindle through a 0-10 V speed signal and run/direction lines ([WIRING.md](WIRING.md)), and has a web panel for everything that differs from one node to another.

**Nothing here has run on a board.** What has been done: the rules of the link, the settings and what every node shares are tested on a computer (nine suites), the messages the Rust library of EH3D-DRILLPRO speaks were exercised against a stand-in node, the panel was exercised against a stand-in node in a script (`tools/panel_mock.mjs`), and both images build with ESP-IDF 5.5.5 without warnings.

## What a node is

The generic part of this firmware (settings, network, panel server, users, update, log, self-protection) is the **firmware base** shared by the EH3D nodes: it lives in [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON) (`firmware_base/`) and a tool there keeps the copy in this project identical to it.

Everything that differs from node to node is **one settings document in flash**, edited from the node's own web panel, so the same image serves every node and no password, network or token is compiled in. The document has the settings every EH3D node has (identity, way in, access point, address, clock, panel mode, language, periodic restart) and a `machine` section of this node (the control link, the RS-232 pins, the lamps and the spindle). The code that reads, checks and writes it is `core/node_settings.hpp` and `core/bungard_config.hpp`: it touches no hardware, so all of it is tested on a computer.

## First start

A node with no user yet is in **set-up**:

1. It opens the Wi-Fi network `EH3D-SETUP-xxxxxx` (the last bytes of its MAC address), whose password is the set-up code, and repeats on its USB console every fifteen seconds how to continue. The code is made the first time the node starts and kept; it appears only on the USB console.
2. Join that network, open `http://192.168.4.1/` and enter the set-up code to create the administrator.
3. In the panel set the way in (Ethernet, or the Wi-Fi to join), the node's name and, in **Machine**, the access token of the control link. Without a token the node refuses to be controlled.

Holding the BOOT button for eight seconds in the first thirty after power-up erases the settings and the users: the node is back in set-up. A node that has no address after ninety seconds opens the set-up network again so that it cannot be left unreachable.

## The panel

HTTP on port 80 and HTTPS on port 443 (the node makes its own certificate the first time; the panel's mode `http`, `both` or `https` is a setting, `both` by default, and HTTP redirects to HTTPS when the mode is `https`). Eight languages: English, Spanish, German, French, Italian, Japanese, Chinese and European Portuguese, chosen by the browser and changeable. Two roles: **administrator** (everything) and **viewer** (reads). A wrong password is slowed down and a source address that keeps failing is blocked for a while.

The menu is on the left of a wide screen (the same layout as the panels of the ARMOR nodes) and on top of a narrow one, a phone, with the language, theme and sign-out next to it.

| Page | What it does |
|---|---|
| Status | Live: the link (program connected, logged in, armed), the queue and the bytes sent, the five lamps, the control unit's input lines, the spindle's speed and direction, and a warning while the node is finishing a job it holds. The only thing it does to the machine is the **stop button** (administrators): it empties the queue and switches the spindle and the drivers off, with or without a program connected |
| Machine | The `machine` settings in four cards: **Link** (port, access token, open control, idle time, queue, whether it finishes the job it holds when the program goes away, and for how long at most), **Control unit** (TX, RX, DTR, DCD, RI, CTS, DSR and whether the transceiver inverts the lines), **Lamps** (the five pins and their polarity) and **Spindle** (top speed, PWM pin and frequency, run forward / reverse pins, single direction line, polarity, settling time between directions). Wrong values are shown next to their field |
| Network | Ethernet or Wi-Fi (scan and join), fixed address or DHCP, the node's host name, the node's own access point |
| Users | Create, change and delete users and roles; change the own password |
| Firmware | The two slots and the version each holds; upload an image into the other slot; boot the other slot at the next restart (the way back to the version that ran before an update, or forward to the one just installed). An empty slot or an image of another project is refused |
| Log | The node's log, with the memory report of every minute |
| System | Restart, factory reset (asks for the word RESET), periodic restart, time zone and NTP |

**While a program holds the machine armed, the panel refuses to save settings** (HTTP 409 `machine_armed`) and postpones a restart: a write to flash can stall the memory for a moment, and a move must not wait for it. Settings that the link reads at every connection (the token, open control, the idle time and the settling time between directions) apply as soon as they are saved; the rest (pins, port, queue, top speed) are taken when the node starts, and the panel says so and offers the restart.

The API of the panel is under `/api/v1/`; the part of this node is `GET` and `PUT /api/v1/machine` (the settings without the token, which is write-only: an empty token keeps the stored one and `token_clear` removes it) and `GET /api/v1/machine/status`. A token is 8 to 64 printable characters.

## Update and rollback

The firmware is replaced from the panel in three ways: by the **upload** of the `.bin`; by the **update from GitHub** (`main/github_update.cpp`: the node looks for the newest release of `JuanenRac/EH3D-NODE-BUNGARD`, `core/release_assets.hpp` picks the `<image>-<version>-<board>-app.bin` of its own board and refuses any address that is not a file of the repository's releases, the download is checked against the SHA-256 that the release publishes and against the project name, the version inside and the one that runs, and an administrator asks for each step); and by **switching to the other slot**, which can first receive a **copy** of the running firmware. The releases are not signed, so the update from GitHub trusts the HTTPS connection to github.com and the hash. In the three the image goes into the slot that is not running, the node checks it and boots it. A **new image is on probation**: it is confirmed once the panel is up and the node can be reached on some network for thirty seconds; if the panel cannot start, or no network is reachable within five minutes, the boot loader goes back to the previous image by itself. The image of the application (`...-app.bin`) is what the panel takes; the complete image (`...-complete.bin`: boot loader, partition table and application) is written with `esptool` at address 0 by cable. There is no update from GitHub yet: it needs a signing key for the EH3D releases, and there is a clearly marked place in `web_server.cpp` for it.

## Memory and self-protection

The board has some 300 KB of internal memory, shared by the radio, the drivers and every task stack, and 8 MB of external RAM (PSRAM). TLS takes its memory from the external RAM and falls back to the internal one on a board without it; requests above 4096 bytes go to the external RAM first; the tasks that only read counters have their stack there. The minute line of the log gives the free internal memory, the least it has had and the biggest block.

A node that runs for months has to notice when it is in trouble and put it right without turning into a restart loop. The rules are pure functions in `core/node_guard.hpp` and `core/task_health.hpp` (tested on a computer) and `main/node_guard.cpp` acts: a guard looks every two seconds at the tasks the node registered as needed and at the internal memory, and after a minute of trouble logs it and restarts the node once, cleanly, at most three times an hour; **a gate at the door of the panel** refuses a new connection while the internal memory is under 28 KB; and a failed allocation is written in memory that survives a restart. The control link and the spindle are not touched by the gate, and a restart stops them as the link rules say (queue emptied, drivers and spindle off).

## Build

`tools/build_node.sh s3-eth` (or `s3-wifi`) writes `dist/eh3d-node-bungard-s3-eth.bin` in the ESP-IDF container (Linux or WSL with Docker, one build at a time). On Windows an installed ESP-IDF 5.5.5 builds the same, with `-D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.board.<board>"`, and the images are `dist/eh3d-node-bungard-<version>-<board>-{app,complete}.bin` with their `.sha256`. The panel is packed into the firmware by `tools/pack_panel.py`. A cached `sdkconfig` keeps the old value of a line added to `sdkconfig.defaults`: delete `.compile/<board>` (or `build/<board>`) before building and run `python tools/check_sdkconfig.py` afterwards; it lists every line of `sdkconfig.defaults` that the build did not take.

The host tests (`build.bat` or `./build.sh`) build the core with any C++17 compiler and run ten suites (the update from GitHub: which file of a release is the image of a board, which addresses may be downloaded from and which versions are newer, is `test_release`): the link's frames and session rules, the settings and the `machine` section, what every node shares (retries, Improv Wi-Fi over serial, users and sessions, the network plan, JSON), the panel's texts in eight languages and the stacks of the tasks.

## What is not done

- The node's **finishing of the job it holds** has not run on a board; its rules (the state machine, the queued spindle commands and their direction rules) are tested on a computer.
- A **physical stop input** (the carrier board has a place for a push-button).
- The **signature of the releases**: the update from GitHub trusts the hash that the release publishes; a signing key and its check do not exist yet. The update from GitHub itself has not run on a board against the real GitHub.
- The panel has been looked at in a real browser (Edge, against the stand-in node `tools/panel_mock.mjs`, which serves it without a board), not on a board.
- Everything on a **real board**: the first flash, the first set-up, the link against the Rust library and a machine.
