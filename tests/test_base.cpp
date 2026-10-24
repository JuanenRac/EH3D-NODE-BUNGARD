// EH3D-NODE-BUNGARD - host tests of what every node shares and that has no hardware: the schedule of retries, Improv Wi-Fi over serial and the stack report.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Nothing was run on a board; the packets are the ones the standard (https://www.improv-wifi.com/serial/) describes.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/admin_sources.hpp"
#include "../core/auth.hpp"
#include "../core/backoff.hpp"
#include "../core/improv.hpp"
#include "../core/json.hpp"
#include "../core/memory_budget.hpp"
#include "../core/node_guard.hpp"
#include "../core/stored_load.hpp"
#include "../core/task_health.hpp"

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

static bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// ---- the schedule of retries -----------------------------------------------------------------------------------------------------------




static void test_backoff() {
  retry::Backoff plain(1000, 30000, 0);   // no scatter: the schedule itself
  const std::uint32_t expected[] = {1000, 2000, 4000, 8000, 16000, 30000, 30000, 30000};
  for (const std::uint32_t wait : expected) CHECK(plain.next(12345) == wait);
  CHECK(plain.failures() >= 8);
  plain.reset();
  CHECK(plain.failures() == 0 && plain.next(1) == 1000);   // a try that worked starts the schedule over
  // with scatter: within 20 % of the plain wait, never above the ceiling, never zero
  retry::Backoff scattered(1000, 30000, 20);
  for (std::uint32_t random : {0u, 1u, 20u, 40u, 41u, 999u, 4294967295u}) {
    retry::Backoff b(1000, 30000, 20);
    b.next(random); b.next(random);                       // two failures: the third wait is 4000 ms
    const std::uint32_t wait = b.next(random);
    CHECK(wait >= 3200 && wait <= 4800);
  }
  for (int i = 0; i < 40; ++i) { const std::uint32_t wait = scattered.next(static_cast<std::uint32_t>(i) * 2654435761u); CHECK(wait >= 1 && wait <= 30000); }
  // odd settings are made sensible: a ceiling under the first wait is the first wait, a first wait of zero is one millisecond
  retry::Backoff odd(0, 0, 500);
  CHECK(odd.next(7) >= 1 && odd.next(7) <= 2);
  // many nodes that lost the same router at the same moment do not wait the same: different random numbers give different waits
  retry::Backoff a(1000, 60000, 20), b(1000, 60000, 20);
  a.next(1); b.next(2);
  CHECK(a.next(3) != b.next(9));
}

// ---- Improv Wi-Fi over serial ---------------------------------------------------------------------------------------------------------

static std::vector<std::uint8_t> rpc_wifi(const std::string& ssid, const std::string& password) {
  std::vector<std::uint8_t> data{0x01, static_cast<std::uint8_t>(2 + ssid.size() + password.size()), static_cast<std::uint8_t>(ssid.size())};
  data.insert(data.end(), ssid.begin(), ssid.end());
  data.push_back(static_cast<std::uint8_t>(password.size()));
  data.insert(data.end(), password.begin(), password.end());
  return improv::packet(improv::Type::kRpc, data);
}

static std::optional<improv::Packet> feed_all(improv::Parser& parser, const std::vector<std::uint8_t>& bytes) {
  std::optional<improv::Packet> last;
  for (const std::uint8_t b : bytes) if (auto p = parser.feed(b)) last = p;
  return last;
}

static void test_improv_packets() {
  // the state packet of the standard: IMPROV, version 1, type 1, length 1, the state, the checksum and a newline
  const std::vector<std::uint8_t> ready = improv::state_packet(improv::State::kReady);
  const std::vector<std::uint8_t> known{'I', 'M', 'P', 'R', 'O', 'V', 0x01, 0x01, 0x01, 0x02, 0x00, '\n'};
  std::vector<std::uint8_t> sum_of(known.begin(), known.begin() + 10);
  CHECK(improv::checksum(sum_of) == static_cast<std::uint8_t>((73 + 77 + 80 + 82 + 79 + 86 + 1 + 1 + 1 + 2) & 0xFF));
  CHECK(ready.size() == 12 && ready[7] == 0x01 && ready[8] == 1 && ready[9] == 0x02 && ready.back() == '\n' && ready[10] == improv::checksum(sum_of));
  const std::vector<std::uint8_t> error = improv::error_packet(improv::Error::kUnableToConnect);
  CHECK(error[7] == 0x02 && error[9] == 0x03);
  // an RPC result: command, the length of what follows, then each string with its length in front
  const std::vector<std::uint8_t> result = improv::result_packet(improv::Command::kGetDeviceInfo, {"ab", "c"});
  CHECK(result[7] == 0x04 && result[8] == 7 && result[9] == 0x04 && result[10] == 5 && result[11] == 2 && result[12] == 'a' && result[13] == 'b' && result[14] == 1 && result[15] == 'c');
  const std::vector<std::uint8_t> empty = improv::result_packet(improv::Command::kWifiSettings, {});
  CHECK(empty[8] == 2 && empty[9] == 0x01 && empty[10] == 0);
  // a packet never carries more than 255 bytes of data
  const std::vector<std::uint8_t> big = improv::packet(improv::Type::kRpcResult, std::vector<std::uint8_t>(400, 'x'));
  CHECK(big[8] == 255 && big.size() == 9 + 255 + 2);
}

