// EH3D-NODE-BUNGARD - the TCP link to the program that drives the machine. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// One client at a time. Frames are decoded by core/frame.hpp and the rules of core/session.hpp decide what happens to
// them; this file only moves bytes between the socket, the session and the UART.
#include "link_server.hpp"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

extern "C" {
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
}

#include "central_uart.hpp"
#include "core/frame.hpp"
#include "core/drain.hpp"
#include "core/session.hpp"
#include "lamps.hpp"
#include "node_guard.hpp"
#include "node_store.hpp"
#include "spindle.hpp"

namespace eh3d {
namespace {
constexpr char kTag[] = "eh3d-link";
std::atomic<bool> g_client{false}, g_auth{false}, g_armed{false}, g_reverse{false};
std::atomic<std::uint32_t> g_rpm{0};
std::atomic<std::uint8_t> g_lamps{0};
std::atomic<bool> g_draining{false};       // the node is finishing the job it holds (the program went away)
std::atomic<bool> g_stop_request{false};   // the panel's stop button: the session in progress stops at its next turn
// What the link was started with (machine.link): the port and the size of the queue are taken when the node starts. The token, open control, the idle time and the spindle's settling time
// between directions are read from the settings at every connection, so changing them in the panel needs no restart.
int g_port = 5800;
bool g_finish_on_loss = false;
std::uint32_t g_max_drain_ms = 600000;

// Finishing the job the node holds when the program goes away (core/drain.hpp), over the UART queue and the spindle.
class NodeDrainHardware final : public bungard::DrainHardware {
 public:
  bool busy() override { return eh3d::queue_busy(); }
  void stop_all() override {
    eh3d::flush_queue();
    eh3d::send_drivers_off();
    eh3d::spindle_set(0, false);
  }
};
NodeDrainHardware g_drain_hardware;
bungard::Drain g_drain;
std::uint32_t g_queue_bytes = 32768;

std::uint32_t now_ms() { return static_cast<std::uint32_t>(esp_timer_get_time() / 1000); }

bool send_all(int fd, const std::uint8_t* data, std::size_t n) {
  while (n > 0) {
    const int sent = ::send(fd, data, n, 0);
    if (sent < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) { vTaskDelay(1); continue; }
      return false;
    }
    data += sent;
    n -= static_cast<std::size_t>(sent);
  }
  return true;
}

// The session's view of the machine: the UART queue, the DTR pin, and the replies that go back down the socket.
class LinkHardware final : public bungard::Hardware {
 public:
  explicit LinkHardware(int fd) : fd_(fd) {}
  bool ok() const { return ok_; }
  std::size_t queue_free() override { return eh3d::queue_free(); }
  bool enqueue(const std::uint8_t* d, std::size_t n) override { return eh3d::enqueue(d, n); }
  void flush_queue() override { eh3d::flush_queue(); }
  void send_drivers_off() override { eh3d::send_drivers_off(); }
  void set_dtr(bool on) override { eh3d::set_dtr(on); }
  void set_spindle(std::uint32_t rpm, bool reverse) override { eh3d::spindle_set(rpm, reverse); }
  bool spindle_event(std::uint32_t rpm, bool reverse) override { return eh3d::enqueue_spindle_event(rpm, reverse); }
  bool begin_drain() override {
    g_max_drain_ms = g_max_drain_ms == 0 ? 1 : g_max_drain_ms;
    const bool started = g_drain.begin(now_ms(), g_drain_hardware);
    g_draining = g_drain.active();
    return started;
  }
  void abort_drain() override {
    g_drain.abort(g_drain_hardware);
    g_draining = false;
  }
  void reply(bungard::Type type, const std::uint8_t* payload, std::size_t n) override {
    std::uint8_t out[bungard::kMaxFrameBytes];
    const std::size_t len = bungard::encode(type, payload, n, out, sizeof out);
    if (len == 0 || !send_all(fd_, out, len)) ok_ = false;
  }

