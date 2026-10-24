// EH3D-NODE-BUNGARD - the spindle output. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Three outputs, all 3.3 V logic (an external converter or driver turns them into what the spindle's control unit wants):
//  - PWM: the speed. 0 % duty is 0 rpm and 100 % is the setting spindle.max_rpm; a PWM to 0-10 V module makes 0-10 V from it;
//  - FWD / REV: the direction. Two lines (run forward, run reverse; never both), or with spindle.dir_single one run
//    line (FWD) and one direction line (REV, active for reverse).
// Everything is off at start-up and whenever the speed is 0. The pins, the frequency and the top speed are the settings' (machine.spindle), read when the node starts.
#include "spindle.hpp"

#include <algorithm>
#include <atomic>

extern "C" {
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
}

namespace eh3d {
namespace {
constexpr char kTag[] = "eh3d-spindle";
constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;
config::SpindleSetting g_cfg;
std::uint32_t g_max_duty = 4095;
bool g_started = false;
std::atomic<std::uint32_t> g_now_rpm{0};
std::atomic<bool> g_now_reverse{false};

void out_level(int gpio, bool active) {
  if (gpio < 0) return;
  if (g_cfg.outputs_active_low) active = !active;
  gpio_set_level(static_cast<gpio_num_t>(gpio), active ? 1 : 0);
}

void out_init(int gpio) {
  if (gpio < 0) return;
  gpio_config_t cfg{};
  cfg.pin_bit_mask = 1ULL << gpio;
  cfg.mode = GPIO_MODE_OUTPUT;
  gpio_config(&cfg);
  out_level(gpio, false);
}
}  // namespace

bool spindle_start(const config::SpindleSetting& setting) {
  g_cfg = setting;
  g_started = false;
  if (g_cfg.max_rpm <= 0) return true;   // no spindle output: nothing is touched
  out_init(g_cfg.fwd_pin);
  out_init(g_cfg.rev_pin);
  // 12 bits of duty up to 19 kHz, 10 bits above (the LEDC timer divides 80 MHz)
  const bool fine = g_cfg.pwm_hz <= 19000;
  g_max_duty = fine ? 4095 : 1023;
  ledc_timer_config_t timer{};
  timer.speed_mode = kMode;
  timer.duty_resolution = fine ? LEDC_TIMER_12_BIT : LEDC_TIMER_10_BIT;
  timer.timer_num = LEDC_TIMER_0;
  timer.freq_hz = static_cast<std::uint32_t>(g_cfg.pwm_hz);
  timer.clk_cfg = LEDC_AUTO_CLK;
  if (ledc_timer_config(&timer) != ESP_OK) return false;
  ledc_channel_config_t ch{};
  ch.gpio_num = g_cfg.pwm_pin;
  ch.speed_mode = kMode;
  ch.channel = kChannel;
  ch.timer_sel = LEDC_TIMER_0;
  ch.duty = 0;
  ch.flags.output_invert = g_cfg.pwm_inverted ? 1 : 0;
  if (ledc_channel_config(&ch) != ESP_OK) return false;
  g_started = true;
  ESP_LOGI(kTag, "spindle output ready: %d rpm at full signal, PWM %d Hz on GPIO %d", g_cfg.max_rpm, g_cfg.pwm_hz, g_cfg.pwm_pin);
  return true;
}

std::uint32_t spindle_max_rpm() { return g_cfg.max_rpm > 0 ? static_cast<std::uint32_t>(g_cfg.max_rpm) : 0u; }

std::uint32_t spindle_rpm_now() { return g_now_rpm.load(); }
bool spindle_reverse_now() { return g_now_reverse.load(); }

void spindle_set(std::uint32_t rpm, bool reverse) {
  if (!g_started) return;
  const std::uint32_t top = static_cast<std::uint32_t>(g_cfg.max_rpm);
  rpm = std::min<std::uint32_t>(rpm, top);
  g_now_rpm = rpm;
  if (rpm > 0) g_now_reverse = reverse;
  if (rpm == 0) {
    // stop: the speed first, then the run lines
    ledc_set_duty(kMode, kChannel, 0);
    ledc_update_duty(kMode, kChannel);
    out_level(g_cfg.fwd_pin, false);
    out_level(g_cfg.rev_pin, false);
    return;
  }
  // direction and run lines first, then the speed
  if (g_cfg.dir_single) {
    out_level(g_cfg.rev_pin, reverse);
    out_level(g_cfg.fwd_pin, true);
  } else {
    out_level(g_cfg.fwd_pin, !reverse);
    out_level(g_cfg.rev_pin, reverse);
  }
  const std::uint32_t duty = static_cast<std::uint32_t>((static_cast<std::uint64_t>(rpm) * g_max_duty + top / 2) / top);
  ledc_set_duty(kMode, kChannel, duty);
  ledc_update_duty(kMode, kChannel);
}

}  // namespace eh3d