static void test_improv_parser() {
  improv::Parser parser;
  // the Wi-Fi settings, with noise (log lines, a person typing) before and between packets
  std::vector<std::uint8_t> stream;
  for (const char c : std::string("I (123) eh3d-net: link up\r\nIMP garbage IMPROV\r\n")) stream.push_back(static_cast<std::uint8_t>(c));
  const std::vector<std::uint8_t> wifi = rpc_wifi("HomeRouter", "una-clave-larga");
  stream.insert(stream.end(), wifi.begin(), wifi.end());
  const std::optional<improv::Packet> got = feed_all(parser, stream);
  CHECK(got.has_value() && got->type == improv::Type::kRpc);
  improv::Rpc rpc;
  CHECK(got && improv::parse_rpc(got->data, rpc) == improv::Error::kNone && rpc.command == improv::Command::kWifiSettings && rpc.ssid == "HomeRouter" && rpc.password == "una-clave-larga");
  // a wrong checksum is dropped and counted, and the next good packet still arrives
  std::vector<std::uint8_t> broken = rpc_wifi("a", "b");
  broken[broken.size() - 2] ^= 0x55;
  const std::uint32_t before = parser.rejected();
  CHECK(!feed_all(parser, broken).has_value() && parser.rejected() == before + 1);
  CHECK(feed_all(parser, rpc_wifi("a", "b")).has_value());
  // a version that is not 1 is not ours
  std::vector<std::uint8_t> future = rpc_wifi("a", "b");
  future[6] = 0x02;
  CHECK(!feed_all(parser, future).has_value());
  CHECK(feed_all(parser, rpc_wifi("c", "")).has_value());
  // a packet fed one byte at a time and one cut in the middle by another header
  improv::Parser slow;
  std::optional<improv::Packet> seen;
  for (const std::uint8_t b : rpc_wifi("x", "y")) if (auto p = slow.feed(b)) seen = p;
  CHECK(seen.has_value());
  improv::Parser cut;
  std::vector<std::uint8_t> half = rpc_wifi("abc", "def");
  half.resize(11);
  const std::vector<std::uint8_t> whole = rpc_wifi("zz", "yy");
  half.insert(half.end(), whole.begin(), whole.end());
  const std::optional<improv::Packet> after_cut = feed_all(cut, half);
  CHECK(after_cut.has_value() || cut.rejected() >= 1);   // the cut one is lost; the stream is not wedged
  CHECK(feed_all(cut, rpc_wifi("q", "r")).has_value());
  // an empty stream and a lone byte do nothing
  improv::Parser nothing;
  CHECK(!nothing.feed('I') && !nothing.feed('x') && !nothing.feed(0));
}

static void test_improv_rpc() {
  improv::Rpc rpc;
  CHECK(improv::parse_rpc({0x02, 0x00}, rpc) == improv::Error::kNone && rpc.command == improv::Command::kIdentify);
  CHECK(improv::parse_rpc({0x03, 0x00}, rpc) == improv::Error::kNone && rpc.command == improv::Command::kGetCurrentState);
  CHECK(improv::parse_rpc({0x04, 0x00}, rpc) == improv::Error::kNone && rpc.command == improv::Command::kGetDeviceInfo);
  CHECK(improv::parse_rpc({0x05, 0x00}, rpc) == improv::Error::kNone && rpc.command == improv::Command::kGetWifiNetworks);
  CHECK(improv::parse_rpc({0x77, 0x00}, rpc) == improv::Error::kUnknownRpc);
  CHECK(improv::parse_rpc({}, rpc) == improv::Error::kInvalidRpc && improv::parse_rpc({0x02}, rpc) == improv::Error::kInvalidRpc);
  CHECK(improv::parse_rpc({0x02, 0x05}, rpc) == improv::Error::kInvalidRpc);   // the length does not match the data
  // the Wi-Fi settings: the lengths must add up, the SSID is 1 to 32 bytes and the password at most 63
  CHECK(improv::parse_rpc({0x01, 0x03, 0x01, 'a', 0x00}, rpc) == improv::Error::kNone && rpc.ssid == "a" && rpc.password.empty());
  CHECK(improv::parse_rpc({0x01, 0x03, 0x05, 'a', 0x00}, rpc) == improv::Error::kInvalidRpc);          // the SSID says 5 bytes and has 1
  CHECK(improv::parse_rpc({0x01, 0x03, 0x00, 'a', 0x00}, rpc) == improv::Error::kInvalidRpc);          // an empty SSID
  std::vector<std::uint8_t> long_ssid{0x01, 0x00, 33};
  long_ssid.insert(long_ssid.end(), 33, 'a'); long_ssid.push_back(0); long_ssid[1] = static_cast<std::uint8_t>(long_ssid.size() - 2);
  CHECK(improv::parse_rpc(long_ssid, rpc) == improv::Error::kInvalidRpc);
  std::vector<std::uint8_t> long_pass{0x01, 0x00, 1, 'a', 64};
  long_pass.insert(long_pass.end(), 64, 'p'); long_pass[1] = static_cast<std::uint8_t>(long_pass.size() - 2);
  CHECK(improv::parse_rpc(long_pass, rpc) == improv::Error::kInvalidRpc);
  CHECK(improv::parse_rpc({0x01, 0x01, 0x00}, rpc) == improv::Error::kInvalidRpc);
}

// ---- the stack report -----------------------------------------------------------------------------------------------------------------------

// ---- the memory ---------------------------------------------------------------------------------------------------------------------------

