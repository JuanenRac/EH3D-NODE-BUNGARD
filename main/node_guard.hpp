// EH3D-NODE-BUNGARD - the node's own watchdog over its health (rules in core/node_guard.hpp, tested on a computer). Nothing here has run on a board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>

#include "core/node_guard.hpp"

extern "C" {
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_partition.h"
}

namespace eh3d::nodeguard {

// Says that a task which runs for ever is needed by the node: call it right before the task is made, so a task that could not be made (no memory) is missed all the same. The name is the one the
// task is made with (FreeRTOS keeps its first 15 characters).
void expect_task(const char* name);

// A task that must keep moving says so by calling this once each time round its loop (a loop that waits for ever for work does not call it: only a loop that must come round in under half a
// minute does). From the first call on the guard watches the count: a count that does not change for `kStallMs` is a stalled task, told in the log and handled like a missing one.
void beat(const char* name);

// A subsystem of the machine (the bits of core/node_guard.hpp: kSubSpindle, kSubUart...) reports itself up when it has started. `subsystems_up()` is what the confirmation of a new image reads.
void subsystem_up(std::uint32_t bit);
std::uint32_t subsystems_up();

// Reads back the image just written in `partition` (its first `length` bytes) and compares the board it names (core/board_mark.hpp) with this node's board. False: the image is for another board and
// must be refused ("wrong_firmware"); the log says which board it named.
bool image_board_ok(const esp_partition_t* partition, std::size_t length);

// What this boot learned from the ledger of restarts: reads the memory that survives a software reset the first time it is called (call it early, before anything that depends on it).
BootInfo boot_info();

// A restart someone asked for on purpose (the panel) marks the memory that survives a software reset, so that the next boot tells it from a restart the guard made.
// Call it right before the restart is scheduled. It does not touch the ledger: deliberate restarts do not count against the limit of the guard.
void note_deliberate_restart();

// Starts the task that looks at the node every few seconds, and the handler of a failed allocation. Called once, first thing, by memory::start_monitor().
void start();

// An image is being written to the flash: an allocation that fails then is not turned into a clean restart.
void note_firmware_write(bool writing);

// False while the memory is short (core/node_guard.hpp, Gate): the panel's servers refuse new connections.
bool accept_gate_open();
// The `open_fn` of the HTTP servers: refuses a new connection (ESP_FAIL closes the socket) while the gate is closed, before anything is allocated for it.
esp_err_t http_open_gate(httpd_handle_t server, int socket_fd);

}  // namespace eh3d::nodeguard
