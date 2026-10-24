// EH3D-NODE-BUNGARD - host tests of the machine settings: the defaults, the checks on every field and on the pins, the round trip through flash and the panel, and the access token that never leaves the node.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Nothing here touches hardware: the pins are numbers and the documents are text.
#include <cstdio>
#include <string>

#include "../core/bungard_config.hpp"
#include "../core/json.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                              \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++failures;                                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                \
    }                                                                                 \
  } while (0)

using namespace eh3d;

static bool has(const config::Problems& problems, const std::string& path, const std::string& code) {
  for (const config::Problem& p : problems) if (p.path == path && p.code == code) return true;
  return false;
}
static bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// A node ready to be saved: the defaults, plus the Wi-Fi network a board without a cable needs before its settings are valid.
static config::Settings fresh() {
  config::Settings s = config::default_settings("a1b2c3");
  s.sta.enabled = true;
  s.sta.ssid = "casa";
  s.sta.password = "una-clave-larga";
  return s;
}

// Applies a (partial) document on top of the defaults and checks the result, as the panel's PUT does.
static bool apply(const std::string& text, config::Settings& out, config::Problems& problems) {
  const config::Settings base = fresh();
  problems.clear();
  return config::load(text, base, out, problems);
}

static void test_defaults() {
  const config::Settings s = fresh();
  CHECK(config::validate(s).empty());
  CHECK(s.machine.link.port == 5800 && s.machine.link.token.empty() && !s.machine.link.allow_open);
  CHECK(s.machine.link.idle_ms == 3000 && s.machine.link.queue_bytes == 262144);
  CHECK(!s.machine.link.finish_on_loss && s.machine.link.max_drain_s == 600);
  CHECK(s.machine.uart.tx == 17 && s.machine.uart.rx == 18 && s.machine.uart.dtr == 15 && s.machine.uart.dcd == 16 && s.machine.uart.ri == 21);
  CHECK(s.machine.uart.cts == -1 && s.machine.uart.dsr == -1 && s.machine.uart.rs232_inverted);
  CHECK(s.machine.lamps.home_x == 1 && s.machine.lamps.home_y == 2 && s.machine.lamps.home_z == 7 && s.machine.lamps.enable == 8 && s.machine.lamps.tool == 38);
  CHECK(s.machine.lamps.active_low);
  CHECK(s.machine.spindle.max_rpm == 24000 && s.machine.spindle.pwm_pin == 4 && s.machine.spindle.pwm_hz == 2000);
  CHECK(s.machine.spindle.fwd_pin == 5 && s.machine.spindle.rev_pin == 6 && !s.machine.spindle.dir_single && !s.machine.spindle.outputs_active_low);
  CHECK(s.machine.spindle.reverse_delay_ms == 3000 && !s.machine.spindle.pwm_inverted);
}

static void test_unconfigured_node() {
  // a node that has never been configured is valid on the Ethernet board and asks only for a Wi-Fi network on the other one
  const config::Settings raw = config::default_settings("a1b2c3");
  const config::Problems problems = config::validate(raw);
  if (board::kHasEthernet) CHECK(problems.empty()); else CHECK(problems.size() == 1 && has(problems, "sta.enabled", "required"));
}

static void test_round_trip() {
  config::Settings s = fresh();
  s.machine.link.port = 5900;
  s.machine.link.token = "a-long-token-1";
  s.machine.link.allow_open = true;
  s.machine.link.idle_ms = 1500;
  s.machine.link.finish_on_loss = true;
  s.machine.link.max_drain_s = 90;
  s.machine.uart.rx = -1;
  s.machine.uart.cts = 9;
  s.machine.uart.rs232_inverted = false;
  s.machine.lamps.tool = -1;
  s.machine.lamps.active_low = false;
  s.machine.spindle.max_rpm = 18000;
  s.machine.spindle.pwm_hz = 5000;
  s.machine.spindle.dir_single = true;
  s.machine.spindle.outputs_active_low = true;
  s.machine.spindle.pwm_inverted = true;
  CHECK(config::validate(s).empty());
  // flash keeps the token; reading it back gives the same settings
  config::Settings back;
  config::Problems problems;
  CHECK(config::load(config::to_json(s, true), fresh(), back, problems));
  CHECK(back.machine.link.port == 5900 && back.machine.link.token == "a-long-token-1" && back.machine.link.allow_open && back.machine.link.idle_ms == 1500);
  CHECK(back.machine.link.finish_on_loss && back.machine.link.max_drain_s == 90);
  CHECK(back.machine.uart.rx == -1 && back.machine.uart.cts == 9 && !back.machine.uart.rs232_inverted);
  CHECK(back.machine.lamps.tool == -1 && !back.machine.lamps.active_low);
  CHECK(back.machine.spindle.max_rpm == 18000 && back.machine.spindle.pwm_hz == 5000 && back.machine.spindle.dir_single);
  CHECK(back.machine.spindle.outputs_active_low && back.machine.spindle.pwm_inverted);
  CHECK(config::to_json(back, true) == config::to_json(s, true));
}

