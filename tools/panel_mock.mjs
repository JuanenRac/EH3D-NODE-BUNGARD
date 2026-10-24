#!/usr/bin/env node
/**
 * EH3D-NODE-BUNGARD - a stand-in for the node, to work on the web panel without a board.
 * Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
 *
 *   node tools/panel_mock.mjs [port] [options]          (default port 8090)     open http://127.0.0.1:8090/
 *
 *   (no option)                  a fresh node in set-up: the set-up code is TESTCODE
 *   --user admin:adminpass123    starts with an administrator (the panel shows the sign-in); add --viewer name:pass for a second user with the viewer role
 *   --auto-login                 the first visit is already signed in as the first user
 *   --armed                      a program has the machine armed: the node refuses to save, restart or update, as the firmware does
 *   --board s3-eth|s3-wifi       the Waveshare ESP32-S3-ETH (a cable) or the Wi-Fi board (default s3-eth)
 *   --one-slot                   the second firmware slot is empty (the default holds an older version)
 *   --lost                       the machine status never answers (the panel shows "no answer")
 *
 * It serves the panel from panel/ (text.js is joined in front of app.js, as tools/pack_panel.py does) and answers /api/v1 like the firmware does, from memory. It is a development tool:
 * it checks less than the firmware and its numbers are made up. The settings checks (ranges, GPIO 0-48, repeated and reserved pins) answer with the codes of the firmware.
 */
import { createServer } from "node:http";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const panel = path.join(here, "..", "panel");
// the version the mock says is the one of the manifest, as the firmware says its own
const manifestVersion = JSON.parse(readFileSync(path.join(here, "..", "eh3d.project.json"), "utf8")).version;
const argv = process.argv.slice(2);
const flag = name => argv.includes(name);
const valueOf = name => (argv.includes(name) ? argv[argv.indexOf(name) + 1] : "");
const port = Number(argv.find(a => /^\d+$/.test(a)) ?? 8090);
const board = valueOf("--board") || "s3-eth";
const wired = board === "s3-eth";

const users = new Map();
for (const [option, role] of [["--user", "admin"], ["--viewer", "viewer"]]) {
  const given = valueOf(option);
  if (given) { const [name, password] = given.split(":"); users.set(name, { password, role }); }
}
const firstUser = [...users.keys()][0] ?? "";
const sessions = new Map();
const started = Date.now();
const armed = flag("--armed");
let logText = "I (1200) eh3d-node: EH3D Bungard node eh3d-a1b2c3, firmware 0.0.1\nI (1500) eh3d-net: link up\nI (2600) eh3d-net: address 192.168.0.50, gateway 192.168.0.1, netmask 255.255.255.0 (wire)\nI (3100) eh3d-link: listening on port 5800\n";

// the network settings (GET/PUT config) as the firmware keeps them, passwords left out
const config = {
  v: 1, node: { id: "bungard-a1b2c3", name: "Bungard CCD", hostname: "" },
  uplink: wired ? "ethernet" : "wifi", ip: { dhcp: true, address: "", netmask: "255.255.255.0", gateway: "", dns1: "", dns2: "" },
  ap: { enabled: true, ssid: "EH3D-BUNGARD-A1B2C3", security: "wpa2", password_set: true, channel: 0, hidden: false, max_clients: 8, tx_power_dbm: 15, bandwidth_mhz: 20, country: "ES" },
  sta: { enabled: !wired, ssid: wired ? "" : "HomeRouter", password_set: !wired, backup: [] },
  web: { mode: "both", admin_sources: "" }, time: { ntp_enabled: true, ntp: "pool.ntp.org", zone: "CET-1CEST,M3.5.0,M10.5.0/3" }, system: { auto_restart_hours: 0 },
  ui: { language: "en" },
};
// the machine settings (GET/PUT machine)
const machine = {
  link: { port: 5800, token_set: true, allow_open: false, idle_ms: 3000, queue_bytes: 262144, finish_on_loss: false, max_drain_s: 600 },
  uart: { tx: 17, rx: 18, dtr: 15, dcd: 16, ri: 21, cts: -1, dsr: -1, rs232_inverted: true },
  lamps: { home_x: 1, home_y: 2, home_z: 7, enable: 8, tool: 38, active_low: true },
  spindle: { max_rpm: 24000, pwm_pin: 4, pwm_hz: 2000, pwm_inverted: false, fwd_pin: 5, rev_pin: 6, dir_single: false, outputs_active_low: false, reverse_delay_ms: 3000 },
};
let token = "mock-token";

