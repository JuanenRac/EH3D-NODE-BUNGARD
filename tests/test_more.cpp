// EH3D-NODE-BUNGARD - more host tests of the small rules the node is built from: the queue of bytes, versions, node names, the calendar, the finish of a job, the
// retries of a send, the addresses the web server answers, the frames of the control link read from a noisy line and the text rules of the network settings.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/byte_queue.hpp"
#include "../core/civil_time.hpp"
#include "../core/drain.hpp"
#include "../core/frame.hpp"
#include "../core/json.hpp"
#include "../core/net_text.hpp"
#include "../core/node_id.hpp"
#include "../core/node_settings.hpp"
#include "../core/release_assets.hpp"
#include "../core/semver.hpp"
#include "../core/send_loop.hpp"
#include "../core/web_policy.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                \
  do {                                                                  \
    ++checks;                                                           \
    if (!(condition)) {                                                 \
      ++failures;                                                       \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);  \
    }                                                                   \
  } while (0)

using namespace eh3d;

// ---- the queue of bytes ----------------------------------------------------------------------------------------------------------------

static void queue_keeps_order_across_the_end_of_its_buffer() {
  bungard::ByteQueue q(8);
  const std::uint8_t first[6] = {1, 2, 3, 4, 5, 6};
  CHECK(q.push(first, 6) && q.size() == 6 && q.free() == 2);
  std::uint8_t out[16] = {};
  CHECK(q.pop(out, 4) == 4 && out[0] == 1 && out[3] == 4);
  const std::uint8_t second[6] = {7, 8, 9, 10, 11, 12};   // this one wraps around the end of the buffer
  CHECK(q.push(second, 6) && q.size() == 8 && q.free() == 0);
  CHECK(q.pop(out, 16) == 8);
  const std::uint8_t expected[8] = {5, 6, 7, 8, 9, 10, 11, 12};
  for (int i = 0; i < 8; ++i) CHECK(out[i] == expected[i]);
  CHECK(q.size() == 0 && q.pop(out, 4) == 0);
}

static void queue_takes_a_push_whole_or_not_at_all() {
  bungard::ByteQueue q(4);
  const std::uint8_t data[5] = {9, 8, 7, 6, 5};
  CHECK(q.push(data, 3));
  CHECK(!q.push(data, 2) && q.size() == 3);     // two do not fit in one place: nothing of them goes in
  CHECK(!q.push(data, 5) && q.size() == 3);
  std::uint8_t out[4] = {};
  CHECK(q.pop(out, 4) == 3 && out[0] == 9 && out[1] == 8 && out[2] == 7);
  CHECK(q.push(data, 4) && !q.push(data, 1));   // exactly full
}

static void queue_edges() {
  bungard::ByteQueue none(0);
  CHECK(none.capacity() == 0 && none.free() == 0);
  std::uint8_t b = 1;
  CHECK(none.push(&b, 0) && !none.push(&b, 1) && none.pop(&b, 1) == 0);
  bungard::ByteQueue q(3);
  const std::uint8_t d[3] = {1, 2, 3};
  CHECK(q.push(d, 3));
  std::uint8_t out[3] = {};
  CHECK(q.pop(out, 0) == 0 && q.size() == 3);
  q.clear();
  CHECK(q.size() == 0 && q.free() == 3 && q.push(d, 3) && q.pop(out, 3) == 3 && out[0] == 1 && out[2] == 3);
}

// ---- versions and names ----------------------------------------------------------------------------------------------------------------

static void plain_versions_only() {
  for (const char* good : {"0.0.1", "v1.2.3", "0.7.9.3", "10.20.30.40", "999999.999999.999999.999999", "1.0.0"}) CHECK(semver::parse(good).ok);
  for (const char* bad : {"", "v", "1", "1.2", "1.2.3.4.5", "1..2", ".1.2", "1.2.", "1.2.3.", "1.2.3-rc1", "1.2.3+b5", "a.b.c", "V1.2.3", " 1.2.3", "1.2.3 ", "vv1.2.3", "1234567.1.1", "1.2.3.x"})
    CHECK(!semver::parse(bad).ok);
  const semver::Version v = semver::parse("v3.14.15.92");
  CHECK(v.major == 3 && v.minor == 14 && v.patch == 15 && v.build == 92);
  CHECK(semver::parse("1.2.3").build == 0);
}

