// EH3D-NODE-BUNGARD - the TCP link to the program that drives the machine. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once

#include <cstdint>

#include "core/bungard_config.hpp"

namespace eh3d {

struct LinkState {
  bool client = false;
  bool authenticated = false;
  bool armed = false;
  std::uint32_t spindle_rpm = 0;
  bool spindle_reverse = false;
  std::uint8_t lamps = 0;
  bool draining = false;   // the node is finishing the job it holds
};

// Starts the task that listens on the port of the settings (machine.link). One program at a time. The port and the queue size are taken now; the token and the rest are read at every connection.
bool link_start(const config::LinkSetting& setting);
LinkState link_state();
// Stops the machine now (queue emptied, spindle and drivers off), whoever is or is not connected. For the panel's stop button.
void link_emergency_stop();

}  // namespace eh3d