static membudget::Snapshot memory_sample(std::uint32_t internal_free, std::uint32_t largest, std::uint32_t psram_free = 7700u * 1024u, std::uint32_t psram_total = 8192u * 1024u) {
  membudget::Snapshot s;
  s.internal_free = internal_free; s.internal_min = internal_free > 5000 ? internal_free - 5000 : 0; s.internal_largest = largest; s.psram_free = psram_free; s.psram_total = psram_total;
  return s;
}

// ---- where the memory goes and how it is judged ---------------------------------------------------------------------------------------------

static void test_memory_budget() {
  using namespace membudget;
  // "low" has two levels: a figure that hovers around the line neither raises the warning nor clears it all day
  CHECK(!internal_is_low(memory_sample(71000, 36000), false));                  // the board that ran short before: 65 to 80 KB is above the line
  CHECK(internal_is_low(memory_sample(39000, 30000), false) && !internal_is_low(memory_sample(41000, 30000), false));
  CHECK(internal_is_low(memory_sample(45000, 30000), true) && !internal_is_low(memory_sample(50000, 30000), true));   // already low: it clears only over 48 KB
  CHECK(internal_is_low(memory_sample(200000, 7000), false) && !internal_is_low(memory_sample(200000, 8192), false));   // plenty free but in pieces: no block of 8 KB
  CHECK(!internal_is_low(Snapshot{}, false) && !internal_is_low(Snapshot{}, true));                                       // nothing read: no verdict
  // where a big buffer or a stack goes
  const Snapshot with_psram = memory_sample(80000, 40000);
  const Snapshot without_psram = memory_sample(80000, 40000, 0, 0);
  const Snapshot nearly_full = memory_sample(80000, 40000, 300u * 1024u, 8192u * 1024u);
  Choice c = choose_place(true, 12288, with_psram);
  CHECK(c.place == Place::kPsram && !c.degraded);
  c = choose_place(false, 12288, with_psram);
  CHECK(c.place == Place::kInternal && !c.degraded);                           // not wanted there: internal, and nothing to report
  c = choose_place(true, 12288, without_psram);
  CHECK(c.place == Place::kInternal && c.degraded);                            // a board with no PSRAM keeps working on the internal memory
  c = choose_place(true, 64u * 1024u, nearly_full);
  CHECK(c.place == Place::kInternal && c.degraded);                            // 300 KB free is not enough for 64 KB and the 256 KB that always stay
  c = choose_place(true, 40u * 1024u, nearly_full);
  CHECK(c.place == Place::kPsram);                                             // exactly what is left over the spare is enough
  CHECK(choose_place(true, 0xFFFFFFFFu, with_psram).place == Place::kInternal);   // no overflow on a silly size
  // the log: the first time, then every minute, and at once (not twice in ten seconds) when the memory turns low or back
  LogState state;
  const Snapshot fine = memory_sample(90000, 40000), low = memory_sample(30000, 20000);
  CHECK(next_log(state, 20000, fine) == LogAction::kInfo);
  CHECK(next_log(state, 21000, fine) == LogAction::kNone && next_log(state, 79999, fine) == LogAction::kNone);
  CHECK(next_log(state, 80000, fine) == LogAction::kInfo);                     // a minute after the last
  CHECK(next_log(state, 85000, low) == LogAction::kNone);                      // turned low only 5 s after the last line: not twice in ten seconds
  CHECK(next_log(state, 90000, low) == LogAction::kWarn);                      // 10 s: said at once
  CHECK(next_log(state, 100000, low) == LogAction::kNone && next_log(state, 150000, low) == LogAction::kWarn);   // while low: every minute, as a warning
  CHECK(next_log(state, 155000, fine) == LogAction::kNone && next_log(state, 160000, memory_sample(60000, 40000)) == LogAction::kInfo);   // back over 48 KB: said once the gap allows
  LogState late;
  CHECK(next_log(late, 5, low) == LogAction::kWarn);                           // a node that starts low says so at the first look
  LogState backwards;
  backwards.started = true; backwards.last_ms = 500000;
  CHECK(next_log(backwards, 1000, fine) == LogAction::kNone);                  // a clock that went back never makes a flood
  // the line
  CHECK(describe(memory_sample(72704, 36864), 7, 33792) == "memory: internal 71 KB free (least 66 KB, biggest block 36 KB), PSRAM 7700 KB free of 8192 KB, 7 task stacks (33 KB) in PSRAM");
  CHECK(describe(memory_sample(72704, 36864, 0, 0), 0, 0) == "memory: internal 71 KB free (least 66 KB, biggest block 36 KB), no PSRAM");
}

static void test_task_health() {
  const std::vector<health::TaskInfo> tasks{{"main", 2048, 1, 'B'}, {"meters", 300, 5, 'B'}, {"mqtt", 300, 5, 'r'}, {"idle", 900, 0, 'R'}};
  CHECK(health::low_count(tasks) == 2 && health::is_low(tasks[1]) && !health::is_low(tasks[0]) && !health::is_low({"edge", 512, 0, 'B'}) && health::is_low({"edge", 511, 0, 'B'}));
  const std::string text = health::tasks_json(tasks);
  // the least room first, a tie by name
  CHECK(text.find("\"meters\"") < text.find("\"mqtt\"") && text.find("\"mqtt\"") < text.find("\"idle\"") && text.find("\"idle\"") < text.find("\"main\""));
  CHECK(contains(text, "{\"name\":\"meters\",\"stack_free\":300,\"priority\":5,\"state\":\"B\",\"low\":true}") && contains(text, "\"name\":\"main\",\"stack_free\":2048,\"priority\":1,\"state\":\"B\",\"low\":false"));
  CHECK(health::tasks_json({}) == "[]" && health::low_count({}) == 0);
}