static void newer_is_by_number_not_by_text() {
  CHECK(semver::is_newer("0.10.0", "0.9.0") && !semver::is_newer("0.9.0", "0.10.0"));
  CHECK(semver::is_newer("0.8.0.1", "0.8.0") && !semver::is_newer("0.8.0", "0.8.0.1"));
  CHECK(!semver::is_newer("0.8.0", "0.8.0.0") && !semver::is_newer("0.8.0.0", "0.8.0"));   // the same version written two ways
  CHECK(semver::is_newer("1.0.0", "0.99.99.99") && semver::is_newer("0.9.0", "0.8.9.9"));
  CHECK(!semver::is_newer("2.0.0-rc1", "1.0.0") && !semver::is_newer("1.0.0", "garbage") && !semver::is_newer("garbage", "1.0.0"));
  const semver::Version a = semver::parse("1.2.3"), b = semver::parse("1.2.3.1");
  CHECK(!semver::less(a, a) && semver::less(a, b) && !semver::less(b, a));
}

static void node_names() {
  for (const char* good : {"a", "node-1", "a_b", "0abc", "bungard-aux-01"}) CHECK(node_id_is_valid(good));
  CHECK(node_id_is_valid(std::string(64, 'a')) && !node_id_is_valid(std::string(65, 'a')));
  for (const char* bad : {"", "-a", "_a", "A", "Node", "a b", "a.b", "a/b", "a\\b", "\xC3\xB1"}) CHECK(!node_id_is_valid(bad));
  CHECK(!node_id_is_valid(std::string_view("a\0b", 3)));
}

// ---- the calendar ----------------------------------------------------------------------------------------------------------------------

static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int days_in(int y, int m) { static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; return m == 2 && leap(y) ? 29 : d[m - 1]; }

static void every_day_from_1899_to_2101_is_one_more_than_the_one_before() {
  int y = 1899, m = 12, d = 31;
  std::int64_t before = civil::days_from_civil(y, m, d);
  bool in_step = true;
  while (!(y == 2101 && m == 1 && d == 1)) {
    if (++d > days_in(y, m)) { d = 1; if (++m > 12) { m = 1; ++y; } }
    const std::int64_t now = civil::days_from_civil(y, m, d);
    if (now != before + 1) in_step = false;
    before = now;
  }
  CHECK(in_step);
}

static void known_moments() {
  CHECK(civil::days_from_civil(1970, 1, 1) == 0 && civil::days_from_civil(1969, 12, 31) == -1 && civil::days_from_civil(1970, 3, 1) == 59);
  CHECK(civil::days_from_civil(2000, 2, 29) == 11016);            // 2000 is a leap year
  CHECK(civil::days_from_civil(2100, 3, 1) - civil::days_from_civil(2100, 2, 28) == 1);   // 2100 is not
  CHECK(civil::seconds_as_utc(2038, 1, 19, 3, 14, 7) == 2147483647LL);
  CHECK(civil::seconds_as_utc(1969, 12, 31, 23, 59, 59) == -1);
  CHECK(civil::seconds_as_utc(2024, 2, 29, 12, 0, 0) - civil::seconds_as_utc(2024, 2, 28, 12, 0, 0) == 86400);
}

static void the_offset_of_a_time_zone_from_the_local_and_the_utc_clock() {
  const std::int64_t utc = civil::seconds_as_utc(2024, 6, 1, 10, 0, 0);
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 12, 0, 0) == 120);
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 5, 0, 0) == -300);
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 15, 45, 0) == 345);       // Nepal
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 10, 0, 0) == 0);
  const std::int64_t late = civil::seconds_as_utc(2024, 12, 31, 23, 30, 0);
  CHECK(civil::utc_offset_minutes(late, 2025, 1, 1, 0, 30, 0) == 60);        // the local day is already the next year
}

// ---- finishing the job --------------------------------------------------------------------------------------------------------------------

struct Fake : bungard::DrainHardware {
  bool is_busy = true;
  int stops = 0;
  bool busy() override { return is_busy; }
  void stop_all() override { ++stops; is_busy = false; }
};

static void drain_waits_for_the_machine_and_stops_it_once() {
  Fake hw;
  bungard::Drain d;
  d.configure(1000);
  hw.is_busy = false;
  CHECK(!d.begin(10, hw) && !d.active());                  // nothing to finish
  hw.is_busy = true;
  CHECK(d.begin(10, hw) && d.active());
  CHECK(d.begin(500, hw));                                  // asking again does not start the clock over
  d.tick(999, hw);
  CHECK(d.active() && hw.stops == 0);
  d.tick(1010, hw);                                         // 1000 ms after the first begin
  CHECK(!d.active() && hw.stops == 1);
  d.tick(5000, hw);
  CHECK(hw.stops == 1);                                     // an idle drain does nothing
  CHECK(d.begin(6000, hw) == false);                        // stop_all left the hardware idle
}

