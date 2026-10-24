// EH3D-NODE-BUNGARD - the spindle output. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once

#include <cstdint>

#include "core/bungard_config.hpp"

namespace eh3d {

// Sets the pins to the safe state (stopped) and starts the PWM, from the settings (machine.spindle). Returns false if the PWM could not be set up; true and nothing touched when the top speed is 0.
bool spindle_start(const config::SpindleSetting& setting);
// The top speed the output was set up for (0: no spindle output).
std::uint32_t spindle_max_rpm();
// Speed in rpm (0 = stopped) and direction. The speed becomes a PWM duty cycle: 0 % at 0 rpm, 100 % at the top speed of the settings,
// which an external PWM to 0-10 V converter turns into the 0-10 V speed signal of the spindle's control unit.
void spindle_set(std::uint32_t rpm, bool reverse);
// The speed and the direction the output has now (a command that waits in the queue has not changed them yet).
std::uint32_t spindle_rpm_now();
bool spindle_reverse_now();

}  // namespace eh3d