// ---- the sessions of the panel ------------------------------------------------------------------------------------------------------------

static void test_remembered_session_is_not_taken_after_half_an_hour() {
  using namespace eh3d::auth;
  SessionTable table;
  const std::string phone(40, 'p');
  table.create(phone, "admin", Role::kAdmin, 1000, /*remembered=*/true);
  // five more people come in 31 minutes later and fill the table (six places); the phone's session is remembered, so it is not idle for thirty days
  const std::uint64_t later = 1000 + 31ULL * 60ULL * 1000ULL;
  for (int i = 0; i < 5; ++i) table.create(std::string(40, static_cast<char>('a' + i)), "user", Role::kAdmin, later);
  CHECK(table.touch(phone, later + 1) != nullptr);      // it is still there
  // a plain session that has been idle too long is the one that goes when the table is full
  SessionTable full;
  for (int i = 0; i < 6; ++i) full.create(std::string(40, static_cast<char>('a' + i)), "user", Role::kAdmin, 1000);
  full.create(std::string(40, 'z'), "new", Role::kAdmin, 1000 + 31ULL * 60ULL * 1000ULL);
  CHECK(full.touch(std::string(40, 'z'), 1000 + 31ULL * 60ULL * 1000ULL + 1) != nullptr);
  // with nothing idle, the one idle the longest is replaced
  SessionTable busy;
  for (int i = 0; i < 6; ++i) busy.create(std::string(40, static_cast<char>('a' + i)), "user", Role::kAdmin, 1000 + static_cast<std::uint64_t>(i) * 10);
  busy.create(std::string(40, 'z'), "new", Role::kAdmin, 2000);
  CHECK(busy.touch(std::string(40, 'a'), 2001) == nullptr && busy.touch(std::string(40, 'b'), 2001) != nullptr);
}

static void test_json_strictness() {
  using namespace eh3d;
  json::Value document;
  // a name repeated in an object is refused: the server's parser keeps the last one and this one the first, so a text could mean two things
  CHECK(!json::parse("{\"a\":1,\"a\":2}", document));
  CHECK(!json::parse("{\"x\":{\"b\":true,\"b\":false}}", document));
  CHECK(!json::parse("[{\"a\":1,\"a\":1}]", document));
  CHECK(json::parse("{\"a\":1,\"b\":{\"a\":2}}", document) && document.get("a")->number == 1);   // the same name at another level is not a repeat
  // a whole number is written whole
  json::Writer w;
  w.begin_array().number(1234567890.0).number(5.0).number(0.25).number(-7.0).number(1e15).number(1e300).end_array();
  CHECK(w.str().rfind("[1234567890,5,0.25,-7,1000000000000000,", 0) == 0);
  json::Value back;
  CHECK(json::parse(w.str(), back) && back.items[0].number == 1234567890.0);
}

static void test_json_refuses_bad_text() {
  using namespace eh3d;
  json::Value v;
  CHECK(json::parse("\"caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80\"", v) && v.text == "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80");   // well-formed UTF-8 passes through
  CHECK(!json::parse("\"\xC3\x28\"", v));            // a lead byte without its continuation
  CHECK(!json::parse("\"\xC0\x80\"", v));            // an overlong NUL
  CHECK(!json::parse("\"\xE0\x80\x80\"", v));        // an overlong three-byte form
  CHECK(!json::parse("\"\xED\xA0\x80\"", v));        // a surrogate written raw
  CHECK(!json::parse("\"\xF4\x90\x80\x80\"", v));    // beyond U+10FFFF
  CHECK(!json::parse("\"\xFF\"", v) && !json::parse("\"\xE2\x82\"", v));   // a byte that never occurs, a sequence cut short
  std::string many = "[";
  for (int i = 0; i < 6000; ++i) many += i == 0 ? "0" : ",0";
  many += "]";
  CHECK(!json::parse(many, v));                           // thousands of values from a few kilobytes of text
  std::string few = "[";
  for (int i = 0; i < 1000; ++i) few += i == 0 ? "0" : ",0";
  few += "]";
  CHECK(json::parse(few, v) && v.items.size() == 1000);
  // a large number keeps its digits when it is written and read back
  json::Writer w;
  w.begin_array().number(1234567890.5).number(12345678901234567890.0).number(0.1).end_array();
  json::Value back;
  CHECK(json::parse(w.str(), back) && back.items[0].number == 1234567890.5 && back.items[1].number == 12345678901234567890.0 && back.items[2].number == 0.1);
}