static void drain_ends_when_the_machine_goes_idle_and_survives_the_wrap_of_the_clock() {
  Fake hw;
  bungard::Drain d;
  d.configure(600);
  CHECK(d.begin(0xFFFFFF00u, hw));
  d.tick(0x00000100u, hw);                                  // 512 ms later, across the wrap
  CHECK(d.active() && hw.stops == 0);
  d.tick(0x00000200u, hw);                                  // 768 ms
  CHECK(!d.active() && hw.stops == 1);
  hw.is_busy = true;
  CHECK(d.begin(100, hw));
  hw.is_busy = false;                                       // the job ended by itself
  d.tick(150, hw);
  CHECK(!d.active() && hw.stops == 2);
}

static void drain_abort_and_zero_time() {
  Fake hw;
  bungard::Drain d;
  d.abort(hw);
  CHECK(hw.stops == 0);                                     // aborting what is not running stops nothing
  CHECK(d.begin(0, hw));
  d.abort(hw);
  CHECK(!d.active() && hw.stops == 1);
  d.configure(0);
  hw.is_busy = true;
  CHECK(d.begin(7, hw));
  d.tick(7, hw);
  CHECK(!d.active() && hw.stops == 2);
}

// ---- sending with a limit --------------------------------------------------------------------------------------------------------------

static void send_goes_on_after_partial_sends_and_gives_up_at_the_limit() {
  std::uint8_t data[10] = {};
  std::uint32_t clock = 0;
  int tries = 0, pauses = 0;
  const auto now = [&]() { return clock; };
  const auto pause = [&]() { ++pauses; clock += 10; };
  // three bytes at a time
  std::size_t left = 10;
  CHECK(bungard::send_within(data, 10, 100, [&](const std::uint8_t*, std::size_t n) { ++tries; const long s = n >= 3 ? 3 : static_cast<long>(n); left -= static_cast<std::size_t>(s); return bungard::SendTry{s, false}; }, now, pause));
  CHECK(tries == 4 && left == 0 && pauses == 0);
  // a full buffer that never frees: tries until the limit, pausing between them
  tries = 0; pauses = 0; clock = 0;
  CHECK(!bungard::send_within(data, 4, 25, [&](const std::uint8_t*, std::size_t) { ++tries; return bungard::SendTry{0, true}; }, now, pause));
  CHECK(tries == 4 && pauses == 3);
  // it frees in time
  tries = 0; pauses = 0; clock = 0;
  CHECK(bungard::send_within(data, 4, 25, [&](const std::uint8_t*, std::size_t n) { return ++tries < 3 ? bungard::SendTry{0, true} : bungard::SendTry{static_cast<long>(n), false}; }, now, pause));
  CHECK(pauses == 2);
}

static void send_fails_at_once_on_an_error_and_has_nothing_to_do_for_nothing() {
  std::uint8_t data[2] = {};
  std::uint32_t clock = 0;
  int tries = 0;
  const auto now = [&]() { return clock; };
  const auto pause = [&]() { clock += 10; };
  CHECK(!bungard::send_within(data, 2, 1000, [&](const std::uint8_t*, std::size_t) { ++tries; return bungard::SendTry{-1, false}; }, now, pause) && tries == 1);
  CHECK(!bungard::send_within(data, 2, 1000, [&](const std::uint8_t*, std::size_t) { ++tries; return bungard::SendTry{0, false}; }, now, pause) && tries == 2);
  CHECK(bungard::send_within(data, 0, 0, [&](const std::uint8_t*, std::size_t) { ++tries; return bungard::SendTry{-1, false}; }, now, pause) && tries == 2);
  // a limit of nothing allows one try and no wait
  CHECK(!bungard::send_within(data, 2, 0, [&](const std::uint8_t*, std::size_t) { ++tries; return bungard::SendTry{0, true}; }, now, pause) && tries == 3);
  // the wrap of the clock does not make a long wait out of a short one
  clock = 0xFFFFFFF0u;
  tries = 0;
  CHECK(bungard::send_within(data, 2, 100, [&](const std::uint8_t*, std::size_t n) { return ++tries < 4 ? bungard::SendTry{0, true} : bungard::SendTry{static_cast<long>(n), false}; }, now, pause));
}

// ---- the web server ------------------------------------------------------------------------------------------------------------------------

