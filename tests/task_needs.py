# EH3D-NODE-BUNGARD - what each FreeRTOS task of this node needs at least (the check in check_task_safety.py reads it; a task that is not here fails the check).
# Every number is the size it has now or more: shrinking one has to be a decision, not an accident.
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
NEEDS = {
    "eh3d-link": (6144, "the control link: frames, the session and the replies (the decoder and the reply buffer are static)"),
    "eh3d-uart-tx": (3072, "writes the queue to the UART"),
    "clock-sync": (3072, "asks the time servers"),
    "github-check": (12288, "TLS and the release JSON"),
    "github-ota": (12288, "TLS, the download and the signature check"),
    "slot-copy": (6144, "copies a firmware slot"),
    "improv": (5120, "the Wi-Fi setup over USB"),
    "net-rescue": (6144, "brings the network back"),
    "setup-hint": (3072, "the lamp of the setup mode"),
    "boot-button": (3072, "the button of the board"),
    "confirm": (3072, "the confirm button"),
    "node-guard": (4096, "watches the other tasks"),
    "mem-watch": (3072, "watches the memory"),
}
