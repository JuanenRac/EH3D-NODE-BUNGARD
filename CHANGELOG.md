# Changelog

## [Unreleased]

- **The panel in the look of EH3D-BUNGARD-GUI and EH3D-STUDIO:** the Forge theme by default (warm black, amber, cards with two cut corners) and Paper as the light theme; the pictures of the README are taken again with it, six pages, by `tools/panel_shots.mjs` against the stand-in node.
## [0.0.3] - A left menu for the web panel and the live panel of the scripts

- **Security policy and support page of the node:** they described a file reader; they now describe the control link, the panel, what is tested and what is not.
- **README in the universal layout, in eight languages:** the README has the same sections in the same order as the other repositories of the family (status, overview, architecture, features, security, interfaces, configuration, quick start, testing, structure, roadmap and known limits), with figures taken from the code and from the tests that were run. The Architecture and Repository Structure pictures are drawn from `docs/architecture.json` and `docs/structure.json` by `tools/make_architecture.py` and `tools/make_structure.py`, and the CI checks that they match.
- **The build, build-test and test scripts show a live panel:** a banner with the name and version, one line per step with its time, the number of tests that passed and a summary at the end, with the whole output kept in a log file. `test.sh` and `test.bat` run only the tests and never change the version; without `tools/pretty_run.py` the scripts behave as they always did.
- **The layout of the README is checked in the CI:** the validator runs `tools/universal_readme_check.py` and lists, as a warning, what differs from the universal layout of the README.
- **The panel has its menu on the left, like the panels of the ARMOR nodes:** on a wide screen the pages are a column on the left (with an icon each), the brand, the language, the theme and the sign-out above them; on a narrow screen, a phone, the menu goes on top as a row that slides sideways. The Machine page keeps the parameters of the Bungard machine (control link, RS-232 unit, lamps and spindle).

## [0.0.2] - Settings, web panel, update with rollback, stop button and finishing the held job

- **Settings and panel:** everything that differs from one node to another is one settings document in flash, edited from the node's web panel (HTTP and HTTPS, eight languages): the first-run set-up with a code shown on the USB console, users and login, Wi-Fi or Ethernet, the control link (port, access token, idle time, queue), the RS-232 pins, the lamps and the spindle. No password, network or token is compiled into the image. Updates by upload from the panel into one of two firmware slots, with rollback and a switch between the slots; a log; a restart and a factory reset. While a machine is armed the panel refuses to save, so a write to flash cannot cut a move.
- **Finishing the job it holds:** with the setting `finish_on_loss` (off by default), when the program or the server goes away the node runs the bytes already in its queue to the end, with the spindle commands queued among them, then switches the spindle and the drivers off. The queue lives in the external RAM (up to 4 MB), a time limit and an Abort end it, and the panel has a stop button that works with or without a program connected.
- **Self-protection:** a guard that watches the tasks and the memory, a gate at the door of the panel when memory is short, and the confirmation of a new image once the node can be reached.
- **Firmware:** ESP-IDF 5.5 for the Waveshare ESP32-S3-ETH (W5500 Ethernet) and for an ESP32-S3-WROOM-1 (Wi-Fi); both images build without warnings. Nothing has run on a board yet.
- **Documentation:** the link protocol, the wiring of the RS-232 port, the lamps and the spindle's drive, and the design of the carrier board.
- Host tests of the link rules, the settings, the machine section and what every node shares, a check of the panel's texts in eight languages and of the tasks' stacks, in CI.

## [0.0.1] - First scaffolding of the node

- **Control link:** the frames of the link (CRC-16, a decoder that resynchronises after damage), the byte queue for the control unit and the rules of a control session (token login, arming, a stop when the session goes quiet or the connection drops), tested on a computer.
- **Spindle output:** a speed PWM for a 0-10 V converter (24000 rpm at full signal by default), run forward and reverse lines or one run and one direction line, a frame to set speed and direction, a stop with the machine, no direction change while turning or before the settling time, and the speed in the status and the panel.
- **Machine lamps:** HOME X, Y and Z, ENABLE and TOOL are read through a buffer transistor, shown in the status and on the panel, and a StopOnHome request drops the rest of a search as soon as an axis is home.
- **Machines:** the Bungard CCD MTC and CCD/2 MTC; the two ATC models are not covered.
