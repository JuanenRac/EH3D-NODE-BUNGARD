/*
 * EH3D-NODE-BUNGARD - ESP32-S3 node in the box of a Bungard CCD drilling and routing machine (a Waveshare ESP32-S3-ETH on its cable, or an ESP32-S3-WROOM-1 N16R8 on Wi-Fi): a thin
 * network pipe to the RS-232 control unit. A program on the network sends the bytes of a move over a framed TCP link and the node sends them out of its UART at the line's own pace.
 * Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
 *
 * The hardware-independent core under core/ (the frames of the link, the queue, the rules of a control session, the settings) is built and tested on a computer (tests/); the files beside
 * this one are built with ESP-IDF (the build script of the project does it). None of it has run on a board yet, and nothing has been connected to a machine.
 *
 * What a node is, in one paragraph: everything that differs from node to node (its identity, its Wi-Fi, the pins, the access token of the control link, the spindle) is one settings
 * document in flash, edited from the node's own web panel, so the same firmware image serves every node and no password or token is compiled in. A node with no user yet is in
 * setup: it opens the network "EH3D-SETUP-xxxxxx" and shows a setup code on its USB console. The firmware can be replaced from the panel; a new image that does not come up is rolled
 * back by the boot loader.
 */
#include <cstdio>
#include <cstring>
extern "C" {
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
}
#include "central_uart.hpp"
#include "clock_sync.hpp"
#include "core/netplan.hpp"
#include "improv_serial.hpp"
#include "lamps.hpp"
#include "link_server.hpp"
#include "log_buffer.hpp"
#include "network.hpp"
#include "node_store.hpp"
#include "psram_mem.hpp"
#include "spindle.hpp"
#include "web_server.hpp"

