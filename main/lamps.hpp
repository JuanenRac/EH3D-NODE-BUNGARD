// EH3D-NODE-BUNGARD - the lamps of the control unit. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once

#include <cstdint>

#include "core/bungard_config.hpp"

namespace eh3d {

// Sets the lamp pins of the settings up as inputs (with pull-ups).
void lamps_start(const config::LampsSetting& setting);
// The lamps, debounced: HOME X 1, HOME Y 2, HOME Z 4, ENABLE 8, TOOL 16. A pin that is not wired (-1) reads 0. Called by the control link's loop only.
std::uint8_t read_lamps();
// The lamps as they are this instant, with no debounce and no state: for the panel's status.
std::uint8_t peek_lamps();

}  // namespace eh3d