static void the_host_of_a_request_is_cleaned_or_refused() {
  CHECK(webpolicy::clean_host("example.com") == "example.com");
  CHECK(webpolicy::clean_host("example.com:8080") == "example.com");
  CHECK(webpolicy::clean_host("192.168.0.5:80") == "192.168.0.5");
  CHECK(webpolicy::clean_host("Node-1.local") == "Node-1.local");
  CHECK(webpolicy::clean_host("host:80:90") == "host");
  for (const char* bad : {"", ":80", "a b", "-a.com", ".a", "a-", "a_b", "[::1]:80", "host\r\nX: 1", "b\xC3\xBC" "cher.de", "a/b", "a@b"}) CHECK(webpolicy::clean_host(bad).empty());
  CHECK(webpolicy::clean_host(std::string(253, 'a')).size() == 253 && webpolicy::clean_host(std::string(254, 'a')).empty());
}

static void a_redirect_goes_to_https_on_the_same_host_and_never_somewhere_else() {
  CHECK(webpolicy::redirect_location("node.local", "/") == "https://node.local/");
  CHECK(webpolicy::redirect_location("node.local:80", "/a/b?x=1&y=2") == "https://node.local/a/b?x=1&y=2");
  for (const char* uri : {"", "x", "//evil.com", "/\\evil.com", "/a b", "/a\r\nSet-Cookie: x=1", "/\xC3\xBC", "/\x7f"}) CHECK(webpolicy::redirect_location("node.local", uri).empty());
  CHECK(webpolicy::redirect_location("bad host", "/").empty() && webpolicy::redirect_location("", "/").empty());
}

static void the_cookie_is_secure_over_tls_and_never_readable_by_scripts() {
  CHECK(webpolicy::cookie_attributes(true, 60) == "Path=/; HttpOnly; SameSite=Strict; Secure; Max-Age=60");
  CHECK(webpolicy::cookie_attributes(false, 0) == "Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  CHECK(webpolicy::cookie_attributes(false, -1).find("Max-Age=-1") != std::string::npos);
  CHECK(webpolicy::serves_https(config::WebMode::kBoth) && webpolicy::serves_https(config::WebMode::kHttps) && !webpolicy::serves_https(config::WebMode::kHttp));
  CHECK(webpolicy::http_redirects(config::WebMode::kHttps) && !webpolicy::http_redirects(config::WebMode::kBoth) && !webpolicy::http_redirects(config::WebMode::kHttp));
}

// ---- the updates from GitHub -----------------------------------------------------------------------------------------------------------

static const char* kRepo = "JuanenRac/EH3D-NODE-BUNGARD";
static const std::string kBase = "https://github.com/JuanenRac/EH3D-NODE-BUNGARD/releases/download/v0.0.4/";

static void only_the_release_files_of_this_repository_are_downloaded() {
  CHECK(release::is_official_url(kBase + "x-0.0.4-s3-eth-app.bin", kRepo));
  CHECK(!release::is_official_url("https://github.com/JuanenRac/EH3D-NODE-BUNGARD-evil/releases/download/v1/x.bin", kRepo));
  CHECK(!release::is_official_url("https://github.com/JuanenRac/EH3D-NODE-BUNGARD/releases/download/v0.0.4//x.bin", kRepo));   // an empty tag
  CHECK(!release::is_official_url(kBase + "x.bin#frag", kRepo));
  CHECK(!release::is_official_url(kBase + "x\\y.bin", kRepo));
  CHECK(!release::is_official_url(kBase + "x y.bin", kRepo));
  CHECK(!release::is_official_url(kBase + "a..b.bin", kRepo));
  CHECK(!release::is_official_url(kBase + "x.bin/", kRepo));
  CHECK(!release::is_official_url("https://github.com/JuanenRac/EH3D-NODE-BUNGARD/releases/download/v/a", kRepo));   // a tag and a file too short to be real
  CHECK(!release::is_official_url("https://github.com/JuanenRac/EH3D-NODE-BUNGARD/releases/download/", kRepo));
  CHECK(!release::is_official_url("", kRepo));
}

static std::string asset(const std::string& name) { return "{\"name\":\"" + name + "\",\"browser_download_url\":\"" + kBase + name + "\"}"; }

static void the_image_is_the_one_with_a_version_for_this_board_and_the_checksum_goes_with_it() {
  json::Value doc;
  // a name that has no version, one whose version does not start with a digit, and the right one, last
  CHECK(json::parse("{\"assets\":[" + asset("img--s3-eth-app.bin") + "," + asset("img-abc-s3-eth-app.bin") + "," + asset("img-1.0.0-s3-eth-app.bin") + "," + asset("img-1.0.0-s3-eth-app.bin.sha256") + "," + asset("img-2.0.0-s3-eth-app.bin.sha256") + "]}", doc));
  const release::Picked p = release::pick(doc.get("assets"), "img", "s3-eth");
  CHECK(p.image_url == kBase + "img-1.0.0-s3-eth-app.bin" && p.checksum_url == kBase + "img-1.0.0-s3-eth-app.bin.sha256");
  // a checksum of another file is not this one's
  json::Value other;
  CHECK(json::parse("{\"assets\":[" + asset("img-1.0.0-s3-eth-app.bin") + "," + asset("img-1.0.1-s3-eth-app.bin.sha256") + "]}", other));
  CHECK(release::pick(other.get("assets"), "img", "s3-eth").checksum_url.empty());
  // not an array, or no names
  json::Value odd;
  CHECK(json::parse("{\"assets\":{\"name\":\"x\"}}", odd));
  CHECK(release::pick(odd.get("assets"), "img", "s3-eth").image_url.empty());
  CHECK(release::pick(doc.get("assets"), nullptr, "s3-eth").image_url.empty() && release::pick(doc.get("assets"), "img", nullptr).image_url.empty());
  json::Value noname;
  CHECK(json::parse("{\"assets\":[{\"browser_download_url\":\"https://x\"},{\"name\":5}]}", noname));
  CHECK(release::pick(noname.get("assets"), "img", "s3-eth").image_url.empty());
}

// ---- the frames of the control link ------------------------------------------------------------------------------------------------------

static std::vector<std::uint8_t> frame_of(bungard::Type type, const std::vector<std::uint8_t>& payload) {
  std::vector<std::uint8_t> out(bungard::kMaxFrameBytes);
  const std::size_t n = bungard::encode(type, payload.data(), payload.size(), out.data(), out.size());
  out.resize(n);
  return out;
}

struct Got { bungard::Type type; std::vector<std::uint8_t> payload; };

static std::vector<Got> read_all(bungard::Decoder& d, const std::vector<std::uint8_t>& bytes) {
  std::vector<Got> got;
  bungard::Frame f;
  for (const std::uint8_t b : bytes) {
    if (d.feed(b, f)) got.push_back({f.type, std::vector<std::uint8_t>(f.payload.begin(), f.payload.begin() + f.length)});
  }
  return got;
}

static void a_frame_goes_through_the_encoder_and_the_decoder_at_every_size() {
  for (const std::size_t n : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(255), std::size_t(256), std::size_t(257), bungard::kMaxPayload - 1, bungard::kMaxPayload}) {
    std::vector<std::uint8_t> payload(n);
    for (std::size_t i = 0; i < n; ++i) payload[i] = static_cast<std::uint8_t>(i * 7 + (i >> 8) + 0xB5 * (i % 3 == 0));   // the sync byte appears inside
    bungard::Decoder d;
    const auto got = read_all(d, frame_of(bungard::Type::Write, payload));
    CHECK(got.size() == 1 && got[0].type == bungard::Type::Write && got[0].payload == payload && d.damaged() == 0);
  }
}

