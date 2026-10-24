// EH3D-NODE-BUNGARD - the serial line to the control unit of the machine. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/bungard_config.hpp"

namespace eh3d {

// Starts the UART (38400 8N1), the DTR output, the input pins and the task that sends the queue out. The pins and the polarity of the RS-232 lines are the settings' (machine.uart), and so is the
// size of the queue (machine.link.queue_bytes).
bool uart_start(const config::UartSetting& pins, std::size_t queue_bytes);

std::size_t queue_free();
std::size_t queue_size();
bool enqueue(const std::uint8_t* data, std::size_t n);
// A spindle command that waits in the queue: it is applied when every byte queued before it has gone out. False when the list of commands is full.
bool enqueue_spindle_event(std::uint32_t rpm, bool reverse);
// True while bytes, or spindle commands among them, are still waiting.
bool queue_busy();
void flush_queue();
// Empties the queue and sends the byte that switches the stepper drivers off (0xC0), ahead of anything else.
void send_drivers_off();
void set_dtr(bool on);
// CTS 1, DSR 2, RI 4, DCD 8 (inputs that are not wired read 0).
std::uint8_t input_lines();
std::uint32_t bytes_sent();
// Takes what the control unit has sent (non-blocking).
std::size_t read_rx(std::uint8_t* out, std::size_t max);

}  // namespace eh3d
