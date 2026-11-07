// EH3D-NODE-BUNGARD - host tests of the self-check of the node: its codes against the page that lists them (docs/SELF_CHECK.md), the verdict of each situation (driven through
// a real control session on a stand-in for the hardware) and its settings, which are read, checked, written and kept in step like every other setting.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../core/bungard_config.hpp"
#include "../core/json.hpp"
#include "../core/self_check.hpp"
#include "../core/session.hpp"

using namespace eh3d;
using namespace eh3d::bungard;

static int fails = 0;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++fails; } } while (0)

static std::string source(const std::string& rel) {
  for (const char* up : {"", "../", "../../", "../../../"}) {
    std::ifstream in(std::string(up) + rel, std::ios::binary);
    if (!in) continue;
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
  }
  return std::string();
}

// The rows `| `CODE` | severity | ...` of docs/SELF_CHECK.md: code -> severity.
static std::map<std::string, std::string> doc_rows() {
  std::map<std::string, std::string> rows;
  std::istringstream in(source("docs/SELF_CHECK.md"));
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("| `BND-", 0) != 0) continue;
    const std::size_t b = line.find('`', 3);
    const std::size_t c = line.find('|', b);
    const std::size_t d = c == std::string::npos ? c : line.find('|', c + 1);
    if (b == std::string::npos || d == std::string::npos) continue;
    rows[line.substr(3, b - 3)] = line.substr(c + 2, d - c - 3);
  }
  return rows;
}

// A stand-in for the hardware: a queue that only counts, and the spindle as the session last set it.
struct Stand : Hardware {
  std::size_t capacity = 1000, held = 0;
  std::uint32_t rpm = 0;
  bool drain_possible = true;
  std::size_t queue_free() override { return capacity - held; }
  bool enqueue(const std::uint8_t*, std::size_t n) override { if (n > capacity - held) return false; held += n; return true; }
  void flush_queue() override { held = 0; }
  void send_drivers_off() override {}
  void set_dtr(bool) override {}
  void set_spindle(std::uint32_t r, bool) override { rpm = r; }
  bool spindle_event(std::uint32_t, bool) override { return true; }
  bool begin_drain() override { return drain_possible; }
  void abort_drain() override {}
  void reply(Type, const std::uint8_t*, std::size_t) override {}
};

static Frame frame(Type type, const std::string& payload) {
  Frame f;
  f.type = type;
  f.length = static_cast<std::uint16_t>(payload.size());
  for (std::size_t i = 0; i < payload.size(); ++i) f.payload[i] = static_cast<std::uint8_t>(payload[i]);
  return f;
}

static config::MachineSettings secure() {
  config::MachineSettings m;
  m.link.token = "abcdefgh12";
  return m;
}

static Snapshot up() {
  Snapshot s;
  s.network_up = true;
  s.queue_capacity = 1000;
  s.uptime_s = 100;
  return s;
}