static void the_encoder_refuses_what_does_not_fit() {
  std::uint8_t out[64] = {};
  const std::uint8_t p[8] = {};
  CHECK(bungard::encode(bungard::Type::Ping, p, 0, out, 5) == 0);          // 6 bytes are needed even for nothing
  CHECK(bungard::encode(bungard::Type::Ping, p, 0, out, 6) == 6);
  CHECK(bungard::encode(bungard::Type::Write, p, 8, out, 13) == 0 && bungard::encode(bungard::Type::Write, p, 8, out, 14) == 14);
  std::vector<std::uint8_t> big(bungard::kMaxPayload + 1), buffer(bungard::kMaxFrameBytes + 10);
  CHECK(bungard::encode(bungard::Type::Write, big.data(), big.size(), buffer.data(), buffer.size()) == 0);
}

static void frames_one_after_the_other_come_out_one_by_one() {
  std::vector<std::uint8_t> bytes;
  for (std::uint8_t i = 0; i < 20; ++i) {
    const auto f = frame_of(i % 2 ? bungard::Type::Dtr : bungard::Type::Write, std::vector<std::uint8_t>(static_cast<std::size_t>(i), i));
    bytes.insert(bytes.end(), f.begin(), f.end());
  }
  bungard::Decoder d;
  const auto got = read_all(d, bytes);
  CHECK(got.size() == 20 && d.damaged() == 0);
  for (std::size_t i = 0; i < got.size() && i < 20; ++i) CHECK(got[i].payload.size() == i && (i == 0 || got[i].payload[0] == i));
}