 private:
  int fd_;
  bool ok_ = true;
};

void put_u32(std::uint8_t* p, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

bungard::Info make_info() {
  bungard::Info info;
  unsigned a = 0, b = 0, c = 0;
  std::sscanf(esp_app_get_description()->version, "%u.%u.%u", &a, &b, &c);
  info.major = static_cast<std::uint8_t>(a);
  info.minor = static_cast<std::uint8_t>(b);
  info.patch = static_cast<std::uint8_t>(c);
  info.baud = 38400;
  info.queue_capacity = g_queue_bytes;
  return info;
}

void serve(int fd) {
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
  timeval tv{0, 5 * 1000};  // 5 ms: the loop reads the lamps, ticks the session and reports the status
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

  LinkHardware hw(fd);
  bungard::Settings settings;
  {
    // a copy of the settings on the heap: this task has a small stack
    const std::unique_ptr<config::Settings> stored(new (std::nothrow) config::Settings(store::settings()));
    if (stored) {
      settings.token = stored->machine.link.token;
      settings.allow_open = stored->machine.link.allow_open;
      settings.idle_ms = static_cast<std::uint32_t>(stored->machine.link.idle_ms);
      settings.reverse_delay_ms = static_cast<std::uint32_t>(stored->machine.spindle.reverse_delay_ms);
      settings.finish_on_loss = stored->machine.link.finish_on_loss;
      g_max_drain_ms = static_cast<std::uint32_t>(stored->machine.link.max_drain_s) * 1000u;
      g_drain.configure(g_max_drain_ms);
    }   // with no memory for the copy the session starts with no token and refuses control
  }
  settings.max_rpm = spindle_max_rpm();   // the output was set up for this top speed when the node started
  settings.draining = g_drain.active();    // a node that is still finishing a job of an earlier connection takes nothing new
  bungard::Session session(settings, make_info(), hw);
  bungard::Decoder decoder;
  static bungard::Frame frame;  // 2 KB: not on the stack
  std::uint8_t rx[256];
  std::uint32_t last_status = now_ms();

  g_client = true;
  while (hw.ok()) {
    const int n = ::recv(fd, rx, sizeof rx, 0);
    if (n == 0) break;  // closed by the program
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != ETIMEDOUT) break;
    for (int i = 0; i < n; ++i) {
      if (decoder.feed(rx[i], frame)) {
        do { session.on_frame(frame, now_ms()); } while (decoder.poll(frame));
      }
    }
    if (g_stop_request.exchange(false)) session.emergency_stop();
    session.on_tick(now_ms());
    g_drain.tick(now_ms(), g_drain_hardware);
    g_draining = g_drain.active();
    if (!g_draining && session.draining()) session.emergency_stop();   // the drain ended by itself: the session leaves it too
    const std::uint8_t lamps = eh3d::read_lamps();
    session.on_inputs(lamps);
    g_lamps = lamps;
    g_auth = session.authenticated();
    g_armed = session.armed();
    g_rpm = spindle_rpm_now();
    g_reverse = spindle_reverse_now();

    if (session.authenticated()) {
      std::uint8_t from_unit[128];
      const std::size_t got = eh3d::read_rx(from_unit, sizeof from_unit);
      if (got > 0) hw.reply(bungard::Type::Rx, from_unit, got);
      if (static_cast<std::uint32_t>(now_ms() - last_status) >= 200) {
        last_status = now_ms();
        std::uint8_t s[15];
        put_u32(s, static_cast<std::uint32_t>(eh3d::queue_size()));
        put_u32(s + 4, eh3d::bytes_sent());
        s[8] = session.armed() ? 1 : 0;
        s[9] = eh3d::input_lines();
        s[10] = static_cast<std::uint8_t>(g_rpm.load() & 0xFF);
        s[11] = static_cast<std::uint8_t>(g_rpm.load() >> 8);
        s[12] = g_reverse.load() ? 1 : 0;
        s[13] = g_lamps;
        s[14] = g_draining ? 1 : 0;
        hw.reply(bungard::Type::Status, s, sizeof s);
      }
    }
  }
  session.on_disconnect();  // an armed session that drops is stopped (queue emptied, drivers off) or, if the setting says so, the node finishes the job it holds
  g_draining = g_drain.active();
  g_client = false;
  g_auth = false;
  g_armed = false;
  g_rpm = 0;
  g_reverse = false;
  g_lamps = 0;
}

void server_task(void*) {
  const int listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  int one = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<std::uint16_t>(g_port));
  if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || ::listen(listener, 1) != 0) {
    ESP_LOGE(kTag, "cannot listen on port %d", g_port);
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(kTag, "control link listening on port %d", g_port);
  timeval accept_wait{0, 100 * 1000};   // the accept gives up every 100 ms so that a node finishing a job is looked after with nobody connected
  setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &accept_wait, sizeof accept_wait);
  for (;;) {
    sockaddr_in peer{};
    socklen_t len = sizeof peer;
    const int fd = ::accept(listener, reinterpret_cast<sockaddr*>(&peer), &len);
    if (g_stop_request.exchange(false)) g_drain.abort(g_drain_hardware);
    g_drain.tick(now_ms(), g_drain_hardware);
    g_draining = g_drain.active();
    if (fd < 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
    ESP_LOGI(kTag, "program connected");
    serve(fd);
    ::close(fd);
    ESP_LOGI(kTag, "program disconnected");
  }
}
}  // namespace

bool link_start(const config::LinkSetting& setting) {
  g_port = setting.port;
  g_queue_bytes = static_cast<std::uint32_t>(setting.queue_bytes);
  g_finish_on_loss = setting.finish_on_loss;
  g_drain.configure(static_cast<std::uint32_t>(setting.max_drain_s) * 1000u);
  if (setting.token.empty() && !setting.allow_open) ESP_LOGW(kTag, "no access token is set: the node will refuse to be controlled (set the link token in the panel)");
  nodeguard::expect_task("eh3d-link");
  return xTaskCreate(server_task, "eh3d-link", 6144, nullptr, 5, nullptr) == pdPASS;
}

// The panel's stop button: whatever the machine is doing stops now, with or without a program connected.
void link_emergency_stop() { g_stop_request = true; }

LinkState link_state() {
  LinkState s;
  s.client = g_client;
  s.authenticated = g_auth;
  s.armed = g_armed;
  s.spindle_rpm = g_rpm;
  s.spindle_reverse = g_reverse;
  s.lamps = g_lamps;
  s.draining = g_draining;
  return s;
}

}  // namespace eh3d
