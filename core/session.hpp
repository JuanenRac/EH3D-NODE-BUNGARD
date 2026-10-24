// EH3D-NODE-BUNGARD - the rules of one control session: who may move the machine, and when the node stops.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The node is a thin pipe to the control unit of a CNC drilling and routing machine, so every rule that keeps the machine
// from moving by accident lives here, away from the hardware, where it is tested on a computer:
//  - nothing works before a Hello with the right token; a node with no token refuses control unless the build allows open control;
//  - bytes for the control unit are accepted only while the session is armed, and never more than the queue can hold;
//  - an armed session that goes quiet, or whose connection drops, is stopped: the queue is emptied and the drivers are
//    switched off, so a lost cable cannot leave the machine running the rest of a move;
//  - Abort does the same at once.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "frame.hpp"

namespace eh3d::bungard {

// What the session needs from the hardware (a stand-in in the tests, the UART in the firmware).
class Hardware {
 public:
  virtual ~Hardware() = default;
  virtual std::size_t queue_free() = 0;
  virtual bool enqueue(const std::uint8_t* data, std::size_t n) = 0;
  virtual void flush_queue() = 0;
  // The byte that switches the stepper drivers off (0xC0 for the Bungard control unit), sent ahead of the queue.
  virtual void send_drivers_off() = 0;
  virtual void set_dtr(bool on) = 0;
  // The spindle: speed in rpm (0 = stopped) and the direction. The firmware turns it into the 0-10 V speed signal and the direction outputs.
  virtual void set_spindle(std::uint32_t rpm, bool reverse) = 0;
  // A spindle command that waits in the queue: it is applied when the bytes queued before it have gone out.
  virtual bool spindle_event(std::uint32_t rpm, bool reverse) = 0;
  // The program went away with the machine armed and the setting `link.finish_on_loss` on: asks the firmware to finish what it holds. False
  // when there is nothing left to finish (the session then stops the machine as usual).
  virtual bool begin_drain() = 0;
  // Ends the finishing at once (an Abort or a Disarm during it).
  virtual void abort_drain() = 0;
  // A frame for the program (Info, Error ...).
  virtual void reply(Type type, const std::uint8_t* payload, std::size_t n) = 0;
};

struct Settings {
  std::string token;           // empty: no token set
  bool allow_open = false;     // accept control with no token (a bench build only)
  std::uint32_t idle_ms = 3000;  // an armed session that sends nothing for this long is stopped
  std::uint32_t max_rpm = 24000;  // the speed of the spindle at full signal; 0 = this node has no spindle output
  std::uint32_t reverse_delay_ms = 3000;  // how long a spindle must have been stopped before it may turn the other way
  bool finish_on_loss = false;   // when the program goes away, finish the job already in the queue instead of stopping at once
  bool draining = false;         // the node is already finishing a job from an earlier connection
};

// Time one byte takes on the line at 38400 baud 8N1 (10 bits), in microseconds: the filler bytes of a stream are the pauses of a job.
constexpr std::uint64_t kByteMicros = 10ull * 1000000ull / 38400ull;

struct Info {
  std::uint8_t major = 0, minor = 0, patch = 0;
  std::uint32_t baud = 38400;
  std::uint32_t queue_capacity = 0;
};

class Session {
 public:
  Session(Settings settings, Info info, Hardware& hw) : settings_(std::move(settings)), info_(info), hw_(hw), draining_(settings_.draining) {}

  bool authenticated() const { return authenticated_; }
  bool armed() const { return armed_; }
  bool draining() const { return draining_; }
  std::uint32_t spindle_rpm() const { return spindle_rpm_; }
  std::uint8_t stop_on_home() const { return stop_mask_; }
  bool spindle_reverse() const { return spindle_reverse_; }