namespace {
constexpr char kTag[] = "eh3d-node";

// While the node has no user, says every 15 seconds how to set it up. The code appears only here, on the USB console.
void setup_reminder_task(void*) {
  for (;;) {
    if (!eh3d::store::users_empty()) vTaskDelete(nullptr);
    const eh3d::network::Status n = eh3d::network::status();
    ESP_LOGW(kTag, "SETUP: this node has no user yet. Join the Wi-Fi \"%s\", open http://192.168.4.1/ and enter the setup code %s", n.ap_ssid.empty() ? "EH3D-SETUP-..." : n.ap_ssid.c_str(),
             eh3d::store::setup_code().c_str());
    vTaskDelay(pdMS_TO_TICKS(15000));
  }
}

// Holding the BOOT button for 8 seconds in the first 30 after power-up erases the settings and the users: the node is back in setup.
// (Skipped when the operator gave GPIO 0 to the machine.)
void boot_button_task(void*) {
  constexpr gpio_num_t kButton = GPIO_NUM_0;
  gpio_config_t io{};
  io.pin_bit_mask = 1ULL << kButton;
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io);
  int held_ms = 0;
  for (int elapsed_ms = 0; elapsed_ms < 30000; elapsed_ms += 100) {
    held_ms = gpio_get_level(kButton) == 0 ? held_ms + 100 : 0;
    if (held_ms >= 8000) {
      ESP_LOGW(kTag, "BOOT held for 8 s: erasing the settings and the users");
      eh3d::store::factory_reset();
      eh3d::network::disconnect_before_restart();
      esp_restart();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  vTaskDelete(nullptr);
}

// A freshly installed firmware is on probation: once the panel is up and has stayed up for a while, it is confirmed; if the panel could not start, the
// boot loader is told to go back to the previous image now.
void confirm_firmware_task(void* argument) {
  const bool panel_ok = argument != nullptr;
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    if (!panel_ok) {
      ESP_LOGE(kTag, "the new firmware could not start its panel: going back to the previous one");
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
    // A panel that is up is not enough: a node that cannot be reached (no address from the network, no access point of its own) would stay that way and need a cable to be rescued. The image
    // is confirmed once the node can be reached, and goes back to the previous one when five minutes pass without.
    bool reachable = false;
    for (int waited = 0; waited < 300 && !reachable; waited += 5) {
      const eh3d::network::Status link = eh3d::network::status();
      reachable = link.has_ip || link.ap_active;
      if (!reachable) vTaskDelay(pdMS_TO_TICKS(5000));
    }
    if (!reachable) {
      ESP_LOGE(kTag, "the new firmware could not be reached on any network: going back to the previous one");
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(kTag, "the new firmware is confirmed");
  }
  vTaskDelete(nullptr);
}
}  // namespace

extern "C" void app_main() {
  eh3d::logbuf::start();
  eh3d::memory::install_tls_allocator();   // TLS memory: the external RAM, and the internal one where a board has none
  eh3d::memory::start_monitor();           // the log says the state of the memory every minute
  eh3d::store::init();
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  const eh3d::config::Settings settings = eh3d::store::settings();
  const bool setup = eh3d::store::users_empty();
  ESP_LOGI(kTag, "EH3D-NODE-BUNGARD %s, firmware %s, MAC tail %s%s", settings.node_id.c_str(), esp_app_get_description()->version, eh3d::store::mac_tail().c_str(), setup ? ", NOT SET UP YET" : "");
  if (eh3d::store::settings_are_stored()) {
    const eh3d::config::Problems problems = eh3d::config::validate(settings);
    for (const eh3d::config::Problem& problem : problems) ESP_LOGE(kTag, "settings problem: %s (%s)", problem.path.c_str(), problem.code.c_str());
  }

  // The machine's outputs first: spindle and DTR are off from the first moment, whatever the network does. The pins and the rest are the settings' (machine.*).
  const eh3d::config::MachineSettings& machine = settings.machine;
  eh3d::lamps_start(machine.lamps);
  if (!eh3d::spindle_start(machine.spindle)) ESP_LOGE(kTag, "the spindle output did not start");
  if (!eh3d::uart_start(machine.uart, static_cast<std::size_t>(machine.link.queue_bytes))) ESP_LOGE(kTag, "the control unit line did not start");

  const eh3d::netplan::Plan plan = eh3d::netplan::plan_network(settings, setup, eh3d::store::setup_code(), eh3d::store::mac_tail(), eh3d::store::mac_sum());
  const bool network_ok = eh3d::network::start(settings, plan);
  if (network_ok && !plan.ap.enabled) {
    // Without an address after 90 s (no cable, no Wi-Fi it can join) the node would be unreachable: it opens the set-up access point again.
    eh3d::network::arm_rescue(eh3d::netplan::plan_network(settings, true, eh3d::store::setup_code(), eh3d::store::mac_tail(), eh3d::store::mac_sum()), 90);
  }
  if (!network_ok) ESP_LOGE(kTag, "the network could not be started: the node stays local");
  eh3d::clocksync::start(settings);
  eh3d::improv_serial::start();   // Improv Wi-Fi over the USB cable (ESP Web Tools)
  if (!eh3d::link_start(machine.link)) ESP_LOGE(kTag, "the control link did not start");
  const bool panel_ok = network_ok && eh3d::web::start(settings);

  // A periodic, unconditional restart the panel can turn on (0: never) - a board left running for weeks without anyone touching it. It waits while the machine is armed.
  if (settings.auto_restart_hours > 0) eh3d::web::restart_after(static_cast<unsigned>(settings.auto_restart_hours) * 3600u * 1000u);

  if (setup) xTaskCreate(setup_reminder_task, "setup-hint", 3072, nullptr, 2, nullptr);
  bool button_is_a_pin = false;
  for (const int pin : {machine.uart.tx, machine.uart.rx, machine.uart.dtr, machine.uart.dcd, machine.uart.ri, machine.uart.cts, machine.uart.dsr, machine.lamps.home_x, machine.lamps.home_y,
                        machine.lamps.home_z, machine.lamps.enable, machine.lamps.tool, machine.spindle.pwm_pin, machine.spindle.fwd_pin, machine.spindle.rev_pin}) {
    if (pin == 0) button_is_a_pin = true;
  }
#if CONFIG_EH3D_BOOT_FACTORY_RESET
  if (!button_is_a_pin) xTaskCreate(boot_button_task, "boot-button", 3072, nullptr, 2, nullptr);
#endif
  xTaskCreate(confirm_firmware_task, "confirm", 3072, panel_ok ? reinterpret_cast<void*>(1) : nullptr, 2, nullptr);
}