static void test_login_throttle_keeps_its_blocks() {
  using namespace eh3d::auth;
  LoginThrottle throttle;
  // every place of the table gets a blocked address ...
  for (std::uint32_t i = 0; i < 32; ++i) for (int j = 0; j < 5; ++j) throttle.failure(100 + i, 1000 + j);
  for (std::uint32_t i = 0; i < 32; ++i) CHECK(!throttle.allowed(100 + i, 2000));
  // ... one more address does not push one of them out to make room: it waits too, and the blocks stand
  CHECK(!throttle.allowed(900, 2000) && throttle.wait_s(900, 2000) > 0);
  for (int j = 0; j < 5; ++j) throttle.failure(900, 2001 + j);   // (a caller that did not look at allowed() gains nothing)
  for (std::uint32_t i = 0; i < 32; ++i) CHECK(!throttle.allowed(100 + i, 2000));
  // when a block ends the place is free again, for the address that is waiting
  CHECK(throttle.allowed(900, 1004 + 30000) && throttle.wait_s(900, 1004 + 30000) == 0);
  throttle.failure(900, 1004 + 30000);
  CHECK(throttle.allowed(900, 1004 + 30001));
  // an address that was only failing a little is the one that goes when the table is full
  LoginThrottle small;
  for (std::uint32_t i = 0; i < 32; ++i) small.failure(200 + i, 10 + i);
  small.failure(300, 100);
  CHECK(small.allowed(200, 101) && small.allowed(300, 101));
}

// Settings kept in flash by another version of the firmware: read as far as they can be, never thrown away whole.
namespace eh3d::config {
namespace {
struct FakeItem { std::string name; int level = 1; };
struct FakeSettings {
  std::string label = "first";
  int count = 5;
  bool on = false;
  std::vector<FakeItem> items;
};
struct FakeProblem { std::string path, code; };
using FakeProblems = std::vector<FakeProblem>;
void fake_text(const json::Value& parent, const char* name, std::string& target, std::size_t longest, const std::string& path, FakeProblems& problems) {
  const json::Value* member = parent.get(name);
  if (member == nullptr) return;
  if (!member->is_string()) { problems.push_back({path, "invalid"}); return; }
  if (member->text.size() > longest) { problems.push_back({path, "too_long"}); return; }
  target = member->text;
}
void fake_int(const json::Value& parent, const char* name, int& target, int lowest, int highest, const std::string& path, FakeProblems& problems) {
  const json::Value* member = parent.get(name);
  if (member == nullptr) return;
  if (!member->is_number()) { problems.push_back({path, "invalid"}); return; }
  if (member->number < lowest || member->number > highest) { problems.push_back({path, "range"}); return; }
  target = static_cast<int>(member->number);
}
void read_settings(const json::Value& document, FakeSettings& s, FakeProblems& problems) {
  fake_text(document, "label", s.label, 8, "label", problems);
  fake_int(document, "count", s.count, 1, 10, "count", problems);
  if (const json::Value* member = document.get("on"); member != nullptr) {
    if (member->is_bool()) s.on = member->boolean; else problems.push_back({"on", "invalid"});
  }
  if (const json::Value* list = document.get("items"); list != nullptr && list->is_array()) {
    s.items.clear();
    for (std::size_t i = 0; i < list->items.size(); ++i) {
      FakeItem item;
      fake_text(list->items[i], "name", item.name, 4, "items." + std::to_string(i) + ".name", problems);
      fake_int(list->items[i], "level", item.level, 1, 3, "items." + std::to_string(i) + ".level", problems);
      s.items.push_back(item);
    }
  }
}
FakeProblems validate(const FakeSettings& s) {
  FakeProblems problems;
  if (s.on && s.count == 7) problems.push_back({"count", "conflict"});
  return problems;
}
}  // namespace
}  // namespace eh3d::config

static void test_stored_settings_survive_an_update() {
  using namespace eh3d::config;
  const FakeSettings base;
  FakeSettings out;
  FakeProblems dropped;
  // an intact document, with a member this version has never heard of
  CHECK(load_stored(R"({"label":"mine","count":9,"on":true,"newer":{"x":[1,2,3]}})", base, out, dropped) && out.label == "mine" && out.count == 9 && out.on && dropped.empty());
  // a member that is missing stays at its first value
  CHECK(load_stored(R"({"on":true})", base, out, dropped) && out.label == "first" && out.count == 5 && out.on && dropped.empty());
  // a value too long, out of range or of the wrong type loses only itself
  CHECK(load_stored(R"({"label":"far too long","count":99,"on":"yes"})", base, out, dropped) && out.label == "first" && out.count == 5 && !out.on && dropped.size() == 3);
  CHECK(load_stored(R"({"label":"kept","count":99})", base, out, dropped) && out.label == "kept" && out.count == 5);
  // inside a list: the field goes, the item and the others stay
  CHECK(load_stored(R"({"items":[{"name":"a","level":2},{"name":"toolong","level":9},{"name":"c","level":3}]})", base, out, dropped));
  CHECK(out.items.size() == 3 && out.items[0].name == "a" && out.items[0].level == 2 && out.items[1].name == "" && out.items[1].level == 1 && out.items[2].level == 3 && dropped.size() == 2);
  // a conflict between two values found by the check is resolved the same way
  CHECK(load_stored(R"({"count":7,"on":true,"label":"x"})", base, out, dropped) && out.label == "x" && out.count == 5 && out.on);
  // a document that is not JSON, or empty, cannot be read: the caller starts from its first settings
  CHECK(!load_stored("not json", base, out, dropped) && dropped.size() == 1 && dropped[0].code == "not_json");
  CHECK(!load_stored("", base, out, dropped));
  // a top-level value that is not an object leaves everything at its first value
  CHECK(load_stored("[1,2]", base, out, dropped) && out.label == "first" && dropped.empty());
  // a huge list of bad fields is bounded, not looped on
  std::string many = "{\"items\":[";
  for (int i = 0; i < 300; ++i) many += std::string(i ? "," : "") + "{\"level\":99}";
  many += "]}";
  const bool loaded = load_stored(many, base, out, dropped);
  CHECK(loaded || dropped.size() <= 128);
}