  void on_frame(const Frame& f, std::uint32_t now_ms) {
    if (f.type == Type::Hello) { hello(f, now_ms); return; }
    if (!authenticated_) { error(Error::NotAuthenticated); return; }
    last_ms_ = now_ms;
    now_ms_ = now_ms;
    if (draining_) {
      // finishing the job it holds: nothing new is taken; Abort and Disarm end it, a Ping is welcome
      switch (f.type) {
        case Type::Abort: case Type::Disarm: stop(); break;
        case Type::Ping: break;
        default: error(Error::Draining); break;
      }
      return;
    }
    switch (f.type) {
      case Type::Arm: armed_ = true; break;
      case Type::Disarm:
        armed_ = false;
        hw_.flush_queue();
        stop_spindle();
        stop_mask_ = 0;
        queued_rpm_ = 0;
        queued_stop_at_valid_ = false;
        enqueued_ = 0;
        break;
      case Type::Spindle: spindle(f); break;
      case Type::StopOnHome:
        if (f.length >= 1) stop_mask_ = f.payload[0] & 7; else error(Error::BadFrame);
        break;
      case Type::Dtr: if (f.length >= 1) hw_.set_dtr(f.payload[0] != 0); else error(Error::BadFrame); break;
      case Type::Write:
        if (!armed_) { error(Error::NotArmed); break; }
        if (f.length > hw_.queue_free() || !hw_.enqueue(f.payload.data(), f.length)) error(Error::QueueFull);
        else enqueued_ += f.length;
        break;
      case Type::Abort: stop(); break;
      case Type::Ping: break;
      default: error(Error::BadFrame); break;
    }
  }

  // The lamps of the machine (HOME X 1, HOME Y 2, HOME Z 4, ENABLE 8, TOOL 16), read by the firmware every few milliseconds. When one of
  // the axes asked for with StopOnHome comes home, the rest of the queue (the surplus of the search) is dropped at once. The control
  // unit has already stopped the axis by itself: this only saves the wait.
  void on_inputs(std::uint8_t lamps) {
    if (stop_mask_ != 0 && (lamps & stop_mask_) != 0) {
      stop_mask_ = 0;
      hw_.flush_queue();
      hw_.reply(Type::HomeSeen, &lamps, 1);
    }
  }

  // Call regularly (every tens of milliseconds).
  void on_tick(std::uint32_t now_ms) {
    now_ms_ = now_ms;
    if (armed_ && static_cast<std::uint32_t>(now_ms - last_ms_) > settings_.idle_ms) {
      lose_the_program();
      error(Error::IdleTimeout);
    }
  }

  // An emergency stop that does not come over the link (the panel's stop button): everything stops now.
  void emergency_stop() { stop(); }

  // The connection to the program closed.
  void on_disconnect() {
    if (armed_) lose_the_program();
    authenticated_ = false;
  }

 private:
  void hello(const Frame& f, std::uint32_t now_ms) {
    if (settings_.token.empty() && !settings_.allow_open) { error(Error::OpenRefused); return; }
    if (!settings_.token.empty()) {
      // constant-time comparison
      unsigned diff = static_cast<unsigned>(f.length != settings_.token.size());
      for (std::size_t i = 0; i < settings_.token.size(); ++i) {
        const std::uint8_t got = i < f.length ? f.payload[i] : 0;
        diff |= static_cast<unsigned>(got ^ static_cast<std::uint8_t>(settings_.token[i]));
      }
      if (diff != 0) { error(Error::BadToken); return; }
    }
    authenticated_ = true;
    last_ms_ = now_ms;
    std::uint8_t p[11] = {info_.major, info_.minor, info_.patch};
    for (int i = 0; i < 4; ++i) p[3 + i] = static_cast<std::uint8_t>(info_.baud >> (8 * i));
    for (int i = 0; i < 4; ++i) p[7 + i] = static_cast<std::uint8_t>(info_.queue_capacity >> (8 * i));
    hw_.reply(Type::Info, p, sizeof p);
  }

  // The program is gone with the machine armed. By default everything stops; with `finish_on_loss` the node finishes what it holds.
  void lose_the_program() {
    if (settings_.finish_on_loss && hw_.begin_drain()) {
      draining_ = true;
      armed_ = false;
      stop_mask_ = 0;
      return;
    }
    stop();
  }

  void stop() {
    hw_.abort_drain();
    draining_ = false;
    queued_rpm_ = 0;
    queued_stop_at_valid_ = false;
    enqueued_ = 0;
    hw_.flush_queue();
    hw_.send_drivers_off();
    stop_spindle();
    stop_mask_ = 0;
    armed_ = false;
  }

