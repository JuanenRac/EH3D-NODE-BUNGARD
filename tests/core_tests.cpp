// EH3D-NODE-BUNGARD host tests: the frames, the queue and the session rules, on a computer with a stand-in for the UART.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../core/byte_queue.hpp"
#include "../core/drain.hpp"
#include "../core/frame.hpp"
#include "../core/session.hpp"

using namespace eh3d::bungard;

static int g_checks = 0;
static int g_failed = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    ++g_checks;                                                           \
    if (!(cond)) { ++g_failed; std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

static std::vector<std::uint8_t> make(Type t, const std::vector<std::uint8_t>& payload = {}) {
  std::vector<std::uint8_t> out(kMaxFrameBytes);
  const std::size_t n = encode(t, payload.data(), payload.size(), out.data(), out.size());
  out.resize(n);
  return out;
}

static Frame frame_of(Type t, const std::vector<std::uint8_t>& payload = {}) {
  Frame f;
  f.type = t;
  f.length = static_cast<std::uint16_t>(payload.size());
  for (std::size_t i = 0; i < payload.size(); ++i) f.payload[i] = payload[i];
  return f;
}

static std::vector<Frame> decode_all(Decoder& d, const std::vector<std::uint8_t>& bytes) {
  std::vector<Frame> out;
  Frame f;
  for (auto b : bytes) {
    if (d.feed(b, f)) {
      out.push_back(f);
      while (d.poll(f)) out.push_back(f);
    }
  }
  return out;
}

// ---- frames ----

static void frame_tests() {
  // known CRC-16/CCITT-FALSE value of "123456789"
  const std::uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(crc16(check, 9) == 0x29B1);

  const auto bytes = make(Type::Write, {0x43, 0x00, 0x43, 0xFF});
  CHECK(bytes.size() == kHeaderBytes + 4 + 2);
  CHECK(bytes[0] == kSync && bytes[1] == 0x05 && bytes[2] == 4 && bytes[3] == 0);

  Decoder d;
  Frame f;
  bool got = false;
  for (auto b : bytes) got = d.feed(b, f);
  CHECK(got && f.type == Type::Write && f.length == 4 && f.payload[0] == 0x43 && f.payload[3] == 0xFF);

  // empty payload
  Decoder d2;
  const auto ping = make(Type::Ping);
  bool g2 = false;
  for (auto b : ping) g2 = d2.feed(b, f);
  CHECK(g2 && f.type == Type::Ping && f.length == 0);

  // garbage before a frame, a false start of frame, and two frames back to back: both frames are found
  Decoder d3;
  std::vector<std::uint8_t> stream = {0x00, 0x13, 0xB5, 0xFF};  // 0xB5 starts a bogus frame (type 0xFF)
  const auto a = make(Type::Arm);
  const auto b2 = make(Type::Abort);
  stream.insert(stream.end(), a.begin(), a.end());
  stream.insert(stream.end(), b2.begin(), b2.end());
  // the false start claims a length from the next bytes (0xB5, 0x02 -> 693) so it only fails after that many bytes
  stream.insert(stream.end(), 800, 0x00);
  const auto last = make(Type::Ping);
  stream.insert(stream.end(), last.begin(), last.end());
  const auto seen = decode_all(d3, stream);
  CHECK(seen.size() == 3);
  if (seen.size() == 3) CHECK(seen[0].type == Type::Arm && seen[1].type == Type::Abort && seen[2].type == Type::Ping);
  CHECK(d3.damaged() >= 1);

  // a damaged byte drops the frame and is counted
  Decoder d4;
  auto bad = make(Type::Write, {1, 2, 3});
  bad[5] ^= 0x01;
  bool got4 = false;
  for (auto b : bad) got4 = d4.feed(b, f) || got4;
  CHECK(!got4 && d4.damaged() == 1);

  // a length above the maximum is rejected at once
  Decoder d5;
  const std::uint8_t huge[] = {kSync, 0x05, 0xFF, 0xFF};
  bool got5 = false;
  for (auto b : huge) got5 = d5.feed(b, f) || got5;
  CHECK(!got5 && d5.damaged() == 1);

  // largest payload round trip
  std::vector<std::uint8_t> big(kMaxPayload);
  for (std::size_t i = 0; i < big.size(); ++i) big[i] = static_cast<std::uint8_t>(i * 7);
  const auto bb = make(Type::Write, big);
  CHECK(bb.size() == kMaxFrameBytes);
  Decoder d6;
  bool got6 = false;
  for (auto b : bb) got6 = d6.feed(b, f);
  CHECK(got6 && f.length == kMaxPayload && f.payload[kMaxPayload - 1] == big.back());

  // too long to encode
  std::vector<std::uint8_t> out(kMaxFrameBytes + 8);
  CHECK(encode(Type::Write, big.data(), kMaxPayload + 1, out.data(), out.size()) == 0);
  CHECK(encode(Type::Write, big.data(), 10, out.data(), 5) == 0);
}

// ---- queue ----

static void queue_tests() {
  ByteQueue q(8);
  const std::uint8_t a[] = {1, 2, 3, 4, 5};
  CHECK(q.push(a, 5) && q.size() == 5 && q.free() == 3);
  CHECK(!q.push(a, 5) && q.size() == 5);  // all or nothing
  std::uint8_t out[8];
  CHECK(q.pop(out, 3) == 3 && out[0] == 1 && out[2] == 3);
  CHECK(q.push(a, 5) && q.size() == 7);  // wraps around
  CHECK(q.pop(out, 8) == 7 && out[0] == 4 && out[1] == 5 && out[2] == 1 && out[6] == 5);
  CHECK(q.size() == 0);
  CHECK(q.push(a, 5));
  q.clear();
  CHECK(q.size() == 0 && q.free() == 8);
}

// ---- session ----

struct FakeHardware : Hardware {
  ByteQueue queue{64};
  int flushes = 0, drivers_off = 0;
  std::vector<bool> dtr;
  std::vector<std::pair<Type, std::vector<std::uint8_t>>> replies;
  std::size_t queue_free() override { return queue.free(); }
  bool enqueue(const std::uint8_t* d, std::size_t n) override { return queue.push(d, n); }
  void flush_queue() override { ++flushes; queue.clear(); }
  void send_drivers_off() override { ++drivers_off; }
  void set_dtr(bool on) override { dtr.push_back(on); }
  std::vector<std::pair<std::uint32_t, bool>> spindle;
  void set_spindle(std::uint32_t rpm, bool reverse) override { spindle.push_back({rpm, reverse}); }
  std::vector<std::pair<std::uint32_t, bool>> events;   // spindle commands queued among the bytes
  bool events_full = false;
  bool spindle_event(std::uint32_t rpm, bool reverse) override {
    if (events_full) return false;
    events.push_back({rpm, reverse});
    return true;
  }
  bool can_drain = true;   // there is something left to finish
  int drains = 0, drain_aborts = 0;
  bool begin_drain() override { if (!can_drain) return false; ++drains; return true; }
  void abort_drain() override { ++drain_aborts; }
  void reply(Type t, const std::uint8_t* p, std::size_t n) override { replies.push_back({t, std::vector<std::uint8_t>(p, p + n)}); }
  Error last_error() const {
    for (auto it = replies.rbegin(); it != replies.rend(); ++it)
      if (it->first == Type::Error) return static_cast<Error>(it->second[0]);
    return static_cast<Error>(0);
  }
};

static std::vector<std::uint8_t> text(const std::string& s) { return std::vector<std::uint8_t>(s.begin(), s.end()); }

static void session_tests() {
  Info info;
  info.major = 0; info.minor = 0; info.patch = 1; info.baud = 38400; info.queue_capacity = 64;

  {  // nothing works before Hello
    FakeHardware hw;
    Session s({"secret", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Arm), 0);
    CHECK(hw.last_error() == Error::NotAuthenticated && !s.armed());
    s.on_frame(frame_of(Type::Write, {0x43}), 0);
    CHECK(hw.queue.size() == 0);
  }
  {  // wrong token, right token
    FakeHardware hw;
    Session s({"secret", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("secreT")), 0);
    CHECK(hw.last_error() == Error::BadToken && !s.authenticated());
    s.on_frame(frame_of(Type::Hello, text("secre")), 0);
    CHECK(hw.last_error() == Error::BadToken && !s.authenticated());
    s.on_frame(frame_of(Type::Hello, text("secret!")), 0);
    CHECK(!s.authenticated());
    s.on_frame(frame_of(Type::Hello, text("secret")), 0);
    CHECK(s.authenticated());
    CHECK(hw.replies.back().first == Type::Info && hw.replies.back().second.size() == 11);
    CHECK(hw.replies.back().second[2] == 1);
    CHECK(hw.replies.back().second[3] == (38400 & 0xFF) && hw.replies.back().second[4] == ((38400 >> 8) & 0xFF));
  }
  {  // a node with no token refuses control unless the build allows open control
    FakeHardware hw;
    Session closed({"", false, 3000}, info, hw);
    closed.on_frame(frame_of(Type::Hello, {}), 0);
    CHECK(!closed.authenticated() && hw.last_error() == Error::OpenRefused);
    Session open({"", true, 3000}, info, hw);
    open.on_frame(frame_of(Type::Hello, {}), 0);
    CHECK(open.authenticated());
  }
  {  // writes need the session armed and room in the queue
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Write, {0x43, 0x43}), 1);
    CHECK(hw.last_error() == Error::NotArmed && hw.queue.size() == 0);
    s.on_frame(frame_of(Type::Arm), 2);
    CHECK(s.armed());
    s.on_frame(frame_of(Type::Write, {0x43, 0x43}), 3);
    CHECK(hw.queue.size() == 2);
    s.on_frame(frame_of(Type::Write, std::vector<std::uint8_t>(63, 0)), 4);  // 2 + 63 > 64
    CHECK(hw.last_error() == Error::QueueFull && hw.queue.size() == 2);
    s.on_frame(frame_of(Type::Write, std::vector<std::uint8_t>(62, 0)), 5);  // fits exactly
    CHECK(hw.queue.size() == 64);
  }
  {  // disarm empties the queue; dtr is passed on; a short Dtr frame is refused
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Dtr, {1}), 1);
    s.on_frame(frame_of(Type::Dtr, {0}), 2);
    CHECK(hw.dtr.size() == 2 && hw.dtr[0] && !hw.dtr[1]);
    s.on_frame(frame_of(Type::Dtr, {}), 3);
    CHECK(hw.last_error() == Error::BadFrame);
    s.on_frame(frame_of(Type::Arm), 4);
    s.on_frame(frame_of(Type::Write, {1, 2, 3}), 5);
    s.on_frame(frame_of(Type::Disarm), 6);
    CHECK(!s.armed() && hw.queue.size() == 0 && hw.flushes == 1 && hw.drivers_off == 0);
  }
  {  // abort empties the queue, switches the drivers off and disarms
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Write, {1, 2, 3}), 2);
    s.on_frame(frame_of(Type::Abort), 3);
    CHECK(!s.armed() && hw.queue.size() == 0 && hw.drivers_off == 1);
  }
  {  // an armed session that goes quiet is stopped; a ping keeps it alive
    FakeHardware hw;
    Session s({"t", false, 1000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 100);
    s.on_frame(frame_of(Type::Write, {1, 2, 3}), 200);
    s.on_tick(1100);
    CHECK(s.armed() && hw.drivers_off == 0);
    s.on_frame(frame_of(Type::Ping), 1150);
    s.on_tick(2100);
    CHECK(s.armed() && hw.drivers_off == 0);
    s.on_tick(2200);
    CHECK(!s.armed() && hw.drivers_off == 1 && hw.queue.size() == 0 && hw.last_error() == Error::IdleTimeout);
    s.on_tick(9999);  // already stopped: no second stop
    CHECK(hw.drivers_off == 1);
  }
  {  // an idle session that is not armed is left alone
    FakeHardware hw;
    Session s({"t", false, 1000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_tick(100000);
    CHECK(hw.drivers_off == 0 && s.authenticated());
  }
  {  // a dropped connection while armed stops the machine and forgets the login
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Write, {1, 2, 3}), 2);
    s.on_disconnect();
    CHECK(!s.armed() && !s.authenticated() && hw.drivers_off == 1 && hw.queue.size() == 0);
    s.on_frame(frame_of(Type::Arm), 3);
    CHECK(!s.armed());  // must log in again
  }
  {  // the clock wraps without stopping a live session
    FakeHardware hw;
    Session s({"t", false, 1000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0xFFFFFF00u);
    s.on_frame(frame_of(Type::Arm), 0xFFFFFF10u);
    s.on_tick(0x00000100u);  // 0x1F0 ms later, across the wrap
    CHECK(s.armed());
    s.on_tick(0x00000500u);  // more than 1000 ms
    CHECK(!s.armed());
  }
  {  // unknown frame types from the program are refused
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Status), 1);
    CHECK(hw.last_error() == Error::BadFrame);
  }
}