static void test_login_limit_per_minute() {
  using namespace eh3d::auth;
  LoginThrottle limit;
  for (unsigned i = 0; i < LoginThrottle::kMinuteFailures; ++i) {
    CHECK(limit.minute_allowed(1000 + i * 100));
    limit.minute_failure(1000 + i * 100);   // thirty different sources, one wrong password each
  }
  const std::uint64_t full = 1000 + 29 * 100;
  CHECK(!limit.minute_allowed(full + 1) && limit.minute_wait_s(full + 1) >= 57 && limit.minute_wait_s(full + 1) <= 60);
  CHECK(!limit.minute_allowed(1000 + 60000 - 1));
  CHECK(limit.minute_allowed(1000 + 60000) && limit.minute_wait_s(1000 + 60000) == 0);   // the oldest has left the minute: one more may try
  limit.minute_failure(1000 + 60000);
  CHECK(!limit.minute_allowed(1000 + 60000 + 1) && limit.minute_wait_s(1000 + 60000 + 1) >= 1);
  CHECK(limit.minute_allowed(1100 + 60000));
  LoginThrottle idle;   // a node that was never attacked never waits
  CHECK(idle.minute_allowed(0) && idle.minute_wait_s(5) == 0);
  for (int i = 0; i < 29; ++i) idle.minute_failure(10);
  CHECK(idle.minute_allowed(11));   // 29 wrong passwords in a minute are still allowed
  idle.minute_failure(11);
  CHECK(!idle.minute_allowed(12));
}

// ---- the networks that may use the panel -------------------------------------------------------------------------------------------------

static std::uint32_t ip(const char* text) { std::uint32_t value = 0; const bool ok = adminsrc::parse_address(text, value); CHECK(ok); return value; }

static void test_user_key() {
  CHECK(auth::user_key("admin") == auth::user_key("ADMIN") && auth::user_key("admin") == auth::user_key("Admin"));
  CHECK(auth::user_key("admin") != auth::user_key("admin2") && auth::user_key("") != 0 && auth::user_key("admin") != 0);
  // wrong passwords for one person block that person from every address, and another person is not touched
  auth::LoginThrottle by_person;
  for (int i = 0; i < 5; ++i) by_person.failure(auth::user_key("admin"), 1000);
  CHECK(!by_person.allowed(auth::user_key("Admin"), 1001));
  CHECK(by_person.allowed(auth::user_key("visitor"), 1001));
  by_person.success(auth::user_key("admin"));
  CHECK(by_person.allowed(auth::user_key("admin"), 1002));
}

static void test_admin_sources() {
  using adminsrc::allowed;
  { std::uint32_t unused = 0; CHECK(!adminsrc::parse_address("010.1.1.1", unused) && !adminsrc::parse_address("1.2.3.04", unused) && !adminsrc::parse_address("00.1.1.1", unused) && adminsrc::parse_address("0.1.1.0", unused)); }   // same rule as net::parse_ipv4
  // the default is the house: the private ranges, the link-local ones, the loopback and the range of Tailscale; the Internet is not
  for (const char* inside : {"10.1.2.3", "172.16.0.1", "172.31.255.254", "192.168.0.50", "169.254.3.4", "127.0.0.1", "100.64.0.9", "100.127.255.1"}) CHECK(allowed(ip(inside), ""));
  for (const char* outside : {"8.8.8.8", "203.0.113.9", "172.15.0.1", "172.32.0.1", "192.169.0.1", "11.0.0.1"}) CHECK(!allowed(ip(outside), ""));
  CHECK(!allowed(0, ""));   // an address the node could not read is never allowed
  CHECK(allowed(ip("192.168.0.50"), "  "));
  // a list replaces the default: only what it names
  CHECK(allowed(ip("203.0.113.9"), "203.0.113.9"));
  CHECK(!allowed(ip("203.0.113.10"), "203.0.113.9"));
  CHECK(allowed(ip("192.168.1.77"), "192.168.1.0/24, 10.66.0.0/24"));
  CHECK(!allowed(ip("192.168.2.77"), "192.168.1.0/24, 10.66.0.0/24"));
  CHECK(allowed(ip("10.66.0.2"), "192.168.1.0/24,10.66.0.0/24"));
  CHECK(allowed(ip("192.168.1.77"), "192.168.1.5/24"));   // the base is cut to the prefix
  CHECK(allowed(ip("1.2.3.4"), "0.0.0.0/0"));            // everybody, if the owner wants it
  CHECK(!allowed(ip("192.168.0.50"), "203.0.113.9"));    // naming an outside address does not keep the house
  // what is not a list is refused, and nothing is let in by a list that cannot be read
  for (const char* bad : {"192.168.0", "192.168.0.256", "192.168.0.1/33", "192.168.0.1/", "192.168.0.1/-1", "a.b.c.d", "192.168.0.1,,10.0.0.0/8", ",10.0.0.0/8", "10.0.0.0/8,", "10.0.0.1.1", "10.0.0.1 /8", "192.168.0.1/ab"}) CHECK(!adminsrc::valid_list(bad));
  CHECK(adminsrc::valid_list("") && adminsrc::valid_list("10.0.0.0/8") && adminsrc::valid_list(" 10.0.0.0/8 , 192.168.0.1 "));
  CHECK(!adminsrc::valid_list("203.0.113.1,203.0.113.2,203.0.113.3,203.0.113.4,203.0.113.5,203.0.113.6,203.0.113.7,203.0.113.8,203.0.113.9"));   // nine entries: too many
  CHECK(!adminsrc::valid_list(std::string(130, '1')));
  CHECK(allowed(ip("192.168.0.50"), "garbage") && !allowed(ip("8.8.8.8"), "garbage"));   // a list that cannot be read falls back to the house, never to "everybody"
}

