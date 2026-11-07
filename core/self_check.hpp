// EH3D-NODE-BUNGARD - the node judges its own state and says it with stable codes, so the panel, the server and a person at the bench read the same words.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The judgement is a pure function of a snapshot of the node (what the status already carries) and of its settings: it touches no hardware, so it runs the same on a computer and
// on the chip. Every limit is a setting (`machine.selfcheck`), never a number in the code. The codes are listed in docs/SELF_CHECK.md; a code is never reused for another meaning.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "bungard_config.hpp"

namespace eh3d::bungard {

enum class Severity { Info = 0, Warn = 1, Fail = 2 };

inline const char* severity_text(Severity s) {
  switch (s) {
    case Severity::Warn: return "warn";
    case Severity::Fail: return "fail";
    default: return "info";
  }
}

struct Finding {
  const char* code;     // BND-xnn, stable
  Severity severity;
  const char* what;     // one line in English for the log and the bench
  const char* setting;  // the setting to look at (empty when it is the hardware or the network)
};

// Every code the node can raise, in the order the documents list them (the documentation test reads this table).
inline const std::vector<Finding>& self_check_catalog() {
  static const std::vector<Finding> all = {
      {"BND-N01", Severity::Warn, "the network is not up", ""},
      {"BND-S01", Severity::Warn, "the node has no token and open control is off: it refuses to be controlled", "machine.link.token"},
      {"BND-S02", Severity::Warn, "open control is on: anybody on the network can drive the machine", "machine.link.allow_open"},
      {"BND-S03", Severity::Info, "the node finishes the job it holds when the program goes away", "machine.link.finish_on_loss"},
      {"BND-S04", Severity::Warn, "an armed session may stay silent for longer than the limit before the node stops", "machine.link.idle_ms"},
      {"BND-Q01", Severity::Warn, "the queue for the control unit is almost full", "machine.selfcheck.queue_percent"},
      {"BND-P01", Severity::Fail, "the spindle turns faster than its top speed", "machine.spindle.max_rpm"},
      {"BND-P02", Severity::Info, "this node has no spindle output", "machine.spindle.max_rpm"},
      {"BND-D01", Severity::Info, "the node is finishing the job it holds", ""},
  };
  return all;
}

inline const Finding* find_code(const std::string& code) {
  for (const Finding& f : self_check_catalog()) if (code == f.code) return &f;
  return nullptr;
}

// What the judgement reads of the running node.
struct Snapshot {
  bool network_up = false;
  bool armed = false;
  bool draining = false;
  std::uint32_t queue = 0;           // bytes waiting for the control unit
  std::uint32_t queue_capacity = 0;  // bytes the queue holds
  std::uint32_t spindle_rpm = 0;
  std::uint32_t uptime_s = 0;
};

struct Report {
  bool enabled = true;
  std::vector<const Finding*> findings;
  // ok (nothing above info), warn or fail; off when the check is switched off in the settings.
  const char* overall() const {
    if (!enabled) return "off";
    bool above_info = false, fail = false;
    for (const Finding* f : findings) {
      if (f->severity != Severity::Info) above_info = true;
      if (f->severity == Severity::Fail) fail = true;
    }
    return fail ? "fail" : above_info ? "warn" : "ok";
  }
  // The codes as the status carries them: separated by commas, empty when there are none.
  std::string codes() const {
    std::string out;
    for (const Finding* f : findings) out += (out.empty() ? "" : ",") + std::string(f->code);
    return out;
  }
  bool has(const char* code) const {
    for (const Finding* f : findings) if (std::string(f->code) == code) return true;
    return false;
  }
};

namespace detail {
inline void raise(Report& r, const char* code) {
  if (const Finding* f = find_code(code); f != nullptr) r.findings.push_back(f);
}
}  // namespace detail

inline Report self_check(const Snapshot& now, const config::MachineSettings& m) {
  Report r;
  r.enabled = m.selfcheck.enabled;
  if (!r.enabled) return r;
  const bool grace = now.uptime_s < static_cast<std::uint32_t>(m.selfcheck.grace_s);

  if (!now.network_up && !grace) detail::raise(r, "BND-N01");

  if (m.link.token.empty() && !m.link.allow_open) detail::raise(r, "BND-S01");
  if (m.link.allow_open) detail::raise(r, "BND-S02");
  if (m.link.finish_on_loss) detail::raise(r, "BND-S03");
  if (m.link.idle_ms > m.selfcheck.idle_ms_max) detail::raise(r, "BND-S04");

  // a queue this full: compared in whole numbers, so there is no rounding to argue about
  if (now.queue_capacity > 0 && static_cast<std::uint64_t>(now.queue) * 100u >= static_cast<std::uint64_t>(now.queue_capacity) * static_cast<std::uint64_t>(m.selfcheck.queue_percent)) detail::raise(r, "BND-Q01");

  if (m.spindle.max_rpm <= 0) detail::raise(r, "BND-P02");
  if (now.spindle_rpm > static_cast<std::uint32_t>(m.spindle.max_rpm > 0 ? m.spindle.max_rpm : 0)) detail::raise(r, "BND-P01");

  if (now.draining) detail::raise(r, "BND-D01");
  return r;
}

}  // namespace eh3d::bungard