int main() {
  // ---- the page and the table say the same ---------------------------------------------------------------------------------
  {
    const auto rows = doc_rows();
    CHECK(rows.size() == self_check_catalog().size());
    for (const Finding& f : self_check_catalog()) {
      CHECK(rows.count(f.code) == 1);
      if (rows.count(f.code) == 1) CHECK(rows.at(f.code) == severity_text(f.severity));
    }
    std::vector<std::string> seen;
    for (const Finding& f : self_check_catalog()) {
      for (const std::string& s : seen) CHECK(s != f.code);
      seen.push_back(f.code);
    }
  }

  // ---- situations -----------------------------------------------------------------------------------------------------------
  {
    const Report fresh = self_check(up(), config::MachineSettings{});  // never configured: no token, open control off
    CHECK(fresh.has("BND-S01") && !fresh.has("BND-S02") && std::string(fresh.overall()) == "warn" && fresh.codes() == "BND-S01");

    const Report ok = self_check(up(), secure());
    CHECK(std::string(ok.overall()) == "ok" && ok.codes().empty() && ok.findings.empty());

    Snapshot down = up();
    down.network_up = false;
    down.uptime_s = 5;  // inside the grace time
    CHECK(!self_check(down, secure()).has("BND-N01"));
    down.uptime_s = 31;
    CHECK(self_check(down, secure()).has("BND-N01") && std::string(self_check(down, secure()).overall()) == "warn");
    config::MachineSettings patient = secure();
    patient.selfcheck.grace_s = 600;
    CHECK(!self_check(down, patient).has("BND-N01"));

    config::MachineSettings open = secure();
    open.link.allow_open = true;
    open.link.token.clear();
    const Report r_open = self_check(up(), open);
    CHECK(r_open.has("BND-S02") && !r_open.has("BND-S01"));

    config::MachineSettings slow = secure();
    slow.link.idle_ms = 20000;
    CHECK(self_check(up(), slow).has("BND-S04"));
    slow.selfcheck.idle_ms_max = 30000;
    CHECK(!self_check(up(), slow).has("BND-S04"));

    config::MachineSettings nospindle = secure();
    nospindle.spindle.max_rpm = 0;
    CHECK(self_check(up(), nospindle).has("BND-P02") && std::string(self_check(up(), nospindle).overall()) == "ok");

    Snapshot fast = up();
    fast.spindle_rpm = 24001;
    CHECK(self_check(fast, secure()).has("BND-P01") && std::string(self_check(fast, secure()).overall()) == "fail");
    fast.spindle_rpm = 24000;
    CHECK(!self_check(fast, secure()).has("BND-P01"));

    config::MachineSettings off = secure();
    off.selfcheck.enabled = false;
    fast.spindle_rpm = 30000;
    CHECK(std::string(self_check(fast, off).overall()) == "off" && self_check(fast, off).codes().empty());
  }

  // ---- the queue: the limit is in whole percents, compared without rounding -------------------------------------------------------
  {
    Snapshot s = up();
    s.queue = 899;
    CHECK(!self_check(s, secure()).has("BND-Q01"));
    s.queue = 900;
    CHECK(self_check(s, secure()).has("BND-Q01"));
    config::MachineSettings m = secure();
    m.selfcheck.queue_percent = 50;
    s.queue = 500;
    CHECK(self_check(s, m).has("BND-Q01"));
    s.queue_capacity = 0;  // no capacity known: nothing to compare with
    CHECK(!self_check(s, m).has("BND-Q01"));
  }

  // ---- through a real session: the program arms, runs the spindle, fills the queue, goes away ---------------------------------------
  {
    config::MachineSettings m = secure();
    m.link.finish_on_loss = true;
    Settings ss;
    ss.token = m.link.token;
    ss.finish_on_loss = m.link.finish_on_loss;
    Stand hw;
    Info info;
    Session session(ss, info, hw);
    session.on_frame(frame(Type::Hello, ss.token), 0);
    session.on_frame(frame(Type::Arm, ""), 10);
    session.on_frame(frame(Type::Spindle, std::string("\xE0\x2E\x00", 3)), 20);  // 12000 rpm
    Snapshot s = up();
    s.armed = session.armed();
    s.spindle_rpm = session.spindle_rpm();
    CHECK(session.armed() && s.spindle_rpm == 12000);
    CHECK(self_check(s, m).codes() == "BND-S03");
    CHECK(std::string(self_check(s, m).overall()) == "ok");  // information only

    session.on_frame(frame(Type::Write, std::string(950, 'x')), 30);
    s.queue = static_cast<std::uint32_t>(hw.held);
    CHECK(s.queue == 950 && self_check(s, m).has("BND-Q01") && std::string(self_check(s, m).overall()) == "warn");

    session.on_disconnect();  // the program goes away with finish_on_loss on: the node finishes the job it holds
    s.armed = session.armed();
    s.draining = session.draining();
    CHECK(s.draining && self_check(s, m).has("BND-D01"));

    session.on_frame(frame(Type::Hello, ss.token), 40);
    session.on_frame(frame(Type::Abort, ""), 50);
    s.draining = session.draining();
    s.queue = static_cast<std::uint32_t>(hw.held);
    s.spindle_rpm = session.spindle_rpm();
    CHECK(!s.draining && s.queue == 0 && s.spindle_rpm == 0 && !self_check(s, m).has("BND-D01") && !self_check(s, m).has("BND-Q01"));
  }

  // ---- the settings: factory values, ranges, the trip through flash, and that they are live ---------------------------------------------
  {
    config::Settings base = config::default_settings("a1b2c3");
    base.sta.enabled = true;
    base.sta.ssid = "casa";
    base.sta.password = "una-clave-larga";
    CHECK(base.machine.selfcheck.enabled && base.machine.selfcheck.queue_percent == 90 && base.machine.selfcheck.idle_ms_max == 10000 && base.machine.selfcheck.grace_s == 30);

    config::Settings out;
    config::Problems problems;
    CHECK(config::load("{\"machine\":{\"selfcheck\":{\"enabled\":false,\"queue_percent\":75,\"idle_ms_max\":5000,\"grace_s\":0}}}", base, out, problems));
    CHECK(!out.machine.selfcheck.enabled && out.machine.selfcheck.queue_percent == 75 && out.machine.selfcheck.idle_ms_max == 5000 && out.machine.selfcheck.grace_s == 0);

    // the trip through flash and the document of the panel
    const std::string stored = config::to_json(out, true);
    config::Settings back;
    problems.clear();
    CHECK(config::load(stored, base, back, problems));
    CHECK(config::to_json(back, true) == stored);
    CHECK(stored.find("\"selfcheck\":{\"enabled\":false,\"queue_percent\":75,\"idle_ms_max\":5000,\"grace_s\":0}") != std::string::npos);
    CHECK(config::machine_to_json(out.machine, false).find("\"queue_percent\":75") != std::string::npos);

    // out of range: refused, with the path of the value, and nothing is applied
    for (const char* bad : {"{\"machine\":{\"selfcheck\":{\"queue_percent\":49}}}", "{\"machine\":{\"selfcheck\":{\"queue_percent\":101}}}", "{\"machine\":{\"selfcheck\":{\"idle_ms_max\":199}}}",
                            "{\"machine\":{\"selfcheck\":{\"grace_s\":601}}}", "{\"machine\":{\"selfcheck\":{\"enabled\":3}}}", "{\"machine\":{\"selfcheck\":5}}"}) {
      problems.clear();
      config::Settings refused;
      CHECK(!config::load(bad, base, refused, problems));
      CHECK(!problems.empty() && problems[0].path.rfind("machine.selfcheck", 0) == 0);
    }

    // the self-check is judged at every status request: changing it needs no restart
    config::Settings a = out, b = out;
    b.machine.selfcheck.queue_percent = 60;
    b.machine.selfcheck.enabled = true;
    config::clear_live_settings(a);
    config::clear_live_settings(b);
    CHECK(config::to_json(a, true) == config::to_json(b, true));
  }

  std::printf("%d checks, %d failed\n", checks, fails);
  return fails == 0 ? 0 : 1;
}