// The node's watchdog: a minute of trouble before it acts, one clean restart, never more than three in an hour, and an alarm that repeats when it cannot restart.
static eh3d::nodeguard::Inputs guard_look(std::uint64_t seconds, std::uint32_t internal_free, unsigned missing = 0) {
  eh3d::nodeguard::Inputs in;
  in.now_ms = seconds * 1000ULL;
  in.internal_free = internal_free;
  in.internal_largest = internal_free / 2;
  in.missing_tasks = missing;
  return in;
}

static void test_node_guard_rules() {
  using namespace eh3d::nodeguard;
  const std::uint32_t fine = 60u * 1024u, low = 12u * 1024u;
  { State s; CHECK(step(s, guard_look(0, fine), true) == Action::kNone && !s.trouble); }
  { Inputs unreadable; CHECK(!memory_is_low(unreadable, false)); }   // a heap that could not be read is not trouble
  // a task that is missing for a minute: told, and the node restarts once
  { State s;
    CHECK(step(s, guard_look(0, fine, 1), true) == Action::kNone);
    CHECK(step(s, guard_look(30, fine, 1), true) == Action::kNone);
    CHECK(step(s, guard_look(59, fine, 1), true) == Action::kNone);
    CHECK(step(s, guard_look(60, fine, 1), true) == Action::kAlertAndRestart);
    CHECK(step(s, guard_look(65, fine, 1), true) == Action::kNone);   // said once: not again while it lasts
  }
  // trouble that ends before the minute is over is forgotten
  { State s;
    step(s, guard_look(0, low), true);
    CHECK(step(s, guard_look(40, low), true) == Action::kNone);
    CHECK(step(s, guard_look(50, fine), true) == Action::kNone && !s.trouble);
    step(s, guard_look(55, low), true);
    CHECK(step(s, guard_look(100, low), true) == Action::kNone);   // the minute counts from the second time
    CHECK(step(s, guard_look(115, low), true) == Action::kAlertAndRestart);
  }
  // the memory is low under 16 KB and back to normal only over 24 KB (a figure that hovers around the line does not flap)
  { State s;
    step(s, guard_look(0, 15u * 1024u), true);
    CHECK(s.memory_low);
    step(s, guard_look(5, 20u * 1024u), true);
    CHECK(s.memory_low && s.trouble);
    step(s, guard_look(10, 25u * 1024u), true);
    CHECK(!s.memory_low && !s.trouble);
  }
  // at the limit of restarts the node stays up and repeats the alarm every ten minutes
  { State s;
    step(s, guard_look(0, low), false);
    CHECK(step(s, guard_look(60, low), false) == Action::kAlert);
    CHECK(step(s, guard_look(120, low), false) == Action::kNone);
    CHECK(step(s, guard_look(60 + 599, low), false) == Action::kNone);
    CHECK(step(s, guard_look(60 + 600, low), false) == Action::kAlert);
    CHECK(step(s, guard_look(60 + 601, low), false) == Action::kNone);
  }
  // what it says is short and names what is missing
  { std::vector<std::string> none, some{"telemetry", "heartbeat"}, many{"a", "b", "c", "d"};
    CHECK(reason_text(some, guard_look(0, fine), false) == "guard: tasks telemetry,heartbeat missing");
    CHECK(reason_text(none, guard_look(0, 12u * 1024u), true) == "guard: internal memory 12 KB for over a minute");
    CHECK(reason_text(many, guard_look(0, 12u * 1024u), true).size() <= 90);
    CHECK(reason_text(std::vector<std::string>{"x"}, guard_look(0, fine), false) == "guard: task x missing");
  }
}

static void test_node_guard_restart_ledger() {
  using namespace eh3d::nodeguard;
  Ledger ledger;
  CHECK(ledger.may_restart(100));
  ledger.note_restart(100);            // boot 1 ran 100 s
  CHECK(ledger.pending && ledger.recent(0) == 1);
  ledger.pending = false;
  ledger.note_restart(200);            // boot 2 ran 200 s: the running clock is at 300
  ledger.pending = false;
  CHECK(ledger.clock(0) == 300 && ledger.recent(0) == 2 && ledger.may_restart(0));
  ledger.note_restart(300);            // boot 3 ran 300 s: the clock is at 600
  CHECK(ledger.recent(0) == 3 && !ledger.may_restart(0));   // the fourth is not allowed within the hour
  CHECK(!ledger.may_restart(3000));    // the clock is at 3600: the first restart (at 100) is still inside the hour
  CHECK(ledger.may_restart(3100));     // 3700: it is out, there is room for one
  ledger.note_restart(3100);
  CHECK(ledger.count <= kMaxRestartsPerHour && ledger.recent(0) <= kMaxRestartsPerHour);
  // never more than three in any hour of running time, however often it asks: a boot of 50 s asks again at once
  { Ledger l; unsigned granted = 0;
    for (int i = 0; i < 200; ++i) {
      if (l.may_restart(50)) { l.note_restart(50); ++granted; }
      else l.base_s += 50;   // the boot ran on without restarting
    }
    CHECK(granted >= 3 && granted <= 3u + 200u * 50u / 3600u * 3u + 3u);
    CHECK(l.recent(0) <= kMaxRestartsPerHour);
  }
}

