// EH3D-NODE-BUNGARD - the lamps of the control unit. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// HOME X, HOME Y and HOME Z light when the axis is on its limit switch; ENABLE when the machine is active; TOOL is not known to light.
// Each lamp is a PNP transistor to 5 V with a 1 k resistor to ground; an NPN buffer between the lamp's anode and the pin keeps the
// control unit's circuit untouched and gives the pin a clean logic level. The pins and the polarity are the settings' (machine.lamps), read when the node starts.
#include "lamps.hpp"

extern "C" {
#include "driver/gpio.h"
}

namespace eh3d {
namespace {
int g_pins[5] = {-1, -1, -1, -1, -1};
bool g_active_low = true;
std::uint8_t g_last = 0;

std::uint8_t sample() {
  std::uint8_t v = 0;
  for (int i = 0; i < 5; ++i) {
    if (g_pins[i] < 0) continue;
    const bool level = gpio_get_level(static_cast<gpio_num_t>(g_pins[i])) != 0;
    if (level != g_active_low) v |= static_cast<std::uint8_t>(1u << i);
  }
  return v;
}
}  // namespace

void lamps_start(const config::LampsSetting& setting) {
  g_pins[0] = setting.home_x;
  g_pins[1] = setting.home_y;
  g_pins[2] = setting.home_z;
  g_pins[3] = setting.enable;
  g_pins[4] = setting.tool;
  g_active_low = setting.active_low;
  for (int pin : g_pins) {
    if (pin < 0) continue;
    gpio_config_t cfg{};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&cfg);
  }
  g_last = sample();
}

// A lamp counts once two readings in a row agree (the loop reads every few milliseconds), so a spike does not stop a search.
std::uint8_t read_lamps() {
  static std::uint8_t stable = 0;
  const std::uint8_t now = sample();
  if (now == g_last) stable = now;
  g_last = now;
  return stable;
}

std::uint8_t peek_lamps() { return sample(); }

}  // namespace eh3d