  // The spindle never keeps turning when the machine is stopped, disarmed or the link is lost.
  void stop_spindle() {
    if (spindle_rpm_ == 0) return;
    spindle_rpm_ = 0;
    stopped_ms_ = now_ms_;
    stopped_valid_ = true;
    queued_rpm_ = 0;
    hw_.set_spindle(0, spindle_reverse_);
  }

  // Payload: rpm (u16 LE) and optionally the direction (0 forward, 1 reverse; left out: the last one used).
  void spindle(const Frame& f) {
    if (f.length < 2) { error(Error::BadFrame); return; }
    if (settings_.max_rpm == 0) { error(Error::NoSpindle); return; }
    std::uint32_t rpm = static_cast<std::uint32_t>(f.payload[0]) | (static_cast<std::uint32_t>(f.payload[1]) << 8);
    if (f.length >= 4 && (f.payload[3] & 1) != 0) { queued_spindle(rpm, f); return; }
    if (rpm == 0) { stop_spindle(); return; }
    if (!armed_) { error(Error::NotArmed); return; }
    const bool reverse = f.length >= 3 ? f.payload[2] != 0 : spindle_reverse_;
    if (reverse != spindle_reverse_) {
      if (spindle_rpm_ != 0) { error(Error::SpindleDirection); return; }
      if (stopped_valid_ && static_cast<std::uint32_t>(now_ms_ - stopped_ms_) < settings_.reverse_delay_ms) {
        error(Error::SpindleSettling);
        return;
      }
    }
    if (rpm > settings_.max_rpm) rpm = settings_.max_rpm;  // never above what the spindle was configured for
    spindle_rpm_ = rpm;
    spindle_reverse_ = reverse;
    queued_rpm_ = rpm;
    queued_reverse_ = reverse;
    hw_.set_spindle(rpm, reverse);
  }

  // A spindle command that waits in the queue and is applied when the bytes before it have gone out, so that a job can start and stop the spindle
  // in the right places and still run to its end when the program is gone. The direction rules are the same, judged on the commands queued so far:
  // no change of direction while the queued spindle turns, and the bytes between a queued stop and a start the other way (the pauses of the stream)
  // must last at least the settling time.
  void queued_spindle(std::uint32_t rpm, const Frame& f) {
    if (!armed_) { error(Error::NotArmed); return; }
    const bool reverse = f.length >= 3 ? f.payload[2] != 0 : queued_reverse_;
    if (rpm == 0) {
      if (!hw_.spindle_event(0, queued_reverse_)) { error(Error::QueueFull); return; }
      if (queued_rpm_ != 0) { queued_stop_at_ = enqueued_; queued_stop_at_valid_ = true; }
      queued_rpm_ = 0;
      return;
    }
    if (reverse != queued_reverse_) {
      if (queued_rpm_ != 0) { error(Error::SpindleDirection); return; }
      if (queued_stop_at_valid_ && (enqueued_ - queued_stop_at_) * kByteMicros < static_cast<std::uint64_t>(settings_.reverse_delay_ms) * 1000ull) {
        error(Error::SpindleSettling);
        return;
      }
    }
    if (rpm > settings_.max_rpm) rpm = settings_.max_rpm;
    if (!hw_.spindle_event(rpm, reverse)) { error(Error::QueueFull); return; }
    queued_rpm_ = rpm;
    queued_reverse_ = reverse;
  }

  void error(Error e) {
    const std::uint8_t code = static_cast<std::uint8_t>(e);
    hw_.reply(Type::Error, &code, 1);
  }

  Settings settings_;
  Info info_;
  Hardware& hw_;
  bool authenticated_ = false;
  bool armed_ = false;
  std::uint32_t last_ms_ = 0;
  std::uint32_t now_ms_ = 0;
  std::uint8_t stop_mask_ = 0;
  std::uint32_t spindle_rpm_ = 0;
  bool spindle_reverse_ = false;
  std::uint32_t stopped_ms_ = 0;
  bool stopped_valid_ = false;
  bool draining_ = false;
  // the spindle as it will be once the queue has gone out, and how many bytes have been queued since the machine was armed
  std::uint32_t queued_rpm_ = 0;
  bool queued_reverse_ = false;
  std::uint64_t enqueued_ = 0;
  std::uint64_t queued_stop_at_ = 0;
  bool queued_stop_at_valid_ = false;
};

}  // namespace eh3d::bungard