static void noise_before_and_between_frames_is_skipped() {
  std::vector<std::uint8_t> bytes = {0x00, 0xFF, 0x12, bungard::kSync, 0x7F};      // a sync byte that starts nothing
  const auto a = frame_of(bungard::Type::Ping, {}), b = frame_of(bungard::Type::Arm, {});
  bytes.insert(bytes.end(), a.begin(), a.end());
  for (int i = 0; i < 3000; ++i) bytes.push_back(static_cast<std::uint8_t>(i % 5 ? 0x20 + i % 7 : 0x00));   // a long quiet noise
  bytes.insert(bytes.end(), b.begin(), b.end());
  bungard::Decoder d;
  const auto got = read_all(d, bytes);
  CHECK(got.size() == 2 && got[0].type == bungard::Type::Ping && got[1].type == bungard::Type::Arm);
}

static void a_damaged_frame_is_counted_and_the_next_good_one_is_found() {
  auto bad = frame_of(bungard::Type::Write, {1, 2, 3, 4});
  bad[5] ^= 0x01;                                                    // one bit of the payload
  const auto good = frame_of(bungard::Type::Abort, {});
  std::vector<std::uint8_t> bytes = bad;
  bytes.insert(bytes.end(), good.begin(), good.end());
  bungard::Decoder d;
  const auto got = read_all(d, bytes);
  CHECK(got.size() == 1 && got[0].type == bungard::Type::Abort && d.damaged() >= 1);
}

static void a_whole_frame_inside_the_bytes_of_a_broken_one_is_not_lost() {
  // the header of a frame whose length says 12 bytes; the bytes after it are a good frame, and the checksum of the broken one cannot match
  const auto inner = frame_of(bungard::Type::Ping, {});
  std::vector<std::uint8_t> bytes = {bungard::kSync, static_cast<std::uint8_t>(bungard::Type::Write), 12, 0};
  bytes.insert(bytes.end(), inner.begin(), inner.end());
  for (int i = 0; i < 20; ++i) bytes.push_back(0x55);
  bungard::Decoder d;
  const auto got = read_all(d, bytes);
  CHECK(got.size() == 1 && got[0].type == bungard::Type::Ping && d.damaged() >= 1);
}

static void a_length_above_the_maximum_is_refused_at_the_header() {
  const auto good = frame_of(bungard::Type::Arm, {});
  std::vector<std::uint8_t> bytes = {bungard::kSync, static_cast<std::uint8_t>(bungard::Type::Write), 0x01, 0x08};   // 2049 bytes
  bytes.insert(bytes.end(), good.begin(), good.end());
  bungard::Decoder d;
  const auto got = read_all(d, bytes);
  CHECK(got.size() == 1 && got[0].type == bungard::Type::Arm && d.damaged() == 1);
}

static void a_frame_cut_by_the_end_of_the_line_waits_and_reset_forgets_it() {
  const auto f = frame_of(bungard::Type::Write, {1, 2, 3, 4, 5, 6});
  bungard::Decoder d;
  auto got = read_all(d, std::vector<std::uint8_t>(f.begin(), f.begin() + 7));
  CHECK(got.empty());
  got = read_all(d, std::vector<std::uint8_t>(f.begin() + 7, f.end()));
  CHECK(got.size() == 1 && got[0].payload.size() == 6);
  // cut and forgotten: the second half alone is noise
  got = read_all(d, std::vector<std::uint8_t>(f.begin(), f.begin() + 7));
  d.reset();
  got = read_all(d, std::vector<std::uint8_t>(f.begin() + 7, f.end()));
  CHECK(got.empty());
}

static void a_flood_of_noise_never_overruns_and_the_next_frame_still_comes() {
  bungard::Decoder d;
  std::vector<std::uint8_t> noise;
  std::uint32_t s = 12345;
  for (int i = 0; i < 200000; ++i) { s = s * 1664525u + 1013904223u; noise.push_back(static_cast<std::uint8_t>(s >> 24)); }
  read_all(d, noise);
  // a random start of a frame can have swallowed the first frames that follow (it waits for up to 2048 bytes and then finds that its check is wrong);
  // after that the decoder looks again from the byte after the false start, so some of the frames sent next come through
  std::vector<std::uint8_t> frames;
  for (int i = 0; i < 600; ++i) {
    const auto f = frame_of(bungard::Type::Status, {1, 2, 3});
    frames.insert(frames.end(), f.begin(), f.end());
  }
  const auto got = read_all(d, frames);
  CHECK(!got.empty() && got.back().type == bungard::Type::Status && got.back().payload.size() == 3);
  CHECK(got.size() > 300);                                           // and once it has found its place it does not lose any more
}