static void spindle_tests() {
  Info info;
  info.queue_capacity = 64;
  auto rpm = [](unsigned v, int dir = -1) {
    std::vector<std::uint8_t> p = {static_cast<std::uint8_t>(v & 0xFF), static_cast<std::uint8_t>(v >> 8)};
    if (dir >= 0) p.push_back(static_cast<std::uint8_t>(dir));
    return p;
  };
  {  // the spindle starts only while armed; stopping is always allowed
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Spindle, rpm(12000)), 1);
    CHECK(hw.last_error() == Error::NotArmed && hw.spindle.empty());
    s.on_frame(frame_of(Type::Spindle, rpm(0)), 2);
    CHECK(hw.spindle.empty());
    s.on_frame(frame_of(Type::Arm), 3);
    s.on_frame(frame_of(Type::Spindle, rpm(12000)), 4);
    CHECK(hw.spindle.size() == 1 && hw.spindle[0].first == 12000 && !hw.spindle[0].second && s.spindle_rpm() == 12000);
    s.on_frame(frame_of(Type::Spindle, rpm(0)), 5);
    CHECK(hw.spindle.size() == 2 && hw.spindle[1].first == 0 && s.spindle_rpm() == 0);
  }
  {  // never above the configured maximum; a short frame is refused
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Spindle, rpm(60000)), 2);
    CHECK(hw.spindle.back().first == 24000);
    s.on_frame(frame_of(Type::Spindle, {1}), 3);
    CHECK(hw.last_error() == Error::BadFrame);
  }
  {  // a node built without a spindle says so
    FakeHardware hw;
    Settings st{"t", false, 3000};
    st.max_rpm = 0;
    Session s(st, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Spindle, rpm(1000)), 2);
    CHECK(hw.last_error() == Error::NoSpindle && hw.spindle.empty());
  }
  {  // the direction cannot change while turning, nor right after stopping
    FakeHardware hw;
    Session s({"t", false, 60000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Spindle, rpm(8000, 0)), 2);
    s.on_frame(frame_of(Type::Spindle, rpm(8000, 1)), 3);
    CHECK(hw.last_error() == Error::SpindleDirection && !s.spindle_reverse() && s.spindle_rpm() == 8000);
    s.on_frame(frame_of(Type::Spindle, rpm(9000)), 4);  // the same direction: just a new speed
    CHECK(s.spindle_rpm() == 9000);
    s.on_frame(frame_of(Type::Spindle, rpm(0)), 1000);
    s.on_frame(frame_of(Type::Spindle, rpm(8000, 1)), 2000);  // only 1000 ms after the stop
    CHECK(hw.last_error() == Error::SpindleSettling && s.spindle_rpm() == 0);
    s.on_frame(frame_of(Type::Spindle, rpm(8000, 1)), 4500);  // 3500 ms
    CHECK(s.spindle_rpm() == 8000 && s.spindle_reverse() && hw.spindle.back().second);
  }
  {  // abort, disarm, a quiet session and a dropped link all stop the spindle
    for (int how = 0; how < 4; ++how) {
      FakeHardware hw;
      Session s({"t", false, 1000}, info, hw);
      s.on_frame(frame_of(Type::Hello, text("t")), 0);
      s.on_frame(frame_of(Type::Arm), 1);
      s.on_frame(frame_of(Type::Spindle, rpm(20000)), 2);
      if (how == 0) s.on_frame(frame_of(Type::Abort), 3);
      if (how == 1) s.on_frame(frame_of(Type::Disarm), 3);
      if (how == 2) s.on_tick(2000);
      if (how == 3) s.on_disconnect();
      CHECK(s.spindle_rpm() == 0 && hw.spindle.back().first == 0);
    }
  }
}