static void test_token_never_leaves() {
  config::Settings s = config::default_settings("a1b2c3");
  s.machine.link.token = "very-secret-token";
  const std::string panel = config::to_json(s, false);
  CHECK(!contains(panel, "very-secret-token") && !contains(panel, "\"token\":") && contains(panel, "\"token_set\":true"));
  const std::string machine = config::machine_to_json(s.machine, false);
  CHECK(!contains(machine, "very-secret-token") && contains(machine, "\"token_set\":true"));
  s.machine.link.token.clear();
  CHECK(contains(config::machine_to_json(s.machine, false), "\"token_set\":false"));
  // flash does keep it
  s.machine.link.token = "very-secret-token";
  CHECK(contains(config::to_json(s, true), "very-secret-token"));
}

static void test_token_rules() {
  config::Settings s;
  config::Problems problems;
  CHECK(apply("{\"machine\":{\"link\":{\"token\":\"short\"}}}", s, problems) == false && has(problems, "machine.link.token", "too_short"));
  CHECK(apply("{\"machine\":{\"link\":{\"token\":\"12345678\"}}}", s, problems) && s.machine.link.token == "12345678");
  CHECK(apply("{\"machine\":{\"link\":{\"token\":\"with a tab\\there\"}}}", s, problems) == false && has(problems, "machine.link.token", "invalid"));
  CHECK(apply("{\"machine\":{\"link\":{\"token\":\"" + std::string(65, 'x') + "\"}}}", s, problems) == false);
  CHECK(config::token_is_valid(std::string(64, 'x')) && !config::token_is_valid(std::string(65, 'x')) && !config::token_is_valid("1234567"));
  // a section sent without a token (or with an empty one) keeps the stored one: the base carries it
  config::Settings stored = fresh();
  stored.machine.link.token = "stored-token-1";
  config::Settings out;
  config::Problems p2;
  CHECK(config::load("{\"machine\":{\"link\":{\"port\":5801}}}", stored, out, p2) && out.machine.link.token == "stored-token-1" && out.machine.link.port == 5801);
  CHECK(config::load("{\"machine\":{\"link\":{\"token\":\"\"}}}", stored, out, p2) && out.machine.link.token == "stored-token-1");
}

static void test_ranges() {
  config::Settings s;
  config::Problems problems;
  CHECK(!apply("{\"machine\":{\"link\":{\"port\":80}}}", s, problems) && has(problems, "machine.link.port", "range"));
  CHECK(!apply("{\"machine\":{\"link\":{\"port\":70000}}}", s, problems) && !problems.empty());
  CHECK(!apply("{\"machine\":{\"link\":{\"idle_ms\":100}}}", s, problems) && has(problems, "machine.link.idle_ms", "range"));
  CHECK(!apply("{\"machine\":{\"link\":{\"idle_ms\":70000}}}", s, problems) && has(problems, "machine.link.idle_ms", "range"));
  CHECK(apply("{\"machine\":{\"link\":{\"idle_ms\":200}}}", s, problems) && apply("{\"machine\":{\"link\":{\"idle_ms\":60000}}}", s, problems));
  CHECK(!apply("{\"machine\":{\"link\":{\"queue_bytes\":512}}}", s, problems) && has(problems, "machine.link.queue_bytes", "range"));
  CHECK(!apply("{\"machine\":{\"link\":{\"queue_bytes\":5000000}}}", s, problems) && has(problems, "machine.link.queue_bytes", "range"));
  CHECK(apply("{\"machine\":{\"link\":{\"queue_bytes\":4194304}}}", s, problems) && s.machine.link.queue_bytes == 4194304);
  CHECK(!apply("{\"machine\":{\"link\":{\"max_drain_s\":5}}}", s, problems) && has(problems, "machine.link.max_drain_s", "range"));
  CHECK(!apply("{\"machine\":{\"link\":{\"max_drain_s\":4000}}}", s, problems) && has(problems, "machine.link.max_drain_s", "range"));
  CHECK(apply("{\"machine\":{\"link\":{\"max_drain_s\":120,\"finish_on_loss\":true}}}", s, problems) && s.machine.link.finish_on_loss && s.machine.link.max_drain_s == 120);
  CHECK(!apply("{\"machine\":{\"spindle\":{\"pwm_hz\":50}}}", s, problems) && has(problems, "machine.spindle.pwm_hz", "range"));
  CHECK(!apply("{\"machine\":{\"spindle\":{\"pwm_hz\":50000}}}", s, problems) && has(problems, "machine.spindle.pwm_hz", "range"));
  CHECK(!apply("{\"machine\":{\"spindle\":{\"max_rpm\":70000}}}", s, problems) && !problems.empty());
  CHECK(!apply("{\"machine\":{\"spindle\":{\"reverse_delay_ms\":-1}}}", s, problems) && !problems.empty());
  CHECK(!apply("{\"machine\":{\"spindle\":{\"reverse_delay_ms\":61000}}}", s, problems) && has(problems, "machine.spindle.reverse_delay_ms", "range"));
  CHECK(!apply("{\"machine\":{\"uart\":{\"dtr\":99}}}", s, problems) && !problems.empty());
  // a field of the wrong type, and a section that is not an object
  CHECK(!apply("{\"machine\":{\"link\":{\"port\":\"x\"}}}", s, problems) && !problems.empty());
  CHECK(!apply("{\"machine\":{\"lamps\":5}}", s, problems) && has(problems, "machine.lamps", "invalid"));
  CHECK(!apply("{\"machine\":5}", s, problems) && !problems.empty());
}