static void the_check_of_a_frame_changes_with_every_bit() {
  const auto f = frame_of(bungard::Type::Write, {0xDE, 0xAD, 0xBE, 0xEF});
  for (std::size_t byte = 1; byte < f.size(); ++byte) {
    for (int bit = 0; bit < 8; ++bit) {
      auto copy = f;
      copy[byte] ^= static_cast<std::uint8_t>(1 << bit);
      bungard::Decoder d;
      const auto got = read_all(d, copy);
      // a frame with one bit wrong is never accepted as it was (a different length makes it wait for more bytes, which is also not accepted)
      CHECK(got.empty() || got[0].payload != std::vector<std::uint8_t>({0xDE, 0xAD, 0xBE, 0xEF}) || got[0].type != bungard::Type::Write);
    }
  }
  const std::uint8_t abc[3] = {'a', 'b', 'c'};
  CHECK(bungard::crc16(abc, 3) == bungard::crc16(abc, 3));
  const std::uint8_t check[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(bungard::crc16(check, 9) == 0x29B1);                          // the known value of CRC-16/CCITT-FALSE
  CHECK(bungard::crc16(check, 0) == 0xFFFF);
}

// ---- the text rules of the network settings ----------------------------------------------------------------------------------------------

static void addresses_are_exactly_four_numbers_without_extra_zeros() {
  std::uint32_t v = 0xDEADBEEF;
  CHECK(net::parse_ipv4("0.0.0.0", v) && v == 0);
  CHECK(net::parse_ipv4("255.255.255.255", v) && v == 0xFFFFFFFFu);
  CHECK(net::parse_ipv4("192.168.1.10", v) && v == 0xC0A8010Au);
  v = 0xDEADBEEF;
  for (const char* bad : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "1.2.3.04", "01.2.3.4", "10.0.0.00", "1.2.3.", ".1.2.3", "1..2.3", " 1.2.3.4", "1.2.3.4 ", "1.2.3.4\n", "a.b.c.d", "1.2.3.-4", "1111.2.3.4", "1.2.3.4abc", "1,2,3,4"}) CHECK(!net::parse_ipv4(bad, v));
  CHECK(v == 0xDEADBEEF);                                            // a refusal leaves the answer alone
  for (std::uint32_t x : {0u, 1u, 0x7F000001u, 0xC0A80001u, 0xFFFFFFFFu, 0x0A00FF00u}) { std::uint32_t back = 0; CHECK(net::parse_ipv4(net::ipv4_text(x), back) && back == x); }
  CHECK(net::ipv4_text(0x0A000102u) == "10.0.1.2");
}

static void masks_are_runs_of_ones_with_room_for_two_hosts() {
  for (const char* good : {"255.255.255.0", "255.255.255.252", "255.255.0.0", "255.0.0.0", "128.0.0.0", "255.255.254.0"}) { std::uint32_t m = 0; CHECK(net::parse_ipv4(good, m) && net::valid_netmask(m)); }
  for (const char* bad : {"255.255.255.254", "255.255.255.255", "0.0.0.0", "255.0.255.0", "0.255.255.255", "255.255.255.253", "1.0.0.0"}) { std::uint32_t m = 0; CHECK(net::parse_ipv4(bad, m) && !net::valid_netmask(m)); }
}

static void host_addresses_leave_out_loopback_multicast_and_the_ends_of_a_subnet() {
  CHECK(net::usable_host_address(0x0A000001u) && net::usable_host_address(0xDFFFFFFFu));
  CHECK(!net::usable_host_address(0) && !net::usable_host_address(0x7F000001u) && !net::usable_host_address(0xE0000001u) && !net::usable_host_address(0xFFFFFFFFu));
  const std::uint32_t mask = 0xFFFFFF00u;
  CHECK(net::is_network_or_broadcast(0xC0A80100u, mask) && net::is_network_or_broadcast(0xC0A801FFu, mask) && !net::is_network_or_broadcast(0xC0A80101u, mask));
  CHECK(net::same_subnet(0xC0A80101u, 0xC0A801FEu, mask) && !net::same_subnet(0xC0A80101u, 0xC0A80201u, mask));
}

static void names_and_hosts() {
  CHECK(net::valid_hostname("node-1") && net::valid_hostname(std::string(32, 'a')) && !net::valid_hostname(std::string(33, 'a')));
  for (const char* bad : {"", "-a", "a-", "A", "a_b", "a.b", "a b", "\xC3\xB1"}) CHECK(!net::valid_hostname(bad));
  CHECK(net::valid_host("Time.Example_org") && net::valid_host("10.0.0.1") && net::valid_host(std::string(253, 'a')) && !net::valid_host(std::string(254, 'a')));
  for (const char* bad : {"", ".a", "-a", "a.", "a b", "a/b", "a:b", "a@b"}) CHECK(!net::valid_host(bad));
}