static void home_tests() {
  Info info;
  info.queue_capacity = 64;
  {  // StopOnHome drops the rest of the queue when a watched lamp lights, once
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Write, {1, 2, 3, 4}), 2);
    s.on_frame(frame_of(Type::StopOnHome, {2}), 3);  // Y
    CHECK(s.stop_on_home() == 2);
    s.on_inputs(0x01);  // HOME X lights: not watched
    s.on_inputs(0x08);  // ENABLE: not an axis
    CHECK(hw.queue.size() == 4 && hw.flushes == 0);
    s.on_inputs(0x03);  // HOME X and HOME Y
    CHECK(hw.queue.size() == 0 && hw.flushes == 1 && s.stop_on_home() == 0);
    CHECK(hw.replies.back().first == Type::HomeSeen && hw.replies.back().second[0] == 0x03);
    s.on_inputs(0x03);  // asked for once: no second flush
    CHECK(hw.flushes == 1);
  }
  {  // a stop, a disarm and a short frame
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::StopOnHome, {7}), 1);
    s.on_frame(frame_of(Type::Disarm), 2);
    CHECK(s.stop_on_home() == 0);
    s.on_frame(frame_of(Type::StopOnHome, {}), 3);
    CHECK(hw.last_error() == Error::BadFrame);
    s.on_frame(frame_of(Type::StopOnHome, {0xFF}), 4);
    CHECK(s.stop_on_home() == 7);  // only the three axes
    s.on_frame(frame_of(Type::Abort), 5);
    CHECK(s.stop_on_home() == 0);
  }
}

