// EH3D-NODE-BUNGARD - the settings of a Bungard node: what the web panel edits, what is stored in flash and what the firmware obeys.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// One document (JSON) holds everything that differs from node to node, so the same firmware image serves every node and nothing secret has to be compiled in. This file only reads,
// checks and writes that document; it touches no hardware, so all of it is tested on a computer. Passwords and the access token of the control link are never written back to the panel:
// a section sent without one (or with an empty one) keeps the stored one, "<name>_clear": true erases it, and "password_set" / "token_set" tell the panel that there is one.
//
// The `machine` section is what used to be fixed when the image was built: the control link (port, access token, idle time, queue), the serial line to the control unit and its
// handshake lines, the lamp inputs and the spindle outputs. The factory values below are the values the first prototype was built with.
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "board_s3.hpp"
#include "config_common.hpp"
#include "json.hpp"
#include "node_settings.hpp"
#include "stored_load.hpp"

namespace eh3d::config {

// The control link: the TCP port the program on the network connects to, who may drive the machine and how long an armed session may stay silent.
struct LinkSetting {
  int port = 5800;
  std::string token;          // write-only: empty means the node refuses to be controlled (unless allow_open)
  bool allow_open = false;    // accept control with no token (a bench setting)
  int idle_ms = 3000;         // an armed session that sends nothing for this long is stopped
  int queue_bytes = 262144;   // what the node can hold for the control unit (kept in the external RAM: a job of a minute or two)
  bool finish_on_loss = false;   // when the program goes away, finish the job already in the queue instead of stopping at once
  int max_drain_s = 600;      // the most seconds it may go on alone
};

// The serial line to the control unit (38400 8N1) and its handshake lines. -1: not wired (only the transmit pin is required).
struct UartSetting {
  int tx = 17;
  int rx = 18;
  int dtr = 15;
  int dcd = 16;
  int ri = 21;
  int cts = -1;
  int dsr = -1;
  bool rs232_inverted = true;   // the RS-232 transceiver inverts the control lines (a MAX3232 does)
};

// The five lamps of the control unit, read through a buffer transistor.
struct LampsSetting {
  int home_x = 1;
  int home_y = 2;
  int home_z = 7;
  int enable = 8;
  int tool = 38;
  bool active_low = true;   // a lit lamp pulls the pin low
};

// The spindle outputs: a speed PWM for a 0-10 V converter and the run / direction lines.
struct SpindleSetting {
  int max_rpm = 24000;          // the speed at full signal; 0: no spindle output
  int pwm_pin = 4;
  int pwm_hz = 2000;
  bool pwm_inverted = false;
  int fwd_pin = 5;              // -1: not wired
  int rev_pin = 6;              // -1: not wired
  bool dir_single = false;      // one run line (fwd) and one direction line (rev) instead of two run lines
  bool outputs_active_low = false;
  int reverse_delay_ms = 3000;  // how long the spindle must have been stopped before it may turn the other way
};

struct MachineSettings {
  LinkSetting link;
  UartSetting uart;
  LampsSetting lamps;
  SpindleSetting spindle;
};

struct Settings : NodeSettings {   // what every node has (core/node_settings.hpp) and the machine's own
  MachineSettings machine;
};

// ---- the defaults of a node that has never been configured ---------------------------------------------------------------------

// `mac_tail` is the last three bytes of the node's MAC as six lowercase hexadecimal digits: it makes the first identity unique.
inline Settings default_settings(std::string_view mac_tail) {
  Settings s;
  node_defaults(s, mac_tail);
  return s;
}

// ---- reading -------------------------------------------------------------------------------------------------------------------

// Applies the members of a `machine` object on top of `m`; whatever the object omits stays as it was. `prefix` is the path the problems are reported under ("machine." in the whole document, "" for the
// object on its own).
inline void machine_read(const json::Value& machine, MachineSettings& m, Problems& problems, const std::string& prefix = "machine.") {
  using namespace detail;
  if (!machine.is_object()) { bad(problems, prefix.empty() ? "" : prefix.substr(0, prefix.size() - 1), "invalid"); return; }
  const int last = board::kLastGpio;
  if (const json::Value* link = machine.get("link"); link != nullptr && link->is_object()) {
    const std::string p = prefix + "link.";
    read_int(*link, "port", m.link.port, 1, 65535, p + "port", problems);
    read_secret(*link, "token", m.link.token, p + "token", problems);
    read_bool(*link, "allow_open", m.link.allow_open, p + "allow_open", problems);
    read_int(*link, "idle_ms", m.link.idle_ms, 1, 3600000, p + "idle_ms", problems);
    read_int(*link, "queue_bytes", m.link.queue_bytes, 1, 4 * 1024 * 1024, p + "queue_bytes", problems);
    read_bool(*link, "finish_on_loss", m.link.finish_on_loss, p + "finish_on_loss", problems);
    read_int(*link, "max_drain_s", m.link.max_drain_s, 1, 86400, p + "max_drain_s", problems);
  } else if (machine.get("link") != nullptr) bad(problems, prefix + "link", "invalid");
  if (const json::Value* uart = machine.get("uart"); uart != nullptr && uart->is_object()) {
    const std::string p = prefix + "uart.";
    read_int(*uart, "tx", m.uart.tx, -1, last, p + "tx", problems);
    read_int(*uart, "rx", m.uart.rx, -1, last, p + "rx", problems);
    read_int(*uart, "dtr", m.uart.dtr, -1, last, p + "dtr", problems);
    read_int(*uart, "dcd", m.uart.dcd, -1, last, p + "dcd", problems);
    read_int(*uart, "ri", m.uart.ri, -1, last, p + "ri", problems);
    read_int(*uart, "cts", m.uart.cts, -1, last, p + "cts", problems);
    read_int(*uart, "dsr", m.uart.dsr, -1, last, p + "dsr", problems);
    read_bool(*uart, "rs232_inverted", m.uart.rs232_inverted, p + "rs232_inverted", problems);
  } else if (machine.get("uart") != nullptr) bad(problems, prefix + "uart", "invalid");
  if (const json::Value* lamps = machine.get("lamps"); lamps != nullptr && lamps->is_object()) {
    const std::string p = prefix + "lamps.";
    read_int(*lamps, "home_x", m.lamps.home_x, -1, last, p + "home_x", problems);
    read_int(*lamps, "home_y", m.lamps.home_y, -1, last, p + "home_y", problems);
    read_int(*lamps, "home_z", m.lamps.home_z, -1, last, p + "home_z", problems);
    read_int(*lamps, "enable", m.lamps.enable, -1, last, p + "enable", problems);
    read_int(*lamps, "tool", m.lamps.tool, -1, last, p + "tool", problems);
    read_bool(*lamps, "active_low", m.lamps.active_low, p + "active_low", problems);
  } else if (machine.get("lamps") != nullptr) bad(problems, prefix + "lamps", "invalid");
  if (const json::Value* spindle = machine.get("spindle"); spindle != nullptr && spindle->is_object()) {
    const std::string p = prefix + "spindle.";
    read_int(*spindle, "max_rpm", m.spindle.max_rpm, 0, 65535, p + "max_rpm", problems);
    read_int(*spindle, "pwm_pin", m.spindle.pwm_pin, -1, last, p + "pwm_pin", problems);
    read_int(*spindle, "pwm_hz", m.spindle.pwm_hz, 1, 1000000, p + "pwm_hz", problems);
    read_bool(*spindle, "pwm_inverted", m.spindle.pwm_inverted, p + "pwm_inverted", problems);
    read_int(*spindle, "fwd_pin", m.spindle.fwd_pin, -1, last, p + "fwd_pin", problems);
    read_int(*spindle, "rev_pin", m.spindle.rev_pin, -1, last, p + "rev_pin", problems);
    read_bool(*spindle, "dir_single", m.spindle.dir_single, p + "dir_single", problems);
    read_bool(*spindle, "outputs_active_low", m.spindle.outputs_active_low, p + "outputs_active_low", problems);
    read_int(*spindle, "reverse_delay_ms", m.spindle.reverse_delay_ms, 0, 3600000, p + "reverse_delay_ms", problems);
  } else if (machine.get("spindle") != nullptr) bad(problems, prefix + "spindle", "invalid");
}

// Applies the members present in `document` on top of `s`; whatever the document omits stays as it was. The result is checked with validate(): read_settings() only reports a member of the
// wrong type or size.
inline void read_settings(const json::Value& document, Settings& s, Problems& problems) {
  if (!document.is_object()) { detail::bad(problems, "", "invalid"); return; }
  node_read(document, s, problems);
  if (const json::Value* machine = document.get("machine"); machine != nullptr) machine_read(*machine, s.machine, problems);
}

// ---- checking ------------------------------------------------------------------------------------------------------------------

// What a link token may be: 8 to 64 printable characters (it travels in the first frame of the link).
inline bool token_is_valid(std::string_view token) {
  if (token.size() < 8 || token.size() > kMaxPasswordText) return false;
  for (const char c : token) if (c < 0x20 || c > 0x7E) return false;
  return true;
}

inline void machine_validate(const MachineSettings& m, Problems& problems, const std::string& prefix = "machine.") {
  using detail::bad;
  // the link
  if (m.link.port < 1024 || m.link.port > 65535) bad(problems, prefix + "link.port", "range");
  if (!m.link.token.empty() && !token_is_valid(m.link.token)) bad(problems, prefix + "link.token", m.link.token.size() < 8 ? "too_short" : "invalid");
  if (m.link.idle_ms < 200 || m.link.idle_ms > 60000) bad(problems, prefix + "link.idle_ms", "range");
  if (m.link.queue_bytes < 1024 || m.link.queue_bytes > 4194304) bad(problems, prefix + "link.queue_bytes", "range");
  if (m.link.max_drain_s < 10 || m.link.max_drain_s > 3600) bad(problems, prefix + "link.max_drain_s", "range");

  // the pins: reserved ones (flash, PSRAM, USB, the Ethernet chip) are refused, the boot ones are allowed, and no pin may do two jobs
  detail::PinClaims claims;
  const auto claim = [&](int gpio, const char* who, const std::string& path) {
    if (gpio < 0) return;
    if (!board::assignable(gpio)) { bad(problems, path, "reserved"); return; }
    if (!claims.claim(gpio, who)) bad(problems, path, "conflict");
  };
  if (m.uart.tx < 0) bad(problems, prefix + "uart.tx", "required"); else claim(m.uart.tx, "uart.tx", prefix + "uart.tx");
  claim(m.uart.rx, "uart.rx", prefix + "uart.rx");   // -1: nothing is wired to receive (the control unit sends nothing on the data line)
  claim(m.uart.dtr, "uart.dtr", prefix + "uart.dtr");
  claim(m.uart.dcd, "uart.dcd", prefix + "uart.dcd");
  claim(m.uart.ri, "uart.ri", prefix + "uart.ri");
  claim(m.uart.cts, "uart.cts", prefix + "uart.cts");
  claim(m.uart.dsr, "uart.dsr", prefix + "uart.dsr");
  claim(m.lamps.home_x, "lamps.home_x", prefix + "lamps.home_x");
  claim(m.lamps.home_y, "lamps.home_y", prefix + "lamps.home_y");
  claim(m.lamps.home_z, "lamps.home_z", prefix + "lamps.home_z");
  claim(m.lamps.enable, "lamps.enable", prefix + "lamps.enable");
  claim(m.lamps.tool, "lamps.tool", prefix + "lamps.tool");
  // the spindle: with no top speed there is no spindle output and its pins are free
  if (m.spindle.max_rpm > 0) {
    if (m.spindle.pwm_pin < 0) bad(problems, prefix + "spindle.pwm_pin", "required"); else claim(m.spindle.pwm_pin, "spindle.pwm_pin", prefix + "spindle.pwm_pin");
    claim(m.spindle.fwd_pin, "spindle.fwd_pin", prefix + "spindle.fwd_pin");
    claim(m.spindle.rev_pin, "spindle.rev_pin", prefix + "spindle.rev_pin");
    if (m.spindle.pwm_hz < 100 || m.spindle.pwm_hz > 40000) bad(problems, prefix + "spindle.pwm_hz", "range");
  }
  if (m.spindle.reverse_delay_ms < 0 || m.spindle.reverse_delay_ms > 60000) bad(problems, prefix + "spindle.reverse_delay_ms", "range");
}

inline Problems validate(const Settings& s) {
  Problems problems;
  node_validate(s, problems);
  machine_validate(s.machine, problems);
  return problems;
}

// ---- writing -------------------------------------------------------------------------------------------------------------------

// The members of the `machine` object (without the braces' owner: the caller opens the object). `secrets`: true writes the token (for flash storage); false replaces it with a "token_set" flag.
inline void machine_write(json::Writer& w, const MachineSettings& m, bool secrets) {
  w.key("link").begin_object().field("port", m.link.port);
  if (secrets) w.field("token", m.link.token); else w.field("token_set", !m.link.token.empty());
  w.field("allow_open", m.link.allow_open).field("idle_ms", m.link.idle_ms).field("queue_bytes", m.link.queue_bytes)
      .field("finish_on_loss", m.link.finish_on_loss).field("max_drain_s", m.link.max_drain_s).end_object();
  w.key("uart").begin_object().field("tx", m.uart.tx).field("rx", m.uart.rx).field("dtr", m.uart.dtr).field("dcd", m.uart.dcd).field("ri", m.uart.ri)
      .field("cts", m.uart.cts).field("dsr", m.uart.dsr).field("rs232_inverted", m.uart.rs232_inverted).end_object();
  w.key("lamps").begin_object().field("home_x", m.lamps.home_x).field("home_y", m.lamps.home_y).field("home_z", m.lamps.home_z).field("enable", m.lamps.enable)
      .field("tool", m.lamps.tool).field("active_low", m.lamps.active_low).end_object();
  w.key("spindle").begin_object().field("max_rpm", m.spindle.max_rpm).field("pwm_pin", m.spindle.pwm_pin).field("pwm_hz", m.spindle.pwm_hz).field("pwm_inverted", m.spindle.pwm_inverted)
      .field("fwd_pin", m.spindle.fwd_pin).field("rev_pin", m.spindle.rev_pin).field("dir_single", m.spindle.dir_single).field("outputs_active_low", m.spindle.outputs_active_low)
      .field("reverse_delay_ms", m.spindle.reverse_delay_ms).end_object();
}

// The `machine` object on its own, as GET /api/v1/machine answers it (never with the token) or as flash keeps it (with it).
inline std::string machine_to_json(const MachineSettings& m, bool secrets) {
  json::Writer w;
  w.begin_object();
  machine_write(w, m, secrets);
  w.end_object();
  return w.str();
}

// `secrets`: true writes the passwords and the token (for flash storage); false replaces them with "password_set" / "token_set" flags (for the panel).
inline std::string to_json(const Settings& s, bool secrets) {
  json::Writer w;
  w.begin_object();
  node_write_head(w, s, secrets);
  w.key("machine").begin_object();
  machine_write(w, s.machine, secrets);
  w.end_object();
  node_write_tail(w, s);
  w.end_object();
  return w.str();
}

// Reads a stored or received document on top of `base` and checks the result. Nothing is applied unless `problems` stays empty.
inline bool load(std::string_view text, const Settings& base, Settings& out, Problems& problems) {
  json::Value document;
  if (!json::parse(text, document)) { problems.push_back({"", "not_json"}); return false; }
  out = base;
  read_settings(document, out, problems);
  if (problems.empty()) problems = validate(out);
  return problems.empty();
}

// ---- what a change needs ---------------------------------------------------------------------------------------------------------------

// Blanks the settings that are applied as soon as they are saved, so that two sets of settings that differ only in those need no restart: the display name, the language, the time zone
// and what the control link reads at every connection (the token, open control, the idle time and the settling time between spindle directions). Everything else - the port, the pins, the
// queue, the speed scale - is taken when the node starts.
inline void clear_live_settings(Settings& s) {
  s.node_name.clear();
  s.language = "en";
  s.time.zone.clear();
  s.machine.link.token.clear();
  s.machine.link.allow_open = false;
  s.machine.link.idle_ms = 0;
  s.machine.link.finish_on_loss = false;
  s.machine.link.max_drain_s = 0;
  s.machine.spindle.reverse_delay_ms = 0;
}

}  // namespace eh3d::config
