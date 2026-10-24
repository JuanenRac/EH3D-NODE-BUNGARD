// EH3D-NODE-BUNGARD - what the panel does over HTTP for the machine: its settings and its status. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The panel has no controls on purpose: moving the machine goes through the authenticated TCP link, never through a page.
#pragma once
#include <string>
#include <string_view>

#include "core/bungard_config.hpp"

namespace eh3d::machine {

// The `machine` settings, as GET /api/v1/machine answers: the link, the serial line, the lamps and the spindle. The token never leaves the node: "link.token_set" says whether there is one.
std::string settings_json();

// {"version","uptime_s","ip","link_up","client","authenticated","armed","queue","sent","lines","lamps","spindle_rpm","spindle_reverse"}
std::string status_json();

enum class PutResult { kSaved, kInvalid, kStorage };
// Applies a (partial) `machine` document on top of the stored settings: checked in full, stored only when nothing is wrong. The problems are reported under the paths of the document
// itself ("link.port", not "machine.link.port"). `restart_required` says that something changed that the hardware takes at start-up.
PutResult put_settings(std::string_view body, config::Problems& problems, bool& restart_required);

std::string saved_json(bool restart_required);              // {"saved":true,"restart_required":..,"problems":[]}
std::string refused_json(const config::Problems& problems);  // {"saved":false,"problems":[{"path","code"}]}

}  // namespace eh3d::machine