static void drain_tests() {
  Info info;
  info.queue_capacity = 64;
  Settings finish{"t", false, 3000};
  finish.finish_on_loss = true;
  {  // by default a lost program stops everything, as before
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Write, {1, 2, 3}), 2);
    s.on_disconnect();
    CHECK(hw.drains == 0 && hw.drivers_off == 1 && hw.queue.size() == 0 && !s.draining());
  }
  {  // with finish_on_loss the queue is kept and the node starts to finish it
    FakeHardware hw;
    Session s(finish, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Spindle, {0x10, 0x27, 0, 1}), 2);   // 10000 rpm, forward, at the end of the queue
    s.on_frame(frame_of(Type::Write, {1, 2, 3}), 3);
    s.on_disconnect();
    CHECK(hw.drains == 1 && hw.drivers_off == 0 && hw.queue.size() == 3 && s.draining() && !s.armed());
    CHECK(hw.events.size() == 1 && hw.events[0].first == 10000 && hw.spindle.empty());
  }
  {  // nothing left to finish: the machine stops as usual
    FakeHardware hw;
    hw.can_drain = false;
    Session s(finish, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_disconnect();
    CHECK(hw.drivers_off == 1 && !s.draining());
  }
  {  // a program that goes quiet while connected also leaves the node finishing
    FakeHardware hw;
    Settings quick = finish;
    quick.idle_ms = 1000;
    Session s(quick, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 100);
    s.on_frame(frame_of(Type::Write, {1, 2, 3}), 200);
    s.on_tick(2000);
    CHECK(s.draining() && hw.queue.size() == 3 && hw.last_error() == Error::IdleTimeout && hw.drivers_off == 0);
  }
  {  // a new connection to a node that is finishing: nothing new is taken, Ping is welcome, Abort ends it
    FakeHardware hw;
    Settings again = finish;
    again.draining = true;
    Session s(again, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    CHECK(s.authenticated() && s.draining());
    s.on_frame(frame_of(Type::Arm), 1);
    CHECK(hw.last_error() == Error::Draining && !s.armed());
    s.on_frame(frame_of(Type::Write, {1}), 2);
    CHECK(hw.last_error() == Error::Draining && hw.queue.size() == 0);
    s.on_frame(frame_of(Type::Spindle, {0x10, 0x27}), 3);
    CHECK(hw.last_error() == Error::Draining && hw.spindle.empty());
    s.on_frame(frame_of(Type::Ping), 4);
    CHECK(hw.drivers_off == 0);
    s.on_frame(frame_of(Type::Abort), 5);
    CHECK(hw.drain_aborts == 1 && hw.drivers_off == 1 && !s.draining());
  }
  {  // the state machine: it ends when the queue is empty, when the time is up or on an abort
    struct Fake : DrainHardware {
      bool is_busy = true;
      int stops = 0;
      bool busy() override { return is_busy; }
      void stop_all() override { ++stops; is_busy = false; }
    };
    Fake fake;
    Drain d;
    d.configure(1000);
    CHECK(d.begin(0, fake) && d.active());
    d.tick(500, fake);
    CHECK(d.active() && fake.stops == 0);
    fake.is_busy = false;
    d.tick(600, fake);
    CHECK(!d.active() && fake.stops == 1);
    Fake other;
    Drain late;
    late.configure(1000);
    CHECK(late.begin(0, other));
    late.tick(1000, other);
    CHECK(!late.active() && other.stops == 1);   // gives up after the time allowed
    Fake idle;
    idle.is_busy = false;
    Drain none;
    CHECK(!none.begin(0, idle) && !none.active());   // nothing to finish
    Fake third;
    Drain stopped;
    stopped.configure(60000);
    CHECK(stopped.begin(0, third));
    stopped.abort(third);
    CHECK(!stopped.active() && third.stops == 1);
    stopped.abort(third);
    CHECK(third.stops == 1);   // already ended
  }
}

static void queued_spindle_tests() {
  Info info;
  info.queue_capacity = 4096;
  auto cmd = [](unsigned rpm, int dir) {
    return std::vector<std::uint8_t>{static_cast<std::uint8_t>(rpm & 0xFF), static_cast<std::uint8_t>(rpm >> 8), static_cast<std::uint8_t>(dir < 0 ? 0 : dir), 1};
  };
  {  // it needs the machine armed, and waits in the queue instead of acting at once
    FakeHardware hw;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Spindle, cmd(12000, 0)), 1);
    CHECK(hw.last_error() == Error::NotArmed && hw.events.empty());
    s.on_frame(frame_of(Type::Arm), 2);
    s.on_frame(frame_of(Type::Spindle, cmd(12000, 0)), 3);
    CHECK(hw.events.size() == 1 && hw.events[0].first == 12000 && !hw.events[0].second && hw.spindle.empty() && s.spindle_rpm() == 0);
    s.on_frame(frame_of(Type::Spindle, cmd(0, 0)), 4);
    CHECK(hw.events.size() == 2 && hw.events[1].first == 0);
    s.on_frame(frame_of(Type::Spindle, cmd(60000, 0)), 5);   // above the top speed: cut to it
    CHECK(hw.events.back().first == 24000);
  }
  {  // the direction cannot change while the queued spindle turns, nor before the pause between a stop and a start has lasted long enough
    FakeHardware hw;
    hw.queue = ByteQueue(4096);
    Settings st{"t", false, 60000};
    st.reverse_delay_ms = 100;   // 100 ms = 384 bytes of the stream
    Session s(st, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Spindle, cmd(8000, 0)), 2);
    s.on_frame(frame_of(Type::Spindle, cmd(8000, 1)), 3);
    CHECK(hw.last_error() == Error::SpindleDirection && hw.events.size() == 1);
    s.on_frame(frame_of(Type::Write, std::vector<std::uint8_t>(500, 0)), 4);
    s.on_frame(frame_of(Type::Spindle, cmd(0, 0)), 5);
    s.on_frame(frame_of(Type::Write, std::vector<std::uint8_t>(300, 0)), 6);   // 300 bytes of pause: too short
    s.on_frame(frame_of(Type::Spindle, cmd(8000, 1)), 7);
    CHECK(hw.last_error() == Error::SpindleSettling && hw.events.size() == 2);
    s.on_frame(frame_of(Type::Write, std::vector<std::uint8_t>(100, 0)), 8);   // 400 bytes: enough
    s.on_frame(frame_of(Type::Spindle, cmd(8000, 1)), 9);
    CHECK(hw.events.size() == 3 && hw.events[2].second);
  }
  {  // a queue that cannot take another command says so
    FakeHardware hw;
    hw.events_full = true;
    Session s({"t", false, 3000}, info, hw);
    s.on_frame(frame_of(Type::Hello, text("t")), 0);
    s.on_frame(frame_of(Type::Arm), 1);
    s.on_frame(frame_of(Type::Spindle, cmd(8000, 0)), 2);
    CHECK(hw.last_error() == Error::QueueFull);
  }
}

int main() {
  frame_tests();
  queue_tests();
  session_tests();
  spindle_tests();
  home_tests();
  drain_tests();
  queued_spindle_tests();
  std::printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed == 0 ? 0 : 1;
}