static void test_pins() {
  config::Settings s;
  config::Problems problems;
  // the transmit pin is required; the others may be left out with -1
  CHECK(!apply("{\"machine\":{\"uart\":{\"tx\":-1}}}", s, problems) && has(problems, "machine.uart.tx", "required"));
  CHECK(apply("{\"machine\":{\"uart\":{\"rx\":-1,\"dtr\":-1,\"dcd\":-1,\"ri\":-1}}}", s, problems));
  CHECK(apply("{\"machine\":{\"lamps\":{\"home_x\":-1,\"home_y\":-1,\"home_z\":-1,\"enable\":-1,\"tool\":-1}}}", s, problems));
  // two jobs on one pin
  CHECK(!apply("{\"machine\":{\"uart\":{\"tx\":17,\"rx\":17}}}", s, problems) && !problems.empty());
  CHECK(!apply("{\"machine\":{\"lamps\":{\"home_x\":4}}}", s, problems) && !problems.empty());   // 4 is the spindle's PWM
  CHECK(!apply("{\"machine\":{\"spindle\":{\"fwd_pin\":6}}}", s, problems) && !problems.empty());   // 6 is the reverse line
  // flash, PSRAM and USB pins are refused
  CHECK(!apply("{\"machine\":{\"uart\":{\"tx\":19}}}", s, problems) && has(problems, "machine.uart.tx", "reserved"));
  CHECK(!apply("{\"machine\":{\"uart\":{\"tx\":20}}}", s, problems) && has(problems, "machine.uart.tx", "reserved"));
  CHECK(!apply("{\"machine\":{\"lamps\":{\"tool\":30}}}", s, problems) && has(problems, "machine.lamps.tool", "reserved"));
  CHECK(!apply("{\"machine\":{\"spindle\":{\"pwm_pin\":35}}}", s, problems) && has(problems, "machine.spindle.pwm_pin", "reserved"));
  // the speed PWM is required while there is a spindle
  CHECK(!apply("{\"machine\":{\"spindle\":{\"pwm_pin\":-1}}}", s, problems) && has(problems, "machine.spindle.pwm_pin", "required"));
}

static void test_no_spindle() {
  config::Settings s;
  config::Problems problems;
  // a top speed of 0 means no spindle output: its pins are free and their values are not checked
  CHECK(apply("{\"machine\":{\"spindle\":{\"max_rpm\":0,\"pwm_pin\":-1,\"pwm_hz\":5}}}", s, problems) && s.machine.spindle.max_rpm == 0);
  CHECK(apply("{\"machine\":{\"spindle\":{\"max_rpm\":0,\"pwm_pin\":17}}}", s, problems));   // 17 is the UART: free to use because the spindle is off
}

static void test_wire_form() {
  const config::Settings s = config::default_settings("a1b2c3");
  const std::string text = config::machine_to_json(s.machine, false);
  json::Value document;
  CHECK(json::parse(text, document) && document.is_object());
  for (const char* section : {"link", "uart", "lamps", "spindle"}) CHECK(document.get(section) != nullptr);
  CHECK(contains(text, "\"max_rpm\":24000") && contains(text, "\"pwm_pin\":4") && contains(text, "\"tool\":38") && contains(text, "\"port\":5800"));
  // the document of the whole node carries the machine section too
  CHECK(contains(config::to_json(s, false), "\"machine\":{"));
}

static void test_live_settings() {
  // settings that apply at once need no restart: two sets that differ only in them compare equal once the live ones are blanked
  config::Settings a = config::default_settings("a1b2c3");
  config::Settings b = a;
  b.machine.link.token = "another-token-9";
  b.machine.link.allow_open = true;
  b.machine.link.idle_ms = 5000;
  b.machine.link.finish_on_loss = true;
  b.machine.link.max_drain_s = 90;
  b.machine.spindle.reverse_delay_ms = 1000;
  b.node_name = "Drill station";
  config::Settings ca = a, cb = b;
  config::clear_live_settings(ca);
  config::clear_live_settings(cb);
  CHECK(config::to_json(ca, true) == config::to_json(cb, true));
  // a pin change does need one
  b = a;
  b.machine.lamps.home_x = 3;
  cb = b;
  config::clear_live_settings(cb);
  CHECK(config::to_json(ca, true) != config::to_json(cb, true));
  b = a;
  b.machine.spindle.max_rpm = 20000;
  cb = b;
  config::clear_live_settings(cb);
  CHECK(config::to_json(ca, true) != config::to_json(cb, true));
}

int main() {
  test_defaults();
  test_unconfigured_node();
  test_round_trip();
  test_token_never_leaves();
  test_token_rules();
  test_ranges();
  test_pins();
  test_no_spindle();
  test_wire_form();
  test_live_settings();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