static void text_is_checked_as_utf8_strictly() {
  std::size_t n = 99;
  CHECK(net::valid_utf8("", &n) && n == 0);
  CHECK(net::valid_utf8("abc", &n) && n == 3);
  CHECK(net::valid_utf8("\xC3\xB1", &n) && n == 1);                    // n with a tilde
  CHECK(net::valid_utf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", &n) && n == 3);
  CHECK(net::valid_utf8("\xF0\x9F\x98\x80", &n) && n == 1);
  CHECK(net::valid_utf8("a\xC3\xB1" "b\xE6\x97\xA5", &n) && n == 4);
  CHECK(net::valid_utf8("\xF4\x8F\xBF\xBF") && net::valid_utf8("\xED\x9F\xBF") && net::valid_utf8("\xC2\x80"));   // the last of each range
  for (const char* bad : {"\xC0\x80", "\xC1\xBF", "\xE0\x80\x80", "\xE0\x9F\xBF", "\xF0\x80\x80\x80", "\xED\xA0\x80", "\xED\xBF\xBF", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF", "\x80", "\xBF",
                          "\xE6\x97", "\xC3", "\xF0\x9F\x98", "\xE6\x97" "A", "\xC3" "A"})
    CHECK(!net::valid_utf8(bad));
}

static void wifi_names_and_keys() {
  CHECK(net::valid_ssid("Casa") && net::valid_ssid(std::string(32, 'x')) && net::valid_ssid("Wi-Fi \xC3\xB1") && !net::valid_ssid("") && !net::valid_ssid(std::string(33, 'x')));
  CHECK(!net::valid_ssid("a\tb") && !net::valid_ssid("a\nb") && !net::valid_ssid("a\x7f"));
  CHECK(net::valid_wpa_passphrase("12345678") && net::valid_wpa_passphrase(std::string(63, 'k')) && net::valid_wpa_passphrase("with spaces ok"));
  CHECK(!net::valid_wpa_passphrase("1234567") && !net::valid_wpa_passphrase(std::string(64, 'k')) && !net::valid_wpa_passphrase("tab\there!!") && !net::valid_wpa_passphrase("ca\xC3\xB1" "on1234"));
}

int main() {
  queue_keeps_order_across_the_end_of_its_buffer();
  queue_takes_a_push_whole_or_not_at_all();
  queue_edges();
  plain_versions_only();
  newer_is_by_number_not_by_text();
  node_names();
  every_day_from_1899_to_2101_is_one_more_than_the_one_before();
  known_moments();
  the_offset_of_a_time_zone_from_the_local_and_the_utc_clock();
  drain_waits_for_the_machine_and_stops_it_once();
  drain_ends_when_the_machine_goes_idle_and_survives_the_wrap_of_the_clock();
  drain_abort_and_zero_time();
  send_goes_on_after_partial_sends_and_gives_up_at_the_limit();
  send_fails_at_once_on_an_error_and_has_nothing_to_do_for_nothing();
  the_host_of_a_request_is_cleaned_or_refused();
  a_redirect_goes_to_https_on_the_same_host_and_never_somewhere_else();
  the_cookie_is_secure_over_tls_and_never_readable_by_scripts();
  only_the_release_files_of_this_repository_are_downloaded();
  the_image_is_the_one_with_a_version_for_this_board_and_the_checksum_goes_with_it();
  a_frame_goes_through_the_encoder_and_the_decoder_at_every_size();
  the_encoder_refuses_what_does_not_fit();
  frames_one_after_the_other_come_out_one_by_one();
  noise_before_and_between_frames_is_skipped();
  a_damaged_frame_is_counted_and_the_next_good_one_is_found();
  a_whole_frame_inside_the_bytes_of_a_broken_one_is_not_lost();
  a_length_above_the_maximum_is_refused_at_the_header();
  a_frame_cut_by_the_end_of_the_line_waits_and_reset_forgets_it();
  a_flood_of_noise_never_overruns_and_the_next_frame_still_comes();
  the_check_of_a_frame_changes_with_every_bit();
  addresses_are_exactly_four_numbers_without_extra_zeros();
  masks_are_runs_of_ones_with_room_for_two_hosts();
  host_addresses_leave_out_loopback_multicast_and_the_ends_of_a_subnet();
  names_and_hosts();
  text_is_checked_as_utf8_strictly();
  wifi_names_and_keys();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
