// EH3D-NODE-BUNGARD - the serial line to the control unit of the machine. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The control unit is a stepper driver with no brain: it takes one byte per step, and the PC makes the timing. Here the
// timing is the line's own: the program on the network sends the bytes already spaced (a step byte followed by the filler
// bytes that make up the pause), and this file only keeps the UART fed. The UART's transmit FIFO is never allowed to run
// dry between two chunks of a move, and a stop empties the queue ahead of it.
#include "central_uart.hpp"

#include <algorithm>
#include <atomic>

extern "C" {
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
}

#include "core/byte_queue.hpp"
#include "core/bungard_config.hpp"
#include "node_guard.hpp"
#include "spindle.hpp"

namespace eh3d {
namespace {
constexpr char kTag[] = "eh3d-uart";
constexpr uart_port_t kPort = UART_NUM_1;
constexpr int kBaud = 38400;
constexpr std::size_t kChunk = 32;  // bytes taken from the queue at a time: the most that still goes out after a stop

eh3d::bungard::ByteQueue* g_queue = nullptr;
SemaphoreHandle_t g_lock = nullptr;
std::atomic<std::uint32_t> g_sent{0};

// Spindle commands that wait in the queue. `at` is how many bytes had been queued before the command (counted from the last time the queue was
// empty and idle), so the writer applies it just before it sends the byte with that number.
struct Event { std::uint32_t at; std::uint32_t rpm; bool reverse; };
constexpr std::size_t kMaxEvents = 64;
Event g_events[kMaxEvents];
std::size_t g_event_count = 0;
std::uint32_t g_enq_total = 0;   // bytes queued
std::uint32_t g_pop_total = 0;   // bytes taken out by the writer

// The pins and the polarity come from the settings (machine.uart) when the line starts.
config::UartSetting g_pins;

bool level_for(bool asserted) { return g_pins.rs232_inverted ? !asserted : asserted; }

void configure_input(int pin) {
  if (pin < 0) return;
  gpio_config_t cfg{};
  cfg.pin_bit_mask = 1ULL << pin;
  cfg.mode = GPIO_MODE_INPUT;
  cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&cfg);
}

bool read_input(int pin) { return pin >= 0 && (gpio_get_level(static_cast<gpio_num_t>(pin)) != 0) == level_for(true); }

void writer_task(void*) {
  std::uint8_t buf[kChunk];
  for (;;) {
    std::size_t n = 0;
    Event due[kMaxEvents];
    std::size_t due_count = 0;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    // the spindle commands whose turn has come go before the next byte
    while (g_event_count > 0 && g_events[0].at <= g_pop_total) {
      due[due_count++] = g_events[0];
      for (std::size_t i = 1; i < g_event_count; ++i) g_events[i - 1] = g_events[i];
      --g_event_count;
    }
    // a chunk never crosses the next command
    std::size_t limit = kChunk;
    if (g_event_count > 0) limit = std::min<std::size_t>(limit, g_events[0].at - g_pop_total);
    n = g_queue->pop(buf, limit);
    g_pop_total += static_cast<std::uint32_t>(n);
    // idle: start counting from zero again, so the counters never wrap
    if (n == 0 && g_event_count == 0 && g_queue->size() == 0) { g_enq_total = 0; g_pop_total = 0; }
    xSemaphoreGive(g_lock);
    for (std::size_t i = 0; i < due_count; ++i) spindle_set(due[i].rpm, due[i].reverse);
    if (n > 0) {
      uart_write_bytes(kPort, reinterpret_cast<const char*>(buf), n);
      g_sent.fetch_add(static_cast<std::uint32_t>(n));
    } else {
      vTaskDelay(1);
    }
  }
}
}  // namespace

