// EH3D-NODE-BUNGARD - what the panel does over HTTP for the machine: its settings and its status. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include "machine_api.hpp"

extern "C" {
#include "esp_timer.h"
}
#include "api_shared.hpp"
#include "central_uart.hpp"
#include "core/json.hpp"
#include "lamps.hpp"
#include "link_server.hpp"
#include "network.hpp"
#include "node_store.hpp"

namespace eh3d::machine {

std::string settings_json() { return config::machine_to_json(store::settings().machine, false); }

std::string status_json() {
  const LinkState link = link_state();
  const network::Status net = network::status();
  json::Writer w;
  w.begin_object().field("version", api::version_text()).field("uptime_s", static_cast<long long>(esp_timer_get_time() / 1000000LL)).field("ip", net.ip).field("link_up", net.link_up)
      .field("client", link.client).field("authenticated", link.authenticated).field("armed", link.armed).field("queue", static_cast<long long>(queue_size()))
      .field("sent", static_cast<long long>(bytes_sent())).field("lines", static_cast<int>(input_lines())).field("lamps", static_cast<int>(peek_lamps()))
      .field("spindle_rpm", static_cast<long long>(link.spindle_rpm)).field("spindle_reverse", link.spindle_reverse).field("draining", link.draining).end_object();
  return w.str();
}

PutResult put_settings(std::string_view body, config::Problems& problems, bool& restart_required) {
  problems.clear();
  json::Value document;
  if (!json::parse(body, document) || !document.is_object()) { problems.push_back({"", "not_json"}); return PutResult::kInvalid; }
  // The settings document keeps this object under "machine": the same checks, the same storage and the same answer about a restart as every other setting.
  const std::string wrapped = "{\"machine\":" + std::string(body) + "}";
  switch (api::put_config(wrapped, problems, restart_required)) {
    case api::PutResult::kSaved: return PutResult::kSaved;
    case api::PutResult::kStorage: return PutResult::kStorage;
    case api::PutResult::kInvalid: break;
  }
  for (config::Problem& problem : problems) if (problem.path.compare(0, 8, "machine.") == 0) problem.path.erase(0, 8);
  return PutResult::kInvalid;
}

std::string saved_json(bool restart_required) {
  json::Writer w;
  w.begin_object().field("saved", true).field("restart_required", restart_required).key("problems").begin_array().end_array().end_object();
  return w.str();
}

std::string refused_json(const config::Problems& problems) {
  json::Writer w;
  w.begin_object().field("saved", false).key("problems").raw(api::problems_json(problems)).end_object();
  return w.str();
}

}  // namespace eh3d::machine
