// EH3D-NODE-BUNGARD - host tests of what every node shares: the settings every node has (read, checked and written), the readers they are made with and the pins of the board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Nothing here touches hardware or a network: the pins are numbers, the clock is a number the test moves and the documents are text.
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "../core/board_s3.hpp"
#include "../core/bungard_config.hpp"
#include "../core/config_common.hpp"
#include "../core/json.hpp"
#include "../core/node_settings.hpp"

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
static bool contains_text(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
static json::Value parsed(const std::string& text) {
  json::Value document;
  const bool ok = json::parse(text, document);
  CHECK(ok);
  return document;
}

// ---- the pins of the module and of the base board -----------------------------------------------------------------------------------------------

static void test_module_pins() {
  using namespace board;
  for (int gpio = 22; gpio <= 25; ++gpio) CHECK(pin_info(gpio).reason == Reserved::kNoSuchPin && !assignable(gpio));
  for (int gpio = 26; gpio <= 32; ++gpio) CHECK(pin_info(gpio).reason == Reserved::kFlash && !assignable(gpio));
  for (int gpio = 33; gpio <= 37; ++gpio) CHECK(pin_info(gpio).reason == Reserved::kPsram && !assignable(gpio));
  CHECK(pin_info(19).reason == Reserved::kUsb && pin_info(20).reason == Reserved::kUsb && !assignable(19) && !assignable(20));
  CHECK(!assignable(-1) && !assignable(49) && !assignable(1000) && pin_info(-1).reason == Reserved::kNoSuchPin && pin_info(49).reason == Reserved::kNoSuchPin);
  // the strapping pins and the boot button are offered, with a warning
  for (const int gpio : {0, 3, 45, 46}) CHECK(assignable(gpio) && pin_info(gpio).use == PinUse::kCaution && std::string(pin_info(gpio).note) != "");
  CHECK(std::string(pin_info(0).note) == "boot" && std::string(pin_info(3).note) == "strapping");
  // the header of the module N16R8 carries 35..37 only on the board without the cable
  CHECK(pin_info(35).on_header == !kHasEthernet && pin_info(33).on_header == false);
  if (kHasEthernet) {
    for (int gpio = 9; gpio <= 14; ++gpio) CHECK(pin_info(gpio).reason == Reserved::kEthernet && !assignable(gpio));
    CHECK(assignable(8) && pin_info(8).use == PinUse::kCaution && std::string(pin_info(8).note) == "camera");   // the camera connector: nothing is plugged there
    for (int gpio = 4; gpio <= 7; ++gpio) CHECK(assignable(gpio) && std::string(pin_info(gpio).note) == "sdcard");
    CHECK(std::string(kId) == "s3-eth");
  } else {
    for (int gpio = 8; gpio <= 14; ++gpio) CHECK(assignable(gpio));
    CHECK(std::string(pin_info(43).note) == "usb_serial" && std::string(pin_info(48).note) == "led" && std::string(kId) == "s3-wifi");
  }
  // every GPIO that exists is exactly one of: free, a caution or reserved, and a reserved one has a reason
  for (int gpio = kFirstGpio; gpio <= kLastGpio; ++gpio) {
    const PinInfo info = pin_info(gpio);
    CHECK(info.gpio == gpio);
    CHECK((info.use == PinUse::kReserved) == (info.reason != Reserved::kNone));
    CHECK((info.use == PinUse::kFree) == (std::string(info.note).empty() && info.use != PinUse::kReserved));
  }
}


// ---- the readers the settings are made with ---------------------------------------------------------------------------------------------------

static void test_config_readers() {
  using namespace config::detail;
  config::Problems problems;
  const json::Value doc = parsed("{\"s\":\"text\",\"long\":\"abcdefgh\",\"n\":7,\"f\":1.5,\"neg\":-1,\"big\":1e12,\"b\":true,\"mode\":\"fast\",\"num_mode\":3,\"pw\":\"newpassword\",\"empty\":\"\",\"huge\":\""
                                 + std::string(65, 'x') + "\"}");
  std::string text = "old";
  read_text(doc, "s", text, 10, "p.s", problems);
  CHECK(text == "text" && problems.empty());
  read_text(doc, "long", text, 5, "p.long", problems);
  CHECK(text == "text" && has(problems, "p.long", "too_long"));   // refused text leaves the old one
  read_text(doc, "n", text, 5, "p.n", problems);
  CHECK(text == "text" && has(problems, "p.n", "invalid"));
  read_text(doc, "missing", text, 5, "p.missing", problems);
  CHECK(text == "text" && problems.size() == 2);   // a member that is not there is no problem
  int number = 3;
  problems.clear();
  read_int(doc, "n", number, 0, 10, "p.n", problems);
  CHECK(number == 7 && problems.empty());
  read_int(doc, "n", number, 8, 10, "p.n", problems);
  CHECK(number == 7 && has(problems, "p.n", "range"));
  problems.clear();
  read_int(doc, "f", number, 0, 10, "p.f", problems);
  CHECK(number == 7 && has(problems, "p.f", "range"));   // a fraction is not a whole number
  problems.clear();
  read_int(doc, "neg", number, 0, 10, "p.neg", problems);
  CHECK(number == 7 && has(problems, "p.neg", "range"));
  read_int(doc, "neg", number, -1, 10, "p.neg", problems);
  CHECK(number == -1);
  problems.clear();
  read_int(doc, "big", number, 0, 100, "p.big", problems);
  CHECK(number == -1 && has(problems, "p.big", "range"));
  problems.clear();
  read_int(doc, "s", number, 0, 100, "p.s", problems);
  CHECK(has(problems, "p.s", "invalid"));
  double real = 0;
  problems.clear();
  read_double(doc, "f", real, 0, 2, "p.f", problems);
  CHECK(real == 1.5 && problems.empty());
  read_double(doc, "f", real, 2, 3, "p.f", problems);
  CHECK(real == 1.5 && has(problems, "p.f", "range"));
  read_double(doc, "s", real, 0, 3, "p.s", problems);
  CHECK(has(problems, "p.s", "invalid"));
  bool flag = false;
  problems.clear();
  read_bool(doc, "b", flag, "p.b", problems);
  CHECK(flag && problems.empty());
  read_bool(doc, "n", flag, "p.n", problems);
  CHECK(flag && has(problems, "p.n", "invalid"));
  enum class Speed { kSlow, kFast };
  Speed speed = Speed::kSlow;
  CHECK(read_choice<Speed>(doc, "none", {{"slow", Speed::kSlow}, {"fast", Speed::kFast}}, speed) && speed == Speed::kSlow);   // not there: kept
  CHECK(!read_choice<Speed>(doc, "mode", {{"slow", Speed::kSlow}, {"quick", Speed::kFast}}, speed) && speed == Speed::kSlow);
  CHECK(!read_choice<Speed>(doc, "num_mode", {{"slow", Speed::kSlow}, {"fast", Speed::kFast}}, speed));
  CHECK(read_choice<Speed>(doc, "mode", {{"slow", Speed::kSlow}, {"fast", Speed::kFast}}, speed) && speed == Speed::kFast);
  // a secret: absent or empty keeps the stored one, "_clear" erases it, a new one replaces it
  problems.clear();
  std::string secret = "stored";
  read_secret(doc, "missing", secret, "p.x", problems);
  CHECK(secret == "stored");
  read_secret(doc, "empty", secret, "p.x", problems);
  CHECK(secret == "stored" && problems.empty());
  read_secret(doc, "pw", secret, "p.pw", problems);
  CHECK(secret == "newpassword");
  read_secret(doc, "huge", secret, "p.huge", problems);
  CHECK(secret == "newpassword" && has(problems, "p.huge", "too_long"));
  read_secret(doc, "n", secret, "p.n", problems);
  CHECK(secret == "newpassword" && has(problems, "p.n", "invalid"));
  read_secret(parsed("{\"pw\":\"another-one\",\"pw_clear\":true}"), "pw", secret, "p.pw", problems);   // "<name>_clear" wins over a new value in the same document
  CHECK(secret.empty());
  read_secret(parsed("{\"pw_clear\":false,\"pw\":\"another-one\"}"), "pw", secret, "p.pw", problems);
  CHECK(secret == "another-one");
  // two uses of one pin
  PinClaims claims;
  CHECK(claims.claim(5, "a") && !claims.claim(5, "b") && claims.claim(6, "b"));
  CHECK(claims.claim(-1, "x") && claims.claim(-1, "y") && claims.claim(board::kLastGpio + 1, "x") && claims.claim(board::kLastGpio + 1, "y"));   // a pin that does not exist cannot clash
  CHECK(claims.claim(board::kLastGpio, "last") && !claims.claim(board::kLastGpio, "again"));
}

// ---- the settings every node has -----------------------------------------------------------------------------------------------------------------

static config::NodeSettings good_node() {
  config::NodeSettings s;
  config::node_defaults(s, "a1b2c3");
  s.sta.enabled = true;
  s.sta.ssid = "casa";
  s.sta.password = "una-clave-larga";
  return s;
}
static config::Problems check(const config::NodeSettings& s) {
  config::Problems problems;
  config::node_validate(s, problems);
  return problems;
}
static std::string written(const config::NodeSettings& s, bool secrets) {
  json::Writer w;
  w.begin_object();
  config::node_write_head(w, s, secrets);
  config::node_write_tail(w, s);
  w.end_object();
  return w.str();
}

static void test_node_helpers() {
  using namespace config;
  CHECK(time_zone_is_valid("UTC0") && time_zone_is_valid("CET-1CEST,M3.5.0,M10.5.0/3") && time_zone_is_valid("<+03>-3") && time_zone_is_valid(std::string(48, 'A')));
  CHECK(!time_zone_is_valid("") && !time_zone_is_valid(std::string(49, 'A')) && !time_zone_is_valid("Europe Madrid") && !time_zone_is_valid("UTC0;rm") && !time_zone_is_valid("a\"b") && !time_zone_is_valid("A\n"));
  for (const char* code : {"en", "es", "de", "fr", "it", "ja", "zh", "pt"}) CHECK(language_is_known(code));
  for (const char* code : {"", "EN", "e", "eng", "pt-PT", "ru", "xx"}) CHECK(!language_is_known(code));
  for (const int hours : {0, 1, 2, 3, 4, 6, 12, 24, 48}) CHECK(auto_restart_hours_is_valid(hours));
  for (const int hours : {-1, 5, 7, 8, 13, 23, 25, 47, 49, 100}) CHECK(!auto_restart_hours_is_valid(hours));
  AccessPoint ap;
  ap.channel = 0;
  CHECK(effective_channel(ap, 0) == 1 && effective_channel(ap, 1) == 6 && effective_channel(ap, 2) == 11 && effective_channel(ap, 3) == 1 && effective_channel(ap, 0xFFFFFFFFu) == 1);
  ap.channel = 13; CHECK(effective_channel(ap, 1) == 13);
  ap.channel = 1; CHECK(effective_channel(ap, 1) == 1);
  ap.channel = 14; CHECK(effective_channel(ap, 1) == 6);
  ap.channel = -3; CHECK(effective_channel(ap, 2) == 11);
  CHECK(std::string(to_text(WifiSecurity::kOpen)) == "open" && std::string(to_text(WifiSecurity::kWpa2)) == "wpa2" && std::string(to_text(WifiSecurity::kWpa3)) == "wpa3" && std::string(to_text(WifiSecurity::kWpa2Wpa3)) == "wpa2wpa3");
  CHECK(std::string(to_text(Uplink::kWifi)) == "wifi" && std::string(to_text(Uplink::kEthernet)) == "ethernet" && std::string(to_text(WebMode::kHttp)) == "http" && std::string(to_text(WebMode::kBoth)) == "both" &&
        std::string(to_text(WebMode::kHttps)) == "https");
}

static void test_node_defaults_and_validation() {
  using namespace config;
  NodeSettings s;
  node_defaults(s, "a1b2c3");
  CHECK(s.node_id == "bungard-a1b2c3" && s.node_name == s.node_id && s.language == "en" && s.web == WebMode::kBoth && s.time.zone == "UTC0" && s.auto_restart_hours == 0);
  CHECK(s.uplink == (board::kHasEthernet ? Uplink::kEthernet : Uplink::kWifi) && s.ip.dhcp && !s.ap.enabled && !s.sta.enabled && s.ap.ssid == "EH3D-NODE-BUNGARD" && s.hostname.empty());
  // a node that was never configured lacks only a way in, and only on the board without a cable
  Problems fresh = check(s);
  if (board::kHasEthernet) CHECK(fresh.empty()); else CHECK(fresh.size() == 1 && has(fresh, "sta.enabled", "required"));
  CHECK(check(good_node()).empty());

  NodeSettings n = good_node();
  n.node_id = "Bad Id"; CHECK(has(check(n), "node.id", "invalid")); n.node_id = ""; CHECK(has(check(n), "node.id", "invalid")); n = good_node();
  n.node_name = ""; CHECK(has(check(n), "node.name", "required"));
  n.node_name = std::string(49, 'n'); CHECK(has(check(n), "node.name", "invalid"));
  n.node_name = std::string(48, 'n'); CHECK(check(n).empty());
  n.node_name = std::string(48, '\xC3').substr(0, 0); n.node_name = "caf\xC3\xA9"; CHECK(check(n).empty());
  n.node_name = "\xC3"; CHECK(has(check(n), "node.name", "invalid"));   // a cut character
  { std::string many; for (int i = 0; i < 48; ++i) many += "\xC3\xA9"; n.node_name = many; CHECK(check(n).empty()); n.node_name = many + "\xC3\xA9"; CHECK(has(check(n), "node.name", "invalid")); }   // 48 characters, not bytes
  n = good_node();
  n.language = "xx"; CHECK(has(check(n), "ui.language", "invalid")); n = good_node();
  n.auto_restart_hours = 5; CHECK(has(check(n), "system.auto_restart_hours", "invalid")); n = good_node();
  n.hostname = "Bad Name"; CHECK(has(check(n), "node.hostname", "invalid")); n.hostname = "good-name"; CHECK(check(n).empty()); n = good_node();
  n.uplink = Uplink::kEthernet;
  if (board::kHasEthernet) CHECK(check(n).empty()); else CHECK(has(check(n), "uplink", "not_available"));
  n = good_node();
  // a fixed address
  n.uplink = Uplink::kWifi;
  n.ip.dhcp = false;
  n.ip.address = "192.168.0.50"; n.ip.netmask = "255.255.255.0"; n.ip.gateway = "192.168.0.1"; n.ip.dns1 = "1.1.1.1"; n.ip.dns2 = "8.8.8.8";
  CHECK(check(n).empty());
  { NodeSettings m = n; m.ip.address = ""; CHECK(has(check(m), "ip.address", "required")); }
  { NodeSettings m = n; m.ip.address = "300.1.1.1"; CHECK(has(check(m), "ip.address", "invalid")); }
  { NodeSettings m = n; m.ip.address = "127.0.0.1"; CHECK(has(check(m), "ip.address", "invalid")); }
  { NodeSettings m = n; m.ip.address = "224.0.0.9"; CHECK(has(check(m), "ip.address", "invalid")); }
  { NodeSettings m = n; m.ip.address = "192.168.0.0"; CHECK(has(check(m), "ip.address", "invalid")); }       // the network address
  { NodeSettings m = n; m.ip.address = "192.168.0.255"; CHECK(has(check(m), "ip.address", "invalid")); }     // the broadcast address
  { NodeSettings m = n; m.ip.netmask = ""; CHECK(has(check(m), "ip.netmask", "required")); }
  { NodeSettings m = n; m.ip.netmask = "255.0.255.0"; CHECK(has(check(m), "ip.netmask", "invalid")); }
  { NodeSettings m = n; m.ip.netmask = "255.255.255.255"; CHECK(has(check(m), "ip.netmask", "invalid")); }
  { NodeSettings m = n; m.ip.gateway = ""; CHECK(has(check(m), "ip.gateway", "required")); }
  { NodeSettings m = n; m.ip.gateway = "10.0.0.1"; CHECK(has(check(m), "ip.gateway", "outside_subnet")); }
  { NodeSettings m = n; m.ip.gateway = "192.168.0.50"; CHECK(has(check(m), "ip.gateway", "conflict")); }
  { NodeSettings m = n; m.ip.gateway = "0.0.0.0"; CHECK(has(check(m), "ip.gateway", "invalid")); }
  { NodeSettings m = n; m.ip.dns1 = "dns.local"; CHECK(has(check(m), "ip.dns1", "invalid")); }
  { NodeSettings m = n; m.ip.dns2 = "1.2.3"; CHECK(has(check(m), "ip.dns2", "invalid")); }
  { NodeSettings m = n; m.ip.dns1.clear(); m.ip.dns2.clear(); CHECK(check(m).empty()); }
  { NodeSettings m = n; m.ip.dhcp = true; m.ip.address = "garbage"; CHECK(check(m).empty()); }   // with DHCP the fixed address is not looked at
  n = good_node();
  // the access point
  n.ap.enabled = true; n.ap.ssid = "mine"; n.ap.password = "secret-key"; CHECK(check(n).empty());
  { NodeSettings m = n; m.ap.ssid = ""; CHECK(has(check(m), "ap.ssid", "required")); }
  { NodeSettings m = n; m.ap.ssid = std::string(33, 'x'); CHECK(has(check(m), "ap.ssid", "invalid")); }
  { NodeSettings m = n; m.ap.password = ""; CHECK(has(check(m), "ap.password", "required")); }
  { NodeSettings m = n; m.ap.password = "short"; CHECK(has(check(m), "ap.password", "invalid_key")); }
  { NodeSettings m = n; m.ap.security = WifiSecurity::kOpen; m.ap.password = ""; CHECK(check(m).empty()); }   // an open network needs no key
  { NodeSettings m = n; m.ap.country = "es"; CHECK(has(check(m), "ap.country", "invalid")); }
  { NodeSettings m = n; m.ap.country = "ESP"; CHECK(has(check(m), "ap.country", "invalid")); }
  { NodeSettings m = n; m.ap.country = ""; CHECK(has(check(m), "ap.country", "invalid")); }
  { NodeSettings m = n; m.ap.enabled = false; m.ap.ssid = ""; m.ap.password = "x"; CHECK(check(m).empty()); }   // a disabled access point is not checked
  n = good_node();
  // the station and its backups
  { NodeSettings m = n; m.sta.ssid = ""; CHECK(has(check(m), "sta.ssid", "required")); }
  { NodeSettings m = n; m.sta.password = "short"; CHECK(has(check(m), "sta.password", "invalid_key")); }
  { NodeSettings m = n; m.sta.password.clear(); CHECK(check(m).empty()); }   // an open network
  { NodeSettings m = n; m.sta.backup.push_back({"other", "another-key"}); CHECK(check(m).empty()); }
  { NodeSettings m = n; m.sta.backup.push_back({"", "another-key"}); CHECK(has(check(m), "sta.backup.0.ssid", "required")); }
  { NodeSettings m = n; m.sta.backup.push_back({"ok", "another-key"}); m.sta.backup.push_back({"ok2", "short"}); CHECK(has(check(m), "sta.backup.1.password", "invalid_key")); }
  // Wi-Fi with neither way in
  { NodeSettings m = n; m.uplink = Uplink::kWifi; m.sta.enabled = false; m.ap.enabled = false; CHECK(has(check(m), "sta.enabled", "required")); m.ap.enabled = true; m.ap.password = "secret-key"; CHECK(check(m).empty()); }
  // the clock
  { NodeSettings m = n; m.time.ntp = "bad host!"; CHECK(has(check(m), "time.ntp", "invalid")); m.time.ntp_enabled = false; CHECK(check(m).empty()); }
  { NodeSettings m = n; m.time.zone = "Europe/Madrid;x"; CHECK(has(check(m), "time.zone", "invalid")); }
}

static void test_node_round_trip() {
  using namespace config;
  NodeSettings s = good_node();
  s.node_id = "custom-id"; s.node_name = "Caf\xC3\xA9 node"; s.hostname = "my-host"; s.uplink = Uplink::kWifi;
  s.ip.dhcp = false; s.ip.address = "10.1.2.3"; s.ip.netmask = "255.255.0.0"; s.ip.gateway = "10.1.0.1"; s.ip.dns1 = "9.9.9.9"; s.ip.dns2 = "1.1.1.1";
  s.ap.enabled = true; s.ap.ssid = "my-ap"; s.ap.security = WifiSecurity::kWpa2Wpa3; s.ap.password = "ap-secret-key"; s.ap.channel = 11; s.ap.hidden = true; s.ap.max_clients = 5; s.ap.tx_power_dbm = 8; s.ap.bandwidth_mhz = 40; s.ap.country = "DE";
  s.sta.backup = {{"back1", "back1-password"}, {"back2", ""}};
  s.time.ntp_enabled = false; s.time.ntp = "ntp.local"; s.time.zone = "CET-1CEST,M3.5.0,M10.5.0/3";
  s.web = WebMode::kHttps; s.admin_sources = "192.168.0.0/24, 203.0.113.9"; s.language = "pt"; s.auto_restart_hours = 12;
  CHECK(check(s).empty());

  // flash: everything, secrets included
  const std::string stored = written(s, true);
  NodeSettings back;
  Problems problems;
  node_read(parsed(stored), back, problems);
  CHECK(problems.empty());
  CHECK(back.node_id == s.node_id && back.node_name == s.node_name && back.hostname == s.hostname && back.uplink == s.uplink && !back.ip.dhcp == !s.ip.dhcp && back.ip.address == "10.1.2.3" && back.ip.netmask == "255.255.0.0" &&
        back.ip.gateway == "10.1.0.1" && back.ip.dns1 == "9.9.9.9" && back.ip.dns2 == "1.1.1.1");
  CHECK(back.ap.enabled && back.ap.ssid == "my-ap" && back.ap.security == WifiSecurity::kWpa2Wpa3 && back.ap.password == "ap-secret-key" && back.ap.channel == 11 && back.ap.hidden && back.ap.max_clients == 5 &&
        back.ap.tx_power_dbm == 8 && back.ap.bandwidth_mhz == 40 && back.ap.country == "DE");
  CHECK(back.sta.enabled && back.sta.ssid == "casa" && back.sta.password == "una-clave-larga" && back.sta.backup.size() == 2 && back.sta.backup[0].ssid == "back1" && back.sta.backup[0].password == "back1-password" && back.sta.backup[1].password.empty());
  CHECK(!back.time.ntp_enabled && back.time.ntp == "ntp.local" && back.time.zone == s.time.zone && back.web == WebMode::kHttps && back.admin_sources == s.admin_sources && back.language == "pt" && back.auto_restart_hours == 12);
  CHECK(written(back, true) == stored);   // writing what was read gives the same document
  CHECK(check(back).empty());

  // the panel: the same document with no password in it, only whether there is one
  const std::string shown = written(s, false);
  CHECK(!contains_text(shown, "ap-secret-key") && !contains_text(shown, "una-clave-larga") && !contains_text(shown, "back1-password"));
  const json::Value view = parsed(shown);
  CHECK(view.get("ap")->bool_or("password_set", false) && view.get("sta")->bool_or("password_set", false));
  CHECK(view.get("ap")->get("password") == nullptr && view.get("sta")->get("password") == nullptr);
  CHECK(view.get("sta")->get("backup")->items[0].bool_or("password_set", false) && !view.get("sta")->get("backup")->items[1].bool_or("password_set", true));
  // what the panel sends back (no passwords) keeps the stored ones
  NodeSettings kept = s;
  Problems again;
  node_read(view, kept, again);
  CHECK(again.empty() && kept.ap.password == s.ap.password && kept.sta.password == s.sta.password && kept.sta.backup[0].password == "back1-password");
  CHECK(written(kept, true) == stored);
}

static void test_node_read_rules() {
  using namespace config;
  auto read = [](NodeSettings& s, const std::string& text) { Problems p; node_read(parsed(text), s, p); return p; };
  NodeSettings s = good_node();
  const std::string before = written(s, true);
  CHECK(read(s, "{}").empty() && written(s, true) == before);                              // an empty document changes nothing
  CHECK(read(s, "{\"unknown\":{\"x\":1},\"ap\":{\"nothing\":1}}").empty() && written(s, true) == before);   // neither does what it does not know
  CHECK(read(s, "{\"node\":5,\"ap\":\"x\",\"sta\":[],\"web\":1,\"time\":0,\"ui\":\"x\",\"system\":[]}").empty() && written(s, true) == before);   // a section of the wrong type is skipped
  // the wrong type or range for a member is named, and the old value stays
  Problems p = read(s, "{\"node\":{\"id\":5,\"name\":true,\"hostname\":[]}}");
  CHECK(has(p, "node.id", "invalid") && has(p, "node.name", "invalid") && has(p, "node.hostname", "invalid") && s.node_id == "bungard-a1b2c3");
  p = read(s, "{\"node\":{\"id\":\"" + std::string(65, 'a') + "\",\"name\":\"" + std::string(49, 'a') + "\",\"hostname\":\"" + std::string(33, 'a') + "\"}}");
  CHECK(has(p, "node.id", "too_long") && has(p, "node.name", "too_long") && has(p, "node.hostname", "too_long"));
  p = read(s, "{\"uplink\":\"bogus\"}"); CHECK(has(p, "uplink", "invalid") && s.uplink == (board::kHasEthernet ? Uplink::kEthernet : Uplink::kWifi));
  p = read(s, "{\"uplink\":3}"); CHECK(has(p, "uplink", "invalid"));
  p = read(s, "{\"ip\":{\"dhcp\":\"yes\",\"address\":\"1.2.3.4.5.6.7.8.9\",\"netmask\":7}}"); CHECK(has(p, "ip.dhcp", "invalid") && has(p, "ip.address", "too_long") && has(p, "ip.netmask", "invalid") && s.ip.dhcp);
  p = read(s, "{\"ap\":{\"enabled\":1,\"ssid\":9,\"security\":\"wep\",\"channel\":14,\"max_clients\":0,\"tx_power_dbm\":1,\"bandwidth_mhz\":41,\"country\":\"ESP\",\"hidden\":\"no\"}}");
  CHECK(has(p, "ap.enabled", "invalid") && has(p, "ap.ssid", "invalid") && has(p, "ap.security", "invalid") && has(p, "ap.channel", "range") && has(p, "ap.max_clients", "range") && has(p, "ap.tx_power_dbm", "range") &&
        has(p, "ap.bandwidth_mhz", "range") && has(p, "ap.country", "too_long") && has(p, "ap.hidden", "invalid"));
  p = read(s, "{\"ap\":{\"bandwidth_mhz\":25}}"); CHECK(has(p, "ap.bandwidth_mhz", "range") && s.ap.bandwidth_mhz == 20);   // between the two is neither
  p = read(s, "{\"ap\":{\"channel\":13,\"max_clients\":10,\"tx_power_dbm\":20,\"bandwidth_mhz\":40,\"security\":\"wpa3\"}}");
  CHECK(p.empty() && s.ap.channel == 13 && s.ap.max_clients == 10 && s.ap.tx_power_dbm == 20 && s.ap.bandwidth_mhz == 40 && s.ap.security == WifiSecurity::kWpa3);
  p = read(s, "{\"ap\":{\"channel\":0,\"max_clients\":1,\"tx_power_dbm\":2,\"bandwidth_mhz\":20}}"); CHECK(p.empty() && s.ap.channel == 0 && s.ap.max_clients == 1 && s.ap.tx_power_dbm == 2);
  p = read(s, "{\"ap\":{\"channel\":1.5,\"max_clients\":\"3\"}}"); CHECK(has(p, "ap.channel", "range") && has(p, "ap.max_clients", "invalid"));
  p = read(s, "{\"web\":{\"mode\":\"ftp\",\"admin_sources\":\"not-an-address\"}}"); CHECK(has(p, "web.mode", "invalid") && has(p, "web.admin_sources", "invalid") && s.admin_sources.empty());
  p = read(s, "{\"web\":{\"mode\":\"http\",\"admin_sources\":\"10.0.0.0/8\"}}"); CHECK(p.empty() && s.web == WebMode::kHttp && s.admin_sources == "10.0.0.0/8");
  p = read(s, "{\"web\":{\"admin_sources\":\"\"}}"); CHECK(p.empty() && s.admin_sources.empty());   // empty is the house's private ranges
  p = read(s, "{\"web\":{\"admin_sources\":\"" + std::string(121, '1') + "\"}}"); CHECK(has(p, "web.admin_sources", "too_long"));
  p = read(s, "{\"time\":{\"ntp_enabled\":\"x\",\"ntp\":\"" + std::string(65, 'n') + "\",\"zone\":\"" + std::string(49, 'z') + "\"}}"); CHECK(has(p, "time.ntp_enabled", "invalid") && has(p, "time.ntp", "too_long") && has(p, "time.zone", "too_long"));
  p = read(s, "{\"ui\":{\"language\":\"english\"}}"); CHECK(has(p, "ui.language", "too_long") && s.language == "en");
  p = read(s, "{\"ui\":{\"language\":\"xx\"}}"); CHECK(p.empty() && s.language == "xx" && has(check(s), "ui.language", "invalid"));   // reading accepts a short text; checking is what refuses an unknown language
  s.language = "en";
  p = read(s, "{\"system\":{\"auto_restart_hours\":49}}"); CHECK(has(p, "system.auto_restart_hours", "range") && s.auto_restart_hours == 0);
  p = read(s, "{\"system\":{\"auto_restart_hours\":5}}"); CHECK(p.empty() && s.auto_restart_hours == 5 && has(check(s), "system.auto_restart_hours", "invalid"));
  // the older place of the time server
  s.auto_restart_hours = 0;
}

static void test_node_backup_lists() {
  using namespace config;
  auto read = [](NodeSettings& s, const std::string& text) { Problems p; node_read(parsed(text), s, p); return p; };
  NodeSettings s = good_node();
  s.sta.backup = {{"one", "one-password"}, {"two", "two-password"}, {"three", "three-password"}};
  // the list that arrives replaces the stored one: removing an entry and saving does not bring it back
  Problems p = read(s, "{\"sta\":{\"backup\":[{\"ssid\":\"two\"}]}}");
  CHECK(p.empty() && s.sta.backup.size() == 1 && s.sta.backup[0].ssid == "two" && s.sta.backup[0].password == "two-password");
  p = read(s, "{\"sta\":{\"backup\":[]}}"); CHECK(p.empty() && s.sta.backup.empty());
  // a network with a new name starts without a password; the same name keeps its own, whatever its place
  s.sta.backup = {{"one", "one-password"}, {"two", "two-password"}};
  p = read(s, "{\"sta\":{\"backup\":[{\"ssid\":\"two\"},{\"ssid\":\"fresh\"},{\"ssid\":\"one\",\"password\":\"changed-pass\"}]}}");
  CHECK(p.empty() && s.sta.backup.size() == 3 && s.sta.backup[0].password == "two-password" && s.sta.backup[1].password.empty() && s.sta.backup[2].password == "changed-pass");
  // more than fit loses the extras only
  p = read(s, "{\"sta\":{\"backup\":[{\"ssid\":\"a\"},{\"ssid\":\"b\"},{\"ssid\":\"c\"},{\"ssid\":\"d\"},{\"ssid\":\"e\"}]}}");
  CHECK(p.empty() && s.sta.backup.size() == kMaxBackupNetworks && s.sta.backup[2].ssid == "c");
  // a list that is not a list, or an entry that is not an object, is named
  const std::size_t kept = s.sta.backup.size();
  p = read(s, "{\"sta\":{\"backup\":\"x\"}}"); CHECK(has(p, "sta.backup", "invalid") && s.sta.backup.size() == kept);
  p = read(s, "{\"sta\":{\"backup\":[5,{\"ssid\":\"ok\"}]}}"); CHECK(has(p, "sta.backup.0.ssid", "invalid") && s.sta.backup.size() == 1 && s.sta.backup[0].ssid == "ok");
  p = read(s, "{\"sta\":{\"backup\":[{\"ssid\":\"" + std::string(33, 's') + "\"}]}}"); CHECK(has(p, "sta.backup.0.ssid", "too_long"));
  // a secret: kept when absent or empty, erased on request, refused when too long
  s.sta.password = "keep-me"; s.ap.password = "ap-password";
  p = read(s, "{\"sta\":{\"password\":\"\"},\"ap\":{}}"); CHECK(p.empty() && s.sta.password == "keep-me" && s.ap.password == "ap-password");
  p = read(s, "{\"sta\":{\"password_clear\":true},\"ap\":{\"password_clear\":true,\"password\":\"ignored-when-cleared\"}}"); CHECK(p.empty() && s.sta.password.empty() && s.ap.password.empty());
  p = read(s, "{\"sta\":{\"password\":\"" + std::string(65, 'p') + "\"}}"); CHECK(has(p, "sta.password", "too_long") && s.sta.password.empty());
}


int main() {
  test_module_pins();
  test_config_readers();
  test_node_helpers();
  test_node_defaults_and_validation();
  test_node_round_trip();
  test_node_read_rules();
  test_node_backup_lists();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