// ---- the checks of the machine settings, with the firmware's codes ------------------------------------------------------------------------------------------

const NUMBERS = { "link.port": [1, 65535], "link.idle_ms": [500, 60000], "link.queue_bytes": [1024, 4194304], "link.max_drain_s": [10, 3600], "spindle.max_rpm": [0, 60000], "spindle.pwm_hz": [100, 40000], "spindle.reverse_delay_ms": [0, 60000] };
const PINS = [["uart.tx", false], ["uart.rx", true], ["uart.dtr", false], ["uart.dcd", false], ["uart.ri", false], ["uart.cts", true], ["uart.dsr", true],
  ["lamps.home_x", true], ["lamps.home_y", true], ["lamps.home_z", true], ["lamps.enable", true], ["lamps.tool", true], ["spindle.pwm_pin", true], ["spindle.fwd_pin", true], ["spindle.rev_pin", true]];
const reserved = gpio => gpio === 19 || gpio === 20 || (gpio >= 26 && gpio <= 37) || (wired && gpio >= 9 && gpio <= 14);
const at = (doc, p) => p.split(".").reduce((o, k) => (o === undefined || o === null ? undefined : o[k]), doc);

function machineProblems(doc) {
  const list = [], used = new Map();
  for (const [p, [low, high]] of Object.entries(NUMBERS)) {
    const v = at(doc, p);
    if (typeof v !== "number" || !Number.isInteger(v)) list.push({ path: p, code: "not_a_number" });
    else if (v < low || v > high) list.push({ path: p, code: "out_of_range" });
  }
  for (const [p, none] of PINS) {
    const v = at(doc, p);
    if (typeof v !== "number" || !Number.isInteger(v)) { list.push({ path: p, code: "not_a_number" }); continue; }
    if (v === -1) { if (!none) list.push({ path: p, code: "required" }); continue; }
    if (v < 0 || v > 48) { list.push({ path: p, code: "out_of_range" }); continue; }
    if (reserved(v)) { list.push({ path: p, code: "reserved_pin" }); continue; }
    used.set(v, (used.get(v) ?? []).concat(p));
  }
  used.forEach(paths => { if (paths.length > 1) paths.forEach(p => list.push({ path: p, code: "duplicate_pin" })); });
  if (typeof at(doc, "link.token") === "string" && at(doc, "link.token").length > 64) list.push({ path: "link.token", code: "too_long" });
  return list;
}

// ---- the live state, made up ------------------------------------------------------------------------------------------------------------------------------

function liveStatus() {
  const seconds = (Date.now() - started) / 1000, step = Math.floor(seconds);
  const spindleOn = step % 20 >= 6 && step % 20 < 16;
  const rpm = spindleOn ? Math.round(Math.min(1, ((step % 20) - 5) / 4) * 18000 / 100) * 100 : 0;
  const connected = step % 40 >= 4;
  return {
    version: manifestVersion, uptime_s: step + 5400, ip: "192.168.0.50", link_up: true, client: connected, authenticated: connected, armed: armed || (connected && step % 40 >= 8),
    queue: connected ? Math.round(12000 + 9000 * Math.sin(seconds / 3)) : 0, sent: connected ? 100000 + step * 380 : 0,
    lines: (step % 6 < 3 ? 8 : 0) | (step % 10 < 5 ? 4 : 0), lamps: (step % 12 < 4 ? 1 : 0) | (step % 12 >= 4 && step % 12 < 8 ? 2 : 0) | (step % 12 >= 8 ? 4 : 0) | (connected ? 8 : 0),
    spindle_rpm: rpm, spindle_reverse: step % 40 >= 20 && rpm > 0, draining: flag("--draining"),
  };
}

