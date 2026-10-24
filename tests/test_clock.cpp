// EH3D-NODE-BUNGARD - host tests of the clock settings (time zone, time server), the calendar arithmetic behind the offset and the backup lists keeping their passwords.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <string>

#include "../core/bungard_config.hpp"
#include "../core/civil_time.hpp"

using namespace eh3d;
static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++fails; } } while (0)
static void show(const char* what, const config::Problems& problems) {
  for (const auto& p : problems) std::printf("  [%s] problem %s %s\n", what, p.path.c_str(), p.code.c_str());
}
int main() {
  config::Settings base = config::default_settings("a1b2c3");
  base.sta.enabled = true; base.sta.ssid = "home"; base.sta.password = "home-password";
  config::Settings out;
  config::Problems problems;
  CHECK(base.time.ntp_enabled && base.time.zone == "UTC0" && base.time.ntp == "pool.ntp.org");
  config::Settings s = base;
  s.time.zone = "CET-1CEST,M3.5.0,M10.5.0/3"; s.time.ntp = "time.example.org"; s.time.ntp_enabled = false;
  s.sta.backup = {{"guest", "guest-password"}};
  const std::string text = config::to_json(s, true);
  config::Settings back;
  const bool loaded = config::load(text, base, back, problems);
  show("roundtrip", problems);
  CHECK(loaded);
  CHECK(back.time.zone == s.time.zone && back.time.ntp == "time.example.org" && !back.time.ntp_enabled);
  CHECK(text.find("\"time\":{") != std::string::npos);
  problems.clear();
  const bool inherited = config::load(R"({"sta":{"backup":[{"ssid":"guest"}]}})", s, out, problems);
  show("inherit", problems);
  CHECK(inherited && out.sta.backup[0].password == "guest-password");
  problems.clear();
  const bool other = config::load(R"({"sta":{"backup":[{"ssid":"other"}]}})", s, out, problems);
  show("other", problems);
  CHECK(other && out.sta.backup[0].password.empty());
  problems.clear();
  CHECK(!config::load(R"({"time":{"zone":"Europe/Madrid; rm"}})", base, out, problems));
  problems.clear();
  CHECK(!config::load(R"({"time":{"ntp_enabled":true,"ntp":"not a host"}})", base, out, problems));
  CHECK(civil::days_from_civil(1970, 1, 1) == 0 && civil::days_from_civil(2000, 3, 1) == 11017);
  std::printf(fails ? "FAILED\n" : "ok\n");
  return fails;
}