bool uart_start(const config::UartSetting& pins, std::size_t queue_bytes) {
  g_pins = pins;
  g_queue = new eh3d::bungard::ByteQueue(queue_bytes);
  g_lock = xSemaphoreCreateMutex();
  if (g_lock == nullptr) return false;

  uart_config_t cfg{};
  cfg.baud_rate = kBaud;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  // No transmit ring: uart_write_bytes() returns when the bytes are in the FIFO, so the queue above stays the only buffer
  // and a stop really empties everything that is waiting.
  if (uart_driver_install(kPort, 512, 0, 0, nullptr, 0) != ESP_OK) { ESP_LOGE(kTag, "UART driver install failed"); return false; }
  if (uart_param_config(kPort, &cfg) != ESP_OK) { ESP_LOGE(kTag, "UART configuration failed"); return false; }
  if (uart_set_pin(kPort, g_pins.tx, g_pins.rx >= 0 ? g_pins.rx : UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
    ESP_LOGE(kTag, "UART pins failed");
    return false;
  }
  if (g_pins.dtr >= 0) {
    gpio_config_t out{};
    out.pin_bit_mask = 1ULL << g_pins.dtr;
    out.mode = GPIO_MODE_OUTPUT;
    gpio_config(&out);
    set_dtr(false);
  }
  configure_input(g_pins.cts);
  configure_input(g_pins.dsr);
  configure_input(g_pins.ri);
  configure_input(g_pins.dcd);
  nodeguard::expect_task("eh3d-uart-tx");
  // The writer outranks the network tasks: a late chunk is a pause in the middle of a move.
  if (xTaskCreate(writer_task, "eh3d-uart-tx", 3072, nullptr, configMAX_PRIORITIES - 3, nullptr) != pdPASS) return false;
  ESP_LOGI(kTag, "control unit line ready: %d baud, TX %d, RX %d, DTR %d, queue %u bytes", kBaud, g_pins.tx, g_pins.rx, g_pins.dtr, static_cast<unsigned>(queue_bytes));
  return true;
}

std::size_t queue_free() {
  if (g_lock == nullptr) return 0;
  xSemaphoreTake(g_lock, portMAX_DELAY);
  const std::size_t n = g_queue->free();
  xSemaphoreGive(g_lock);
  return n;
}

std::size_t queue_size() {
  if (g_lock == nullptr) return 0;
  xSemaphoreTake(g_lock, portMAX_DELAY);
  const std::size_t n = g_queue->size();
  xSemaphoreGive(g_lock);
  return n;
}

bool enqueue(const std::uint8_t* data, std::size_t n) {
  xSemaphoreTake(g_lock, portMAX_DELAY);
  const bool ok = g_queue->push(data, n);
  if (ok) g_enq_total += static_cast<std::uint32_t>(n);
  xSemaphoreGive(g_lock);
  return ok;
}

bool enqueue_spindle_event(std::uint32_t rpm, bool reverse) {
  xSemaphoreTake(g_lock, portMAX_DELAY);
  const bool ok = g_event_count < kMaxEvents;
  if (ok) g_events[g_event_count++] = {g_enq_total, rpm, reverse};
  xSemaphoreGive(g_lock);
  return ok;
}

bool queue_busy() {
  if (g_lock == nullptr) return false;
  xSemaphoreTake(g_lock, portMAX_DELAY);
  const bool busy = g_queue->size() > 0 || g_event_count > 0;
  xSemaphoreGive(g_lock);
  return busy;
}

void flush_queue() {
  xSemaphoreTake(g_lock, portMAX_DELAY);
  g_queue->clear();
  g_event_count = 0;
  g_pop_total = g_enq_total;
  xSemaphoreGive(g_lock);
}

void send_drivers_off() {
  flush_queue();
  const std::uint8_t off = 0xC0;
  uart_write_bytes(kPort, reinterpret_cast<const char*>(&off), 1);
}

void set_dtr(bool on) {
  if (g_pins.dtr < 0) return;
  gpio_set_level(static_cast<gpio_num_t>(g_pins.dtr), level_for(on) ? 1 : 0);
}

std::uint8_t input_lines() {
  std::uint8_t v = 0;
  if (read_input(g_pins.cts)) v |= 1;
  if (read_input(g_pins.dsr)) v |= 2;
  if (read_input(g_pins.ri)) v |= 4;
  if (read_input(g_pins.dcd)) v |= 8;
  return v;
}

std::uint32_t bytes_sent() { return g_sent.load(); }

std::size_t read_rx(std::uint8_t* out, std::size_t max) {
  const int n = uart_read_bytes(kPort, out, max, 0);
  return n > 0 ? static_cast<std::size_t>(n) : 0;
}

}  // namespace eh3d