const slotRows = () => [
  { label: "ota_0", size: 3145728, app: true, used: 1048576, version: manifestVersion, running: true, next_boot: true },
  { label: "ota_1", size: 3145728, app: true, used: flag("--one-slot") ? 0 : 1040384, version: flag("--one-slot") ? "" : "0.0.0", running: false, next_boot: false },
];
function nodeStatus() {
  return {
    node_id: config.node.id, name: config.node.name, version: manifestVersion, uptime_s: Math.floor((Date.now() - started) / 1000) + 5400, reset_reason: "power_on", heap_free: 182000, partition: "ota_0",
    flash: { total: 16777216, allocated: 9437184, partitions: slotRows() },
    network: { board, ethernet_available: wired, ethernet_ok: true, layout: wired ? "ethernet+ap" : "wifi-station+ap", link_up: true, has_ip: true, ip: "192.168.0.50", netmask: "255.255.255.0", gateway: "192.168.0.1", dns: "192.168.0.1", mac: "34:85:18:a1:b2:c3",
      ap_active: config.ap.enabled, ap_setup: users.size === 0, ap_ssid: config.ap.ssid, ap_channel: 6, ap_clients: 0, sta_connected: !wired, sta_ssid: config.sta.ssid, sta_error: "", sta_rssi: -52 },
  };
}

const json = (response, code, body, headers = {}) => { response.writeHead(code, { "Content-Type": "application/json", "Cache-Control": "no-store", ...headers }); response.end(JSON.stringify(body)); };
const bodyOf = request => new Promise(resolve => { const chunks = []; request.on("data", c => chunks.push(c)); request.on("end", () => resolve(Buffer.concat(chunks))); });
const tokenOf = request => /eh3d_session=([0-9a-f]+)/.exec(request.headers.cookie ?? "")?.[1] ?? "";
const newSession = (name, role) => { const id = [...Array(48)].map(() => "0123456789abcdef"[Math.floor(Math.random() * 16)]).join(""); sessions.set(id, { user: name, role }); return id; };
const cookieHeader = id => ({ "Set-Cookie": `eh3d_session=${id}; Path=/; HttpOnly; SameSite=Strict` });

