// EH3D-NODE-BUNGARD - finishing the job the node already holds when the program goes away.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// By default a node stops the moment its program goes quiet or the connection drops: the queue is emptied and the stepper drivers and the
// spindle are switched off. A machine that is half way through a job when the server falls over is then left with a half-drilled board. With
// the setting `link.finish_on_loss` (off by default) the node instead **finishes what it holds**: the bytes already in its queue, and the spindle
// commands queued among them, run to the end - the program has cut the job so that this end is a safe place (Z up, between two holes) - and then
// the node switches the spindle and the drivers off and disarms. It never takes anything new while it does so, it gives up after
// `link.max_drain_s` seconds whatever is left, and an Abort (from the program that comes back, the panel's stop button) ends it at once.
//
// This file is the little state machine; it touches no hardware, so it is tested on a computer.
#pragma once

#include <cstdint>

namespace eh3d::bungard {

// What the drain needs from the hardware (a stand-in in the tests, the UART queue and the spindle in the firmware).
class DrainHardware {
 public:
  virtual ~DrainHardware() = default;
  // Bytes still waiting for the control unit, or spindle commands still waiting among them.
  virtual bool busy() = 0;
  // Empties the queue and switches the stepper drivers and the spindle off.
  virtual void stop_all() = 0;
};

class Drain {
 public:
  void configure(std::uint32_t max_ms) { max_ms_ = max_ms; }
  bool active() const { return active_; }
  // Starts finishing the job; false (and nothing started) when there is nothing left to finish.
  bool begin(std::uint32_t now_ms, DrainHardware& hw) {
    if (active_) return true;
    if (!hw.busy()) return false;
    active_ = true;
    started_ms_ = now_ms;
    return true;
  }
  // Call every few tens of milliseconds, also when no program is connected.
  void tick(std::uint32_t now_ms, DrainHardware& hw) {
    if (!active_) return;
    if (!hw.busy() || static_cast<std::uint32_t>(now_ms - started_ms_) >= max_ms_) {
      hw.stop_all();
      active_ = false;
    }
  }
  // An Abort, or anything else that must stop the machine now.
  void abort(DrainHardware& hw) {
    if (!active_) return;
    hw.stop_all();
    active_ = false;
  }

 private:
  bool active_ = false;
  std::uint32_t started_ms_ = 0;
  std::uint32_t max_ms_ = 600000;
};

}  // namespace eh3d::bungard