static void test_node_guard_survives_a_restart() {
  using namespace eh3d::nodeguard;
  Ledger saved;
  saved.note_restart(500);
  BootInfo info;
  // a software reset keeps the memory: the next boot knows the guard asked for it, and reads that once
  Ledger next = ledger_at_boot(saved, ledger_check(saved), true, info);
  CHECK(info.by_guard && info.restarts_in_hour == 1 && !next.pending && next.count == 1);
  Ledger after = ledger_at_boot(next, ledger_check(next), true, info);
  CHECK(!info.by_guard && after.count == 1);   // the boot after that is an ordinary one
  // a power-on leaves garbage: it is not a ledger, however it looks
  Ledger garbage = saved;
  ledger_at_boot(garbage, ledger_check(garbage), false, info);
  CHECK(!info.by_guard && info.restarts_in_hour == 0);
  Ledger damaged = saved;
  ledger_at_boot(damaged, ledger_check(damaged) + 1, true, info);   // the check number does not fit
  CHECK(!info.by_guard);
  Ledger absurd = saved;
  absurd.count = 99;
  ledger_at_boot(absurd, ledger_check(absurd), true, info);
  CHECK(!info.by_guard);
}



// The gate at the door of the panel, the failed allocation, the dead-man switch and the probation of a new image.
static void test_node_guard_gate_and_streams() {
  using namespace eh3d::nodeguard;
  const std::uint32_t kb = 1024;
  Gate gate;
  CHECK(!gate.step(0, 0) && !gate.closed);                 // a heap that could not be read leaves it open
  CHECK(!gate.step(60 * kb, 30 * kb) && !gate.closed);
  CHECK(gate.step(27 * kb, 30 * kb) && gate.closed);       // under 28 KB free: closed
  CHECK(!gate.step(35 * kb, 30 * kb) && gate.closed);      // between the two levels it stays closed (no flapping)
  CHECK(!gate.step(45 * kb, 14 * kb) && gate.closed);      // plenty free but the biggest block is still under 16 KB
  CHECK(gate.step(45 * kb, 20 * kb) && !gate.closed);      // over 40 KB and 16 KB: open again
  CHECK(gate.step(60 * kb, 11 * kb) && gate.closed);       // a heap in pieces closes it although the sum looks fine
  CHECK(gate.transitions == 3);
  CHECK(!gate.step(0, 0) && gate.closed);
  GateThresholds odd;
  odd.open_free = 1;       // opens "below" where it closes: put right, the gate must still close and open
  odd.open_block = 1;
  Gate other;
  CHECK(other.step(10 * kb, 30 * kb, odd) && other.closed);
  CHECK(other.step(odd.sane().open_free + 1, 30 * kb, odd) && !other.closed);
  // streams: capped, and none while the gate is closed
  CHECK(may_open_stream(0, false) && may_open_stream(3, false) && !may_open_stream(4, false) && !may_open_stream(0, true) && may_open_stream(1, false, 2) && !may_open_stream(2, false, 2));
  CHECK(gate_text(true, 24 * kb, 10 * kb) == "guard: no new panel connections while the memory is short (internal 24/10 KB)");
  CHECK(gate_text(false, 41 * kb, 20 * kb).find("accepted again") != std::string::npos);
}

static void test_node_guard_out_of_memory() {
  using namespace eh3d::nodeguard;
  OomRecord none;
  CHECK(!oom_valid(none));
  const OomRecord first = make_oom(9000, 3000, 120, 500);
  CHECK(oom_valid(first) && oom_text(first) == "oom heap=9000/3000 after 120 s");
  OomRecord damaged = first;
  damaged.free_bytes = 1;
  CHECK(!oom_valid(damaged));
  CHECK(oom_decide(none, 500, false) == OomAction::kRestart);
  CHECK(oom_decide(none, 500, true) == OomAction::kAbort);                 // while an image is written: abort, as before
  CHECK(oom_decide(first, 500 + 59, false) == OomAction::kAbort);          // a second one within a minute of running time: abort, the loop stays visible
  CHECK(oom_decide(first, 500 + 60, false) == OomAction::kRestart);
  CHECK(oom_decide(damaged, 501, false) == OomAction::kRestart);           // a record that does not check out is not a record
}

static void test_node_guard_probation() {
  using namespace eh3d::nodeguard;
  CHECK(!image_may_be_confirmed(59, true, true) && image_may_be_confirmed(60, true, true) && !image_may_be_confirmed(300, false, true) && !image_may_be_confirmed(300, true, false));
  CHECK(!settings_reverted(7, 7) && settings_reverted(7, 8));
}

int main() {
  test_node_guard_gate_and_streams();
  test_node_guard_out_of_memory();
  test_node_guard_probation();
  test_node_guard_rules();
  test_node_guard_restart_ledger();
  test_node_guard_survives_a_restart();
  test_admin_sources();
  test_user_key();
  test_json_strictness();
  test_json_refuses_bad_text();
  test_login_throttle_keeps_its_blocks();
  test_login_limit_per_minute();
  test_stored_settings_survive_an_update();
  test_remembered_session_is_not_taken_after_half_an_hour();
  test_backoff();
  test_improv_packets();
  test_improv_parser();
  test_improv_rpc();
  test_task_health();
  test_memory_budget();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