const server = createServer(async (request, response) => {
  const url = new URL(request.url, "http://x");
  if (request.method === "GET" && ["/", "/index.html"].includes(url.pathname)) { response.writeHead(200, { "Content-Type": "text/html" }); return response.end(readFileSync(path.join(panel, "index.html"))); }
  if (request.method === "GET" && url.pathname === "/style.css") { response.writeHead(200, { "Content-Type": "text/css" }); return response.end(readFileSync(path.join(panel, "style.css"))); }
  if (request.method === "GET" && url.pathname === "/app.js") { response.writeHead(200, { "Content-Type": "text/javascript" }); return response.end(readFileSync(path.join(panel, "text.js"), "utf8") + "\n" + readFileSync(path.join(panel, "app.js"), "utf8")); }
  if (!url.pathname.startsWith("/api/v1/")) return json(response, 404, { error: "not_found" });

  const route = url.pathname.slice(8), method = request.method;
  const raw = await bodyOf(request);
  let body = {};
  if (raw.length && route !== "ota") { try { body = JSON.parse(raw.toString("utf8")); } catch { return json(response, 400, { error: "not_json" }); } }
  let session = sessions.get(tokenOf(request));
  const setup = users.size === 0;
  let extra = {};
  if (!session && flag("--auto-login") && firstUser && route === "session") { const id = newSession(firstUser, users.get(firstUser).role); session = sessions.get(id); extra = cookieHeader(id); }

  if (method === "GET" && route === "session") {
    return json(response, 200, { setup, authenticated: !!session, user: session?.user ?? "", role: session?.role ?? "", node_id: config.node.id, language: config.ui.language, version: manifestVersion, setup_ssid: setup ? "EH3D-BUNGARD-A1B2C3" : "", mac: "34:85:18:a1:b2:c3",
      kind: "bungard", board, cert_sha256: "", ethernet: wired }, extra);
  }
  if (method === "POST" && route === "setup") {
    if (!setup) return json(response, 403, { error: "forbidden" });
    if (body.code !== "TESTCODE") return json(response, 403, { error: "wrong_code" });
    if (!/^[a-z0-9_.-]{3,32}$/.test(body.user ?? "")) return json(response, 422, { error: "invalid_name" });
    if ((body.password ?? "").length < 8) return json(response, 422, { error: "weak_password" });
    users.set(body.user, { password: body.password, role: "admin" });
    return json(response, 200, { ok: true, restart_required: true });
  }
  if (method === "POST" && route === "login") {
    const user = users.get(body.user);
    if (!user || user.password !== body.password) return json(response, 401, { error: "wrong_credentials" });
    return json(response, 200, { ok: true, restart_required: false }, cookieHeader(newSession(body.user, user.role)));
  }
  if (method === "POST" && route === "logout") { sessions.delete(tokenOf(request)); return json(response, 200, { ok: true }); }
  if (setup) return json(response, 403, { error: "setup_required" });
  if (!session) return json(response, 401, { error: "unauthorized" });
  const writes = method !== "GET";
  if (writes && request.headers["x-requested-with"] !== "eh3d") return json(response, 403, { error: "forbidden" });
  const admin = session.role === "admin";
  const needAdmin = () => { if (!admin) { json(response, 403, { error: "forbidden" }); return false; } return true; };
  const writable = () => { if (armed) { json(response, 409, { error: "machine_armed" }); return false; } return true; };

  if (method === "GET" && route === "status") return json(response, 200, nodeStatus());
  if (method === "GET" && route === "machine/status") { if (flag("--lost")) return request.socket.destroy(); return json(response, 200, liveStatus()); }
  if (method === "GET" && route === "machine") return json(response, 200, machine);
  if (method === "PUT" && route === "machine") {
    if (!needAdmin() || !writable()) return;
    const list = machineProblems(body);
    if (list.length) return json(response, 400, { saved: false, problems: list });
    for (const key of ["link", "uart", "lamps", "spindle"]) {
      const { token: given, clear_token: clear, ...rest } = body[key] ?? {};
      Object.assign(machine[key], rest);
      if (key === "link") {
        if (clear === true) { token = ""; machine.link.token_set = false; }
        else if (typeof given === "string" && given !== "") { token = given; machine.link.token_set = true; }
      }
    }
    return json(response, 200, { saved: true, restart_required: true, problems: [] });
  }
  if (method === "GET" && route === "wifi/scan") { if (!needAdmin()) return; return json(response, 200, { networks: [{ ssid: "HomeRouter", rssi: -48, channel: 6, security: "wpa2" }, { ssid: "Neighbour", rssi: -71, channel: 11, security: "wpa2wpa3" }, { ssid: "CafeOpen", rssi: -80, channel: 1, security: "open" }] }); }
  if (method === "GET" && route === "config") return json(response, 200, { config, channel_auto: 6, firmware: manifestVersion });
  if (method === "PUT" && route === "config") {
    if (!needAdmin() || !writable()) return;
    const list = [];
    if (body.ap?.enabled && !String(body.ap.ssid ?? "").trim()) list.push({ path: "ap.ssid", code: "required" });
    if (body.ip && body.ip.dhcp === false && !/^\d+\.\d+\.\d+\.\d+$/.test(body.ip.address ?? "")) list.push({ path: "ip.address", code: body.ip.address ? "invalid" : "required" });
    if (typeof body.ap?.password === "string" && body.ap.password.length > 0 && body.ap.password.length < 8) list.push({ path: "ap.password", code: "invalid_key" });
    if (list.length) return json(response, 422, { error: "invalid", problems: list });
    for (const key of Object.keys(body)) {
      const value = body[key];
      if (value && typeof value === "object" && !Array.isArray(value) && config[key]) Object.assign(config[key], value); else config[key] = value;
    }
    for (const section of [config.ap, config.sta]) if (typeof section.password === "string" && section.password) { section.password_set = true; delete section.password; } else delete section.password;
    return json(response, 200, { ok: true, restart_required: body.ui === undefined || Object.keys(body).length > 1 });
  }
  if (method === "GET" && route === "users") { if (!needAdmin()) return; return json(response, 200, [...users].map(([name, u]) => ({ name, role: u.role }))); }
  if (method === "POST" && route === "users") {
    if (!needAdmin() || !writable()) return;
    if (!/^[a-z0-9_.-]{3,32}$/.test(body.name ?? "")) return json(response, 422, { error: "invalid_name" });
    if ((body.password ?? "").length < 8) return json(response, 422, { error: "weak_password" });
    if (users.has(body.name)) return json(response, 409, { error: "exists" });
    users.set(body.name, { password: body.password, role: body.role === "admin" ? "admin" : "viewer" });
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "PUT" && route.startsWith("users/")) {
    if (!needAdmin() || !writable()) return;
    const user = users.get(route.slice(6));
    if (!user) return json(response, 404, { error: "not_found" });
    if (body.role) user.role = body.role;
    if (body.password) user.password = body.password;
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "DELETE" && route.startsWith("users/")) {
    if (!needAdmin() || !writable()) return;
    const name = route.slice(6);
    if (users.get(name)?.role === "admin" && [...users.values()].filter(u => u.role === "admin").length === 1) return json(response, 409, { error: "last_admin" });
    users.delete(name);
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "PUT" && route === "account") {
    const user = users.get(session.user);
    if (user.password !== body.current) return json(response, 403, { error: "wrong_password" });
    if ((body.password ?? "").length < 8) return json(response, 422, { error: "weak_password" });
    user.password = body.password;
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "GET" && route === "log") return json(response, 200, { next: logText.length, text: logText.slice(Number(url.searchParams.get("from") ?? 0)) });
  if (method === "POST" && route === "ota/switch") {
    if (!needAdmin() || !writable()) return;
    if (flag("--one-slot")) return json(response, 409, { error: "slot_empty" });
    return json(response, 200, { ok: true, restart_required: true, slot: "ota_1", version: "0.0.0" });
  }
  if (method === "POST" && route === "machine/stop") { if (!needAdmin()) return; return json(response, 200, { stopped: true }); }
  if (method === "POST" && route === "reboot") { if (!needAdmin() || !writable()) return; return json(response, 200, { ok: true, restart_required: false }); }
  if (method === "POST" && route === "factory-reset") { if (!needAdmin() || !writable()) return; if (body.confirm !== "RESET") return json(response, 422, { error: "confirm_required" }); users.clear(); return json(response, 200, { ok: true, restart_required: true }); }
  if (method === "POST" && route === "ota") { if (!needAdmin() || !writable()) return; return json(response, 200, { ok: true, restart_required: true, version: "0.0.2", bytes: raw.length, sha256: "0".repeat(64) }); }
  return json(response, 404, { error: "not_found" });
});
server.listen(port, "127.0.0.1", () => console.log(`EH3D Bungard node panel mock on http://127.0.0.1:${port}/ (${users.size ? "users: " + [...users.keys()].join(", ") : "set-up code TESTCODE"}${armed ? ", machine armed" : ""})`));
