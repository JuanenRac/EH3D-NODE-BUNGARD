/* EH3D-NODE-BUNGARD panel - the pages. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
   text.js (the eight languages) is joined in front of this file when the panel is packed, so LANGS and L exist here.
   No framework and no inline style: the panel is served with a strict content-security policy. The panel has no control of the machine:
   it shows the state of the node and edits its settings; moving the machine goes only through the authenticated link. */
"use strict";

// ---- small tools ---------------------------------------------------------------------------------------------------------------------

const $app = document.getElementById("app");
const REQUEST_MARK = "eh3d";   // the value of X-Requested-With that the node asks for on every request that changes something
let lang = 0;
// A text in the panel's language. A key or a language without its text is a mistake of the panel, not something to hide: it is said in the console
// (tests/check_panel_text.py makes sure it cannot happen) and the key is shown.
const t = (key, ...args) => {
  const row = L[key];
  if (!row || typeof row[lang] !== "string") { console.error("missing text: " + key); return key; }
  let text = row[lang];
  args.forEach((a, i) => { text = text.split("{" + i + "}").join(a); });
  return text;
};

function el(tag, attrs, ...kids) {
  const node = document.createElement(tag);
  for (const [name, value] of Object.entries(attrs || {})) {
    if (value === undefined || value === null || value === false) continue;
    if (name === "class") node.className = value;
    else if (name.startsWith("on")) node.addEventListener(name.slice(2), value);
    else if (name in node && name !== "list" && name !== "role") node[name] = value;
    else node.setAttribute(name, value === true ? "" : value);
  }
  const add = kid => {
    if (Array.isArray(kid)) kid.forEach(add);
    else if (kid !== null && kid !== undefined && kid !== false) node.append(kid instanceof Node ? kid : document.createTextNode(String(kid)));
  };
  kids.forEach(add);
  return node;
}

const S = {
  session: null, page: "status", message: null, restartNeeded: false, rebooting: false, busy: false,
  cfg: null, saved: "",                       // the network settings (GET/PUT config) and what was last saved
  machine: null, machineSaved: "",            // the machine settings (GET/PUT machine) and what was last saved
  token: { state: "", value: "" },            // the access token is write-only: "set" (a new one is typed), "clear" (remove it) or "" (leave it)
  problems: { cfg: {}, machine: {} }, touched: new Set(), server: {},   // what is drawn next to the fields; the fields left once; what the node refused
  generated: false,
  status: null,                               // the node's own status (flash slots, network)
  ms: null,                                   // the machine's status, polled
  users: [], scan: { busy: false, list: null, error: "" }, log: { next: 0, text: "" }, channelAuto: 1,
};
const roleName = role => (role === "admin" ? t("roleAdmin") : t("roleViewer"));
const isAdmin = () => S.session && S.session.role === "admin";
const hasEthernet = () => !!(S.session && S.session.ethernet);
const isWired = () => hasEthernet() && S.cfg && S.cfg.uplink === "ethernet";

async function api(method, path, body) {
  let response;
  try {
    response = await fetch("api/v1/" + path, {
      method, credentials: "same-origin",
      headers: Object.assign({ "X-Requested-With": REQUEST_MARK }, body !== undefined ? { "Content-Type": "application/json" } : {}),
      body: body !== undefined ? JSON.stringify(body) : undefined,
    });
  } catch (error) { return { ok: false, status: 0, data: { error: "network" } }; }
  let data = {};
  try { data = await response.json(); } catch (error) { /* an empty answer */ }
  if (response.status === 401 && S.session && S.session.authenticated) { S.session.authenticated = false; start(); }
  return { ok: response.ok, status: response.status, data };
}

const errorText = code => (L["e_" + code] ? t("e_" + code) : t("e_other", String(code || "?")));
const problemText = code => (L["p_" + code] ? t("p_" + code) : String(code));

function pickLanguage(preferred) {
  let code = "";
  try { code = localStorage.getItem("eh3d_lang") || ""; } catch (error) { /* storage may be blocked */ }
  if (!code) code = preferred || (navigator.language || "en").slice(0, 2);
  const index = LANGS.findIndex(l => l[0] === code);
  lang = index >= 0 ? index : 0;
  document.documentElement.lang = LANGS[lang][0];
}

// The theme: the one the browser prefers until the person chooses the other, which is then remembered on this device.
function currentTheme() {
  let saved = "";
  try { saved = localStorage.getItem("eh3d_theme") || ""; } catch (error) { /* storage may be blocked */ }
  if (saved === "light" || saved === "dark") return saved;
  return window.matchMedia && window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
}
function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  document.querySelector("meta[name=color-scheme]").setAttribute("content", theme);
}
const themeName = () => (currentTheme() === "dark" ? t("themeToLight") : t("themeToDark"));   // the button names the theme it switches to
function toggleTheme() {
  const next = currentTheme() === "dark" ? "light" : "dark";
  try { localStorage.setItem("eh3d_theme", next); } catch (error) { /* the choice lasts until the page is closed */ }
  applyTheme(next);
  render();
}
applyTheme(currentTheme());

// ---- the working copies of the settings ------------------------------------------------------------------------------------------------

const target = store => (store === "machine" ? S.machine : S.cfg);
const walk = (store, path, create) => {
  const parts = path.split(".");
  let obj = target(store);
  for (let i = 0; i < parts.length - 1; ++i) {
    const key = /^\d+$/.test(parts[i]) ? Number(parts[i]) : parts[i];
    if (obj[key] === undefined && create) obj[key] = {};
    obj = obj[key];
  }
  const last = parts[parts.length - 1];
  return [obj, /^\d+$/.test(last) ? Number(last) : last];
};
const getValue = (store, path) => { const [o, k] = walk(store, path); return o ? o[k] : undefined; };
function setValue(store, path, value) {
  const [o, k] = walk(store, path, true);
  o[k] = value;
  delete S.problems[store][path];
  if (store === "machine") { delete S.server[path]; revalidateMachine(); }
  paintProblems();
  refreshBar();
}

// What the node is sent. The machine's settings go as they are read; the network's go as the parts the panel edits, so nothing else is overwritten.
function machineBody() {
  const m = S.machine;
  const link = { port: m.link.port, allow_open: m.link.allow_open, idle_ms: m.link.idle_ms, queue_bytes: m.link.queue_bytes, finish_on_loss: m.link.finish_on_loss, max_drain_s: m.link.max_drain_s };
  if (S.token.state === "set") link.token = S.token.value;
  if (S.token.state === "clear") link.clear_token = true;
  return { link, uart: Object.assign({}, m.uart), lamps: Object.assign({}, m.lamps), spindle: Object.assign({}, m.spindle) };
}
function networkBody() {
  const c = S.cfg, ap = c.ap, sta = c.sta, body = { node: { hostname: c.node.hostname }, uplink: c.uplink, ip: Object.assign({}, c.ip),
    ap: { enabled: ap.enabled, ssid: ap.ssid, security: ap.security, channel: ap.channel, hidden: ap.hidden }, sta: { enabled: sta.enabled, ssid: sta.ssid } };
  if (typeof ap.password === "string") body.ap.password = ap.password;
  if (typeof sta.password === "string") body.sta.password = sta.password;
  return body;
}
const isDirty = () => (S.page === "machine" && S.machine ? JSON.stringify(machineBody()) !== S.machineSaved : S.page === "network" && S.cfg ? JSON.stringify(networkBody()) !== S.saved : false);

// ---- checking the machine's settings in the browser, before anything is sent ---------------------------------------------------------------

const NUMBERS = { "link.port": [1, 65535], "link.idle_ms": [500, 60000], "link.queue_bytes": [1024, 4194304], "link.max_drain_s": [10, 3600], "spindle.max_rpm": [0, 60000], "spindle.pwm_hz": [100, 40000], "spindle.reverse_delay_ms": [0, 60000] };
// every pin of the machine settings, and whether -1 (not wired) is allowed for it
const PINS = [["uart.tx", false], ["uart.rx", true], ["uart.dtr", false], ["uart.dcd", false], ["uart.ri", false], ["uart.cts", true], ["uart.dsr", true],
  ["lamps.home_x", true], ["lamps.home_y", true], ["lamps.home_z", true], ["lamps.enable", true], ["lamps.tool", true],
  ["spindle.pwm_pin", true], ["spindle.fwd_pin", true], ["spindle.rev_pin", true]];
const GPIO_LAST = 48;

function validateMachine() {
  const found = {}, used = new Map();
  Object.entries(NUMBERS).forEach(([path, [low, high]]) => {
    const v = getValue("machine", path);
    if (typeof v !== "number" || !Number.isInteger(v)) found[path] = "not_a_number";
    else if (v < low || v > high) found[path] = "out_of_range";
  });
  PINS.forEach(([path, none]) => {
    const v = getValue("machine", path);
    if (typeof v !== "number" || !Number.isInteger(v)) { found[path] = "not_a_number"; return; }
    if (v === -1) { if (!none) found[path] = "required"; return; }
    if (v < 0 || v > GPIO_LAST) { found[path] = "out_of_range"; return; }
    used.set(v, (used.get(v) || []).concat(path));
  });
  used.forEach(paths => { if (paths.length > 1) paths.forEach(path => { found[path] = "duplicate_pin"; }); });
  return found;
}
// Shows the problems of the fields the person has left (all of them after a try to save); what the node refused is kept until that field changes.
function revalidateMachine() {
  const found = validateMachine(), shown = {};
  Object.entries(found).forEach(([path, code]) => { if (S.touched.has(path)) shown[path] = code; });
  S.problems.machine = Object.assign({}, S.server, shown);
}

// Writes the problems next to their fields in place, so a field that is being left does not lose the click that left it.
function paintProblems() {
  document.querySelectorAll("[data-store]").forEach(input => {
    const store = input.dataset.store, path = input.dataset.path, code = S.problems[store][path];
    const out = document.getElementById(input.id + "-err");
    if (out) out.textContent = code ? problemText(code) : "";
    if (code) input.setAttribute("aria-invalid", "true"); else input.removeAttribute("aria-invalid");
    input.classList.toggle("bad", !!code);
  });
}

// ---- fields ---------------------------------------------------------------------------------------------------------------------------

const fieldId = (store, path) => "f-" + store + "-" + path.replace(/[^A-Za-z0-9]/g, "-");

// One labelled field of a form. `store` is "cfg" (the network) or "machine"; `path` points into it. A problem is written next to the field and tied to it for screen readers.
function field(labelKey, store, path, options = {}) {
  const current = getValue(store, path), problem = S.problems[store][path];
  const disabled = !isAdmin() || options.disabled, id = fieldId(store, path), errId = id + "-err", hintId = id + "-hint";
  const common = { id, disabled, "data-store": store, "data-path": path, "aria-describedby": (options.hint ? hintId + " " : "") + errId, "aria-invalid": problem ? "true" : undefined };
  const factor = options.factor || 1;
  const tail = [options.hint ? el("span", { class: "hint", id: hintId }, options.hint) : null, el("span", { class: "err", id: errId }, problem ? problemText(problem) : "")];
  let input;
  if (options.type === "checkbox") {
    input = el("input", Object.assign({ type: "checkbox", checked: !!current, onchange: e => { setValue(store, path, e.target.checked); if (options.rerender) render(); } }, common));
    return el("div", { class: "field-box" }, el("label", { class: "check", for: id }, input, el("span", {}, t(labelKey))), tail);
  }
  if (options.type === "select") {
    input = el("select", Object.assign({ onchange: e => { const v = e.target.value; setValue(store, path, options.number ? Number(v) : v); if (options.rerender) render(); } }, common),
      options.options.map(([value, text]) => el("option", { value: String(value), selected: String(current) === String(value) }, text)));
  } else if (options.type === "number") {
    const shown = typeof current === "number" && Number.isFinite(current) ? current / factor : "";
    input = el("input", Object.assign({ type: "number", inputMode: "numeric", min: options.min, max: options.max, step: options.step || 1, value: shown, placeholder: options.placeholder || "",
      oninput: e => setValue(store, path, e.target.value === "" ? NaN : Math.round(Number(e.target.value) * factor)),
      onblur: () => { if (store === "machine") { S.touched.add(path); revalidateMachine(); paintProblems(); } } }, common));
  } else if (options.type === "password") {
    const stored = getValue(store, path.replace(/password$/, "password_set"));
    input = el("input", Object.assign({ type: "password", value: "", autocomplete: "new-password", placeholder: stored ? "••••••••" : "", oninput: e => setValue(store, path, e.target.value) }, common));
  } else {
    input = el("input", Object.assign({ type: "text", value: current === undefined ? "" : current, maxLength: options.max, placeholder: options.placeholder || "", autocomplete: "off",
      oninput: e => setValue(store, path, e.target.value) }, common));
  }
  return el("div", { class: "field" }, el("label", { for: id }, t(labelKey)), input, tail);
}

// ---- the save bar ---------------------------------------------------------------------------------------------------------------------

let barNode = null;
function refreshBar() {
  if (!barNode) return;
  const dirty = isDirty();
  const message = S.message || (dirty ? { kind: "warn", text: t("unsaved") } : S.restartNeeded ? { kind: "warn", text: t("savedRestart") } : null);
  barNode.replaceChildren(...[
    el("span", { class: "msg " + (message ? message.kind : ""), role: "status" }, message ? message.text : ""),
    S.restartNeeded && !dirty ? el("button", { class: "b danger", type: "button", onclick: () => confirm(t("confirmAsk")) && reboot() }, t("restart")) : null,
    dirty ? el("button", { class: "b", type: "button", onclick: discard }, t("discard")) : null,
    dirty ? el("button", { class: "b primary", type: "button", disabled: S.busy, onclick: () => save(false) }, t("save")) : null,
    dirty ? el("button", { class: "b danger", type: "button", disabled: S.busy, onclick: () => save(true) }, t("saveRestart")) : null].filter(Boolean));
  barNode.hidden = !(message || dirty || S.restartNeeded);
}

// The panel's language also lives in the node's settings (ui.language): choosing it here saves it there at once, with no restart. Only an administrator can change settings.
async function persistLanguage() {
  if (!S.cfg || !S.session || !S.session.authenticated || !isAdmin()) return;
  const code = LANGS[lang][0];
  if (S.cfg.ui && S.cfg.ui.language === code) return;
  const r = await api("PUT", "config", { ui: { language: code } });
  if (r.ok && S.cfg.ui) S.cfg.ui.language = code;
}

async function discard() {
  S.token = { state: "", value: "" }; S.generated = false; S.problems = { cfg: {}, machine: {} }; S.server = {}; S.touched = new Set(); S.message = null;
  if (S.page === "machine") await loadMachine();
  else if (S.page === "network") await loadConfig();
  render();
}

async function save(restartAfter) {
  S.busy = true; S.message = { kind: "warn", text: t("working") }; refreshBar();
  const ok = S.page === "machine" ? await saveMachine() : await saveNetwork();
  S.busy = false;
  if (ok && restartAfter && S.restartNeeded) { reboot(); return; }
  render();
}

async function saveMachine() {
  PINS.forEach(([path]) => S.touched.add(path)); Object.keys(NUMBERS).forEach(path => S.touched.add(path));
  revalidateMachine();
  if (Object.keys(S.problems.machine).length) { S.message = { kind: "bad", text: t("e_invalid") }; return false; }
  const r = await api("PUT", "machine", machineBody());
  if (r.ok) {
    S.problems.machine = {}; S.server = {}; S.touched = new Set(); S.token = { state: "", value: "" }; S.generated = false;
    await loadMachine();
    S.restartNeeded = S.restartNeeded || !!r.data.restart_required;
    S.message = { kind: "ok", text: r.data.restart_required ? t("savedRestart") : t("saved") };
    return true;
  }
  if (r.status === 400 && Array.isArray(r.data.problems)) {
    S.server = {};
    r.data.problems.forEach(p => { S.server[p.path] = p.code; });
    revalidateMachine();
    S.message = { kind: "bad", text: t("e_invalid") };
  } else S.message = { kind: "bad", text: errorText(r.data.error) };
  return false;
}

async function saveNetwork() {
  const r = await api("PUT", "config", networkBody());
  if (r.ok) {
    S.problems.cfg = {};
    await loadConfig();
    S.restartNeeded = S.restartNeeded || !!r.data.restart_required;
    S.message = { kind: "ok", text: r.data.restart_required ? t("savedRestart") : t("saved") };
    return true;
  }
  if ((r.status === 422 || r.status === 400) && r.data.problems) {
    S.problems.cfg = {};
    r.data.problems.forEach(p => { S.problems.cfg[p.path] = p.code; });
    S.message = { kind: "bad", text: t("e_invalid") };
  } else S.message = { kind: "bad", text: errorText(r.data.error) };
  return false;
}

async function loadConfig() {
  const r = await api("GET", "config");
  if (!r.ok) return;
  S.cfg = r.data.config; S.channelAuto = r.data.channel_auto;
  S.saved = JSON.stringify(networkBody());
}
async function loadMachine() {
  const r = await api("GET", "machine");
  if (!r.ok) return;
  S.machine = r.data;
  S.machineSaved = JSON.stringify(machineBody());
}

// Waits for the node to answer again after a restart, then loads the panel afresh.
function waitForNode(first) {
  const wait = async () => {
    const r = await api("GET", "session");
    if (r.ok) location.reload(); else setTimeout(wait, 2000);
  };
  setTimeout(wait, first || 4000);
}
async function reboot() {
  await api("POST", "reboot", {});
  S.rebooting = true; render();
  waitForNode(4000);
}

// ---- pages: shared pieces -------------------------------------------------------------------------------------------------------------

const PAGES = [
  { id: "status", icon: "◎", label: "navStatus" },
  { id: "machine", icon: "⚙", label: "navMachine" },
  { id: "network", icon: "⌁", label: "navNetwork" },
  { id: "users", icon: "☺", label: "navUsers" },
  { id: "update", icon: "⇪", label: "navUpdate" },
  { id: "log", icon: "≣", label: "navLog" },
  { id: "system", icon: "▣", label: "navSystem" },
];

const card = (title, ...kids) => el("section", { class: "card" }, title ? el("h2", {}, title) : null, ...kids);
const kv = rows => el("dl", { class: "kv" }, rows.filter(Boolean).flatMap(([k, v]) => [el("dt", {}, k), el("dd", {}, v)]));
const note = (text, kind) => el("p", { class: "note " + (kind || ""), role: kind === "bad" ? "alert" : undefined }, text);
const yesNo = value => t(value ? "yes" : "no");
const pill = (text, kind) => el("span", { class: "pill " + (kind || "") }, text);

function formatUptime(seconds) {
  const d = Math.floor(seconds / 86400), h = Math.floor(seconds % 86400 / 3600), m = Math.floor(seconds % 3600 / 60);
  return (d ? d + " d " : "") + (h || d ? h + " h " : "") + m + " min";
}
const fmtBytes = b => b >= 1048576 ? (Math.round(b / 104857.6) / 10) + " MB" : b >= 1024 ? Math.round(b / 1024) + " kB" : b + " B";

// ---- the status page (live, read-only) --------------------------------------------------------------------------------------------------

const LAMPS = [["lampHomeX", 1, "home"], ["lampHomeY", 2, "home"], ["lampHomeZ", 4, "home"], ["lampEnable", 8, "enable"], ["lampTool", 16, "tool"]];
const LINES = [["lineCts", 1], ["lineDsr", 2], ["lineRi", 4], ["lineDcd", 8]];

function barBox(id, labelId) {
  const fill = el("div", { class: "fill", id: id + "-fill" });
  return el("div", { class: "bar-track", id, role: "progressbar", "aria-labelledby": labelId, "aria-valuemin": "0", "aria-valuemax": "100", "aria-valuenow": "0" }, fill);
}

// The stop button: the node empties its queue and switches the spindle and the drivers off, with or without a program connected.
async function stopMachine() {
  const button = document.getElementById("st-stop");
  if (button) button.disabled = true;
  const r = await api("POST", "machine/stop", {});
  S.message = r.ok ? { kind: "ok", text: t("stStopped") } : { kind: "bad", text: errorText(r.data.error) };
  render();
}

function statusPage() {
  return el("div", { class: "grid" },
    el("div", { class: "span-all" }, note(t("statusNoControl"), "info")),
    el("div", { class: "span-all", id: "st-lost" }),
    el("div", { class: "span-all", id: "st-draining" }),
    isAdmin() ? el("div", { class: "span-all actions" }, el("button", { class: "b danger", type: "button", id: "st-stop", onclick: stopMachine }, t("stStop")), el("span", { class: "hint" }, t("stStopHint"))) : null,
    card(t("stLink"), el("div", { class: "pills" }, el("span", { id: "st-p-link" }), el("span", { id: "st-p-client" }), el("span", { id: "st-p-auth" }), el("span", { id: "st-p-armed" })),
      el("h3", { id: "st-queue-label" }, t("stQueue")), barBox("st-queue", "st-queue-label"),
      kv([[t("stQueueBytes"), el("span", { id: "st-queue-n" }, "—")], [t("stSent"), el("span", { id: "st-sent" }, "—")]])),
    card(t("stLamps"), el("ul", { class: "lamps" }, LAMPS.map(([key, , kind], i) => el("li", {}, el("span", { class: "lamp " + kind, id: "lamp-" + i, "aria-hidden": "true" }), el("span", { class: "lamp-name" }, t(key)), el("span", { class: "lamp-state", id: "lamp-t-" + i }, "—")))),
      el("p", { class: "muted" }, t("stLampsHelp"))),
    card(t("stLines"), el("ul", { class: "lamps lines" }, LINES.map(([key], i) => el("li", {}, el("span", { class: "lamp line", id: "line-" + i, "aria-hidden": "true" }), el("span", { class: "lamp-name" }, t(key)), el("span", { class: "lamp-state", id: "line-t-" + i }, "—")))),
      el("p", { class: "muted" }, t("stLinesHelp"))),
    card(t("stSpindle"), el("p", { class: "big" }, el("span", { id: "sp-rpm" }, "—")), el("p", { class: "muted", id: "sp-dir" }), el("span", { id: "sp-label", class: "hint" }, t("stSpindleBar")), barBox("sp-bar", "sp-label"),
      el("p", { class: "muted", id: "sp-max" })),
    card(t("stNode"), kv([[t("firmware"), el("span", { id: "st-version" }, "—")], [t("uptime"), el("span", { id: "st-uptime" }, "—")], [t("address"), el("span", { id: "st-ip" }, "—")]])));
}

function setBar(id, fraction) {
  const box = document.getElementById(id), fill = document.getElementById(id + "-fill");
  if (!box || !fill) return;
  const pct = Math.max(0, Math.min(100, Math.round(fraction * 100)));
  fill.style.width = pct + "%";
  box.setAttribute("aria-valuenow", String(pct));
}
function setText(id, text) { const node = document.getElementById(id); if (node && node.textContent !== text) node.textContent = text; }
function setPill(id, text, kind) { const node = document.getElementById(id); if (node) node.replaceChildren(pill(text, kind)); }

// Draws the polled values into the status page without rebuilding it.
function patchStatus() {
  const lost = document.getElementById("st-lost");
  if (!lost) return;
  const m = S.ms;
  lost.replaceChildren(...(m ? [] : [note(t("stLost"), "bad")]));
  if (!m) return;
  setPill("st-p-link", t(m.link_up ? "stLinkUp" : "stLinkDown"), m.link_up ? "ok" : "bad");
  setPill("st-p-client", t(m.client ? "stClientYes" : "stClientNo"), m.client ? "ok" : "");
  setPill("st-p-auth", t(m.authenticated ? "stAuthYes" : "stAuthNo"), m.authenticated ? "ok" : "");
  setPill("st-p-armed", t(m.armed ? "stArmed" : "stDisarmed"), m.armed ? "warn" : "");
  const draining = document.getElementById("st-draining");
  if (draining) draining.replaceChildren(...(m.draining ? [note(t("stDraining"), "bad")] : []));
  const capacity = S.machine ? S.machine.link.queue_bytes : 0;
  setBar("st-queue", capacity ? m.queue / capacity : 0);
  setText("st-queue-n", capacity ? m.queue + " / " + capacity + " B" : m.queue + " B");
  setText("st-sent", String(m.sent));
  LAMPS.forEach(([, bit], i) => {
    const on = !!(m.lamps & bit), lamp = document.getElementById("lamp-" + i);
    if (lamp) lamp.classList.toggle("on", on);
    setText("lamp-t-" + i, t(on ? "lampOn" : "lampOff"));
  });
  LINES.forEach(([, bit], i) => {
    const on = !!(m.lines & bit), lamp = document.getElementById("line-" + i);
    if (lamp) lamp.classList.toggle("on", on);
    setText("line-t-" + i, t(on ? "lampOn" : "lampOff"));
  });
  const max = S.machine ? S.machine.spindle.max_rpm : 0;
  setText("sp-rpm", m.spindle_rpm + " rpm");
  setText("sp-dir", m.spindle_rpm > 0 ? t(m.spindle_reverse ? "spReverse" : "spForward") : t("spStopped"));
  setBar("sp-bar", max ? m.spindle_rpm / max : 0);
  setText("sp-max", max ? t("spMax", max) : t("spNoSpindle"));
  setText("st-version", m.version); setText("st-ip", m.ip || "—"); setText("st-uptime", formatUptime(m.uptime_s));
}

// ---- the machine page ----------------------------------------------------------------------------------------------------------------

// The access token is write-only: the node says only whether one is set. A new one is typed (or generated and shown once), or the old one is cleared; both are sent with Save.
function tokenField() {
  const link = S.machine.link, pending = S.token.state, problem = S.problems.machine["link.token"];
  // typing is enough to have a new token sent with Save; "Set token" only makes the indicator say so
  const draft = el("input", { type: "password", id: "token-input", autocomplete: "new-password", disabled: !isAdmin(), value: pending === "set" ? S.token.value : "", "aria-describedby": "token-state token-hint token-err",
    "aria-invalid": problem ? "true" : undefined, oninput: e => { S.token = { state: e.target.value.trim() ? "set" : "", value: e.target.value.trim() }; S.generated = false; delete S.server["link.token"]; revalidateMachine(); refreshBar(); } });
  const setIt = () => {
    if (!draft.value.trim()) { S.server["link.token"] = "required"; revalidateMachine(); render(); return; }
    delete S.server["link.token"]; S.token = { state: "set", value: draft.value.trim() }; revalidateMachine(); render();
  };
  const generate = () => {
    const bytes = new Uint8Array(24);
    crypto.getRandomValues(bytes);
    S.token = { state: "set", value: Array.from(bytes, b => b.toString(16).padStart(2, "0")).join("") }; S.generated = true; delete S.server["link.token"]; revalidateMachine(); render();
  };
  const state = pending === "set" ? t("tokenWillChange") : pending === "clear" ? t("tokenWillClear") : link.token_set ? t("tokenIsSet") : t("tokenNotSet");
  return el("div", { class: "field" }, el("label", { for: "token-input" }, t("fToken")),
    el("p", { class: "token-state", id: "token-state", role: "status" }, pill(state, pending ? "warn" : link.token_set ? "ok" : "bad")),
    pending === "set" && S.generated ? el("p", { class: "note info" }, t("tokenShown"), " ", el("code", { class: "mono" }, S.token.value)) : null,
    draft, el("span", { class: "hint", id: "token-hint" }, t("fTokenHint")), el("span", { class: "err", id: "token-err" }, problem ? problemText(problem) : ""),
    isAdmin() ? el("div", { class: "actions" },
      el("button", { class: "b", type: "button", onclick: setIt }, t("tokenSet")),
      el("button", { class: "b", type: "button", onclick: generate }, t("tokenGenerate")),
      pending ? el("button", { class: "b", type: "button", onclick: () => { S.token = { state: "", value: "" }; S.generated = false; render(); } }, t("tokenKeep"))
        : el("button", { class: "b danger", type: "button", disabled: !link.token_set, onclick: () => { S.token = { state: "clear", value: "" }; S.generated = false; render(); } }, t("tokenClear"))) : null);
}

const pinField = (labelKey, path, ph, none, hint) => field(labelKey, "machine", path, { type: "number", min: none ? -1 : 0, max: GPIO_LAST, placeholder: ph, hint });

function machinePage() {
  if (!S.machine) return el("p", { class: "muted" }, t("loading"));
  const m = S.machine, single = !!m.spindle.dir_single;
  return el("div", {}, note(t("mRestartNote"), "info"), !isAdmin() ? note(t("adminOnly"), "info") : null,
    el("div", { class: "grid wide" },
      card(t("mLink"), el("p", { class: "muted" }, t("mLinkIntro")),
        el("div", { class: "row" }, field("fPort", "machine", "link.port", { type: "number", min: 1, max: 65535, placeholder: "5800", hint: t("fPortHint") }),
          field("fIdle", "machine", "link.idle_ms", { type: "number", min: 0.5, max: 60, step: "any", factor: 1000, placeholder: "3", hint: t("fIdleHint") }),
          field("fQueue", "machine", "link.queue_bytes", { type: "number", min: 1024, max: 4194304, placeholder: "262144", hint: t("fQueueHint") })),
        field("fFinish", "machine", "link.finish_on_loss", { type: "checkbox", rerender: true, hint: t("fFinishHint") }),
        m.link.finish_on_loss ? note(t("finishWarn"), "bad") : null,
        m.link.finish_on_loss ? field("fDrain", "machine", "link.max_drain_s", { type: "number", min: 10, max: 3600, placeholder: "600", hint: t("fDrainHint") }) : null,
        tokenField(),
        field("fAllowOpen", "machine", "link.allow_open", { type: "checkbox", rerender: true, hint: t("fAllowOpenHint") }),
        m.link.allow_open ? note(t("allowOpenWarn"), "bad") : null,
        !m.link.allow_open && !m.link.token_set && S.token.state !== "set" ? note(t("noTokenNote"), "info") : null),
      card(t("mUart"), el("p", { class: "muted" }, t("mUartIntro")),
        el("div", { class: "row" }, pinField("fTx", "uart.tx", "17", false), pinField("fRx", "uart.rx", "18", true, t("fRxHint")), pinField("fDtr", "uart.dtr", "15", false)),
        el("div", { class: "row" }, pinField("fDcd", "uart.dcd", "16", false), pinField("fRi", "uart.ri", "21", false), pinField("fCts", "uart.cts", "-1", true, t("fNoneHint")), pinField("fDsr", "uart.dsr", "-1", true, t("fNoneHint"))),
        field("fInverted", "machine", "uart.rs232_inverted", { type: "checkbox", hint: t("fInvertedHint") }),
        note(t("mUartWiring"), "info")),
      card(t("mLamps"), el("p", { class: "muted" }, t("mLampsIntro")),
        el("div", { class: "row" }, pinField("fHomeX", "lamps.home_x", "1", true), pinField("fHomeY", "lamps.home_y", "2", true), pinField("fHomeZ", "lamps.home_z", "7", true)),
        el("div", { class: "row" }, pinField("fEnable", "lamps.enable", "8", true), pinField("fTool", "lamps.tool", "38", true)),
        field("fActiveLow", "machine", "lamps.active_low", { type: "checkbox", hint: t("fActiveLowHint") }),
        note(t("mLampsWiring"), "info")),
      card(t("mSpindle"), el("p", { class: "muted" }, t("mSpindleIntro")),
        el("div", { class: "row" }, field("fMaxRpm", "machine", "spindle.max_rpm", { type: "number", min: 0, max: 60000, placeholder: "24000", hint: t("fMaxRpmHint") }),
          field("fPwmHz", "machine", "spindle.pwm_hz", { type: "number", min: 100, max: 40000, placeholder: "2000", hint: t("fPwmHzHint") })),
        el("div", { class: "row" }, pinField("fPwmPin", "spindle.pwm_pin", "4", true), pinField(single ? "fRunPin" : "fFwdPin", "spindle.fwd_pin", "5", true), pinField(single ? "fDirPin" : "fRevPin", "spindle.rev_pin", "6", true)),
        field("fPwmInverted", "machine", "spindle.pwm_inverted", { type: "checkbox" }),
        field("fDirSingle", "machine", "spindle.dir_single", { type: "checkbox", rerender: true, hint: t("fDirSingleHint") }),
        field("fOutLow", "machine", "spindle.outputs_active_low", { type: "checkbox" }),
        field("fSettle", "machine", "spindle.reverse_delay_ms", { type: "number", min: 0, max: 60, step: "any", factor: 1000, placeholder: "3", hint: t("fSettleHint") }),
        note(t("mSpindleWiring"), "info"), note(t("mSpindleSafety"), "bad"))));
}

// ---- the network page -----------------------------------------------------------------------------------------------------------------

async function scanNetworks() {
  S.scan = { busy: true, list: null, error: "" }; render();
  const r = await api("GET", "wifi/scan");
  S.scan = r.ok ? { busy: false, list: r.data.networks || [], error: "" } : { busy: false, list: null, error: errorText(r.data.error) };
  render();
}

function scanResults() {
  const scan = S.scan;
  if (scan.busy) return el("p", { class: "muted", role: "status" }, t("scanning"));
  if (scan.error) return el("p", { class: "err" }, scan.error);
  if (!scan.list) return null;
  if (!scan.list.length) return el("p", { class: "muted" }, t("noNetworks"));
  return el("div", { class: "scroll" }, el("table", {}, el("thead", {}, el("tr", {}, el("th", {}, t("ssid")), el("th", {}, t("signal")), el("th", {}, t("wifiChannel")), el("th", {}, t("security")), el("th", {}))),
    el("tbody", {}, scan.list.map(n => el("tr", {}, el("td", {}, n.ssid), el("td", { class: "mono" }, n.rssi + " dBm"), el("td", {}, n.channel), el("td", {}, n.security),
      el("td", {}, el("button", { class: "b", type: "button", disabled: !isAdmin(), onclick: () => { setValue("cfg", "sta.enabled", true); setValue("cfg", "sta.ssid", n.ssid); render(); } }, t("useNetwork"))))))));
}

function currentNetworkCard() {
  const n = S.status && S.status.network;
  if (!n) return null;
  return card(t("netNow"), kv([[t("link"), n.link_up ? t("linkUp") : t("linkDown")], [t("address"), n.has_ip ? n.ip : "—"], [t("netmask"), n.netmask || "—"], [t("gateway"), n.gateway || "—"], [t("mac"), n.mac],
    n.sta_ssid ? [t("station"), n.sta_ssid + (n.sta_connected ? " · " + n.sta_rssi + " dBm" : " · " + t("notConnected"))] : null,
    n.sta_error ? [t("staError"), L["se_" + n.sta_error] ? t("se_" + n.sta_error) : String(n.sta_error)] : null,
    n.ap_active ? [t("apActive"), "“" + n.ap_ssid + "” · " + t("channel") + " " + n.ap_channel + " · " + n.ap_clients + " " + t("apClients") + (n.ap_setup ? " · " + t("setupNetwork") : "")] : null]),
    n.ethernet_available && n.ethernet_ok === false ? note(t("ethernetMissing"), "bad") : null);
}

function networkPage() {
  if (!S.cfg) return el("p", { class: "muted" }, t("loading"));
  const cfg = S.cfg, wired = isWired(), ap = cfg.ap;
  const channels = [[0, t("channelAuto")]].concat(Array.from({ length: 13 }, (_, i) => [i + 1, String(i + 1)]));
  return el("div", {}, note(t("netRestartNote"), "info"),
    el("div", { class: "grid wide" },
      currentNetworkCard(),
      card(t("nodeTitle"), field("hostname", "cfg", "node.hostname", { max: 32, hint: t("hostnameHint") })),
      card(t("netTitle"),
        hasEthernet() ? field("uplink", "cfg", "uplink", { type: "select", rerender: true, options: [["ethernet", t("uplinkEthernet")], ["wifi", t("uplinkWifi")]] }) : null,
        field("dhcp", "cfg", "ip.dhcp", { type: "checkbox", rerender: true }),
        !cfg.ip.dhcp ? el("div", { class: "row" }, field("address", "cfg", "ip.address", { max: 15 }), field("netmask", "cfg", "ip.netmask", { max: 15 }), field("gateway", "cfg", "ip.gateway", { max: 15 }),
          field("dns1", "cfg", "ip.dns1", { max: 15 }), field("dns2", "cfg", "ip.dns2", { max: 15 })) : null,
        wired ? note(t("wiredNote"), "info") : null),
      wired ? null : card(t("wifiSta"), field("staEnable", "cfg", "sta.enabled", { type: "checkbox", rerender: true }),
        cfg.sta.enabled ? el("div", { class: "row" }, field("ssid", "cfg", "sta.ssid", { max: 32 }), field("wifiPassword", "cfg", "sta.password", { type: "password", hint: cfg.sta.password_set ? t("passwordSet") : "" })) : null,
        el("div", { class: "actions" }, el("button", { class: "b", type: "button", disabled: !isAdmin() || S.scan.busy, onclick: scanNetworks }, t("scanNetworks"))), scanResults(),
        el("p", { class: "hint" }, t("scanNote")), note(t("staNote"), "info")),
      card(t("wifiAp"), field("apEnable", "cfg", "ap.enabled", { type: "checkbox", rerender: true }),
        ap.enabled ? [
          el("div", { class: "row" }, field("ssid", "cfg", "ap.ssid", { max: 32 }),
            field("security", "cfg", "ap.security", { type: "select", rerender: true, options: [["wpa2", t("secWpa2")], ["wpa3", t("secWpa3")], ["wpa2wpa3", t("secWpa2Wpa3")], ["open", t("secOpen")]] })),
          ap.security !== "open" ? field("wifiPassword", "cfg", "ap.password", { type: "password", hint: ap.password_set ? t("passwordSet") : "" }) : null,
          field("wifiChannel", "cfg", "ap.channel", { type: "select", number: true, options: channels, hint: ap.channel === 0 ? t("channelNow", S.channelAuto) : "" }),
          field("hidden", "cfg", "ap.hidden", { type: "checkbox" })] : null,
        !wired && !ap.enabled && !cfg.sta.enabled ? note(t("noWifiWarn"), "bad") : null,
        note(t("apNote"), "info"))));
}

// ---- users ----------------------------------------------------------------------------------------------------------------------------

const usersMessage = { text: "", kind: "" };
async function usersAction(promise, done) {
  const r = await promise;
  usersMessage.text = r.ok ? (done || t("saved")) : errorText(r.data.error); usersMessage.kind = r.ok ? "ok" : "bad";
  const list = await api("GET", "users");
  if (list.ok) S.users = list.data;
  render();
  return r;
}

function usersPage() {
  const form = { name: "", password: "", role: "viewer" }, mine = { current: "", password: "" };
  const rows = S.users.map((u, i) => {
    const pw = el("input", { type: "password", id: "upw-" + i, placeholder: t("newPassword"), "aria-label": t("newPassword") + " - " + u.name, autocomplete: "new-password" });
    return el("tr", {}, el("td", {}, u.name, u.name === S.session.user ? el("span", { class: "hint" }, " (" + t("you") + ")") : null),
      el("td", {}, el("select", { "aria-label": t("role") + " - " + u.name, onchange: e => usersAction(api("PUT", "users/" + u.name, { role: e.target.value })) },
        [["admin", t("roleAdmin")], ["viewer", t("roleViewer")]].map(([v, text]) => el("option", { value: v, selected: u.role === v }, text)))),
      el("td", {}, pw), el("td", { class: "actions" }, el("button", { class: "b", type: "button", onclick: () => usersAction(api("PUT", "users/" + u.name, { password: pw.value })) }, t("setPassword")),
        el("button", { class: "b danger", type: "button", onclick: () => confirm(t("confirmAsk")) && usersAction(api("DELETE", "users/" + u.name)) }, t("delete"))));
  });
  const labelled = (key, input) => el("div", { class: "field" }, el("label", {}, t(key), input));
  return el("div", { class: "grid wide" },
    isAdmin() ? card(t("usersTitle"), el("div", { class: "scroll" }, el("table", {}, el("thead", {}, el("tr", {}, el("th", {}, t("user")), el("th", {}, t("role")), el("th", {}, t("newPassword")), el("th", {}))), el("tbody", {}, rows))),
      el("p", { class: "muted" }, t("rolesHelp")),
      el("h3", {}, t("addUser")),
      el("div", { class: "row" }, labelled("user", el("input", { autocomplete: "off", oninput: e => { form.name = e.target.value; } })),
        labelled("newPassword", el("input", { type: "password", autocomplete: "new-password", oninput: e => { form.password = e.target.value; } })),
        labelled("role", el("select", { onchange: e => { form.role = e.target.value; } }, [["viewer", t("roleViewer")], ["admin", t("roleAdmin")]].map(([v, text]) => el("option", { value: v }, text))))),
      el("div", { class: "actions" }, el("button", { class: "b primary", type: "button", onclick: () => usersAction(api("POST", "users", form)) }, t("add"))),
      usersMessage.text ? el("p", { class: usersMessage.kind === "ok" ? "hint" : "err", role: "status" }, usersMessage.text) : null) : null,
    card(t("myAccount"),
      labelled("currentPassword", el("input", { type: "password", autocomplete: "current-password", oninput: e => { mine.current = e.target.value; } })),
      labelled("newPassword", el("input", { type: "password", autocomplete: "new-password", oninput: e => { mine.password = e.target.value; } })),
      el("div", { class: "actions" }, el("button", { class: "b primary", type: "button", onclick: async () => {
        const r = await api("PUT", "account", mine);
        if (r.ok) { alert(t("passwordChanged")); S.session.authenticated = false; start(); } else { usersMessage.text = errorText(r.data.error); usersMessage.kind = "bad"; render(); }
      } }, t("changePassword"))), !isAdmin() && usersMessage.text ? el("p", { class: "err", role: "status" }, usersMessage.text) : null));
}

// ---- update, log, system --------------------------------------------------------------------------------------------------------------

function uploadFirmware(file, progressBar, label, done) {
  const request = new XMLHttpRequest();
  request.open("POST", "api/v1/ota");
  request.setRequestHeader("X-Requested-With", REQUEST_MARK);
  request.upload.onprogress = e => { if (e.lengthComputable) { const pct = Math.round(e.loaded * 100 / e.total); progressBar.style.width = pct + "%"; label.textContent = t("uploading", pct); } };
  request.onload = () => {
    let data = {};
    try { data = JSON.parse(request.responseText); } catch (error) { /* keep {} */ }
    done(request.status === 200 ? { ok: true, version: data.version } : { ok: false, error: data.error || "network" });
  };
  request.onerror = () => done({ ok: false, error: "network" });
  request.send(file);
}

// The two firmware slots and the switch between them: the node boots the other one at the next restart (the way back to the previous version, or forward to the new one).
function slotCard() {
  const parts = ((S.status && S.status.flash && S.status.flash.partitions) || []).filter(p => p.app);
  const other = parts.find(p => !p.running), running = parts.find(p => p.running);
  const usable = !!(other && other.used);
  const result = el("p", { class: "muted", role: "status" });
  return card(t("slotsTitle"), el("p", { class: "muted" }, t("slotsHelp")),
    parts.length ? kv(parts.map(p => [p.label, p.used ? t("slotVersion", p.version) + (p.running ? " · " + t("slotRunning") : p.next_boot ? " · " + t("slotNextBoot") : "") : t("slotEmpty")])) : el("p", { class: "muted" }, t("slotsUnknown")), result,
    el("div", { class: "actions" }, el("button", { class: "b danger", type: "button", disabled: !isAdmin() || !usable, onclick: async () => {
      if (!usable || !confirm(t("switchAsk", other.label, other.version))) return;
      const r = await api("POST", "ota/switch", {});
      if (!r.ok) { result.textContent = errorText(r.data.error); result.className = "err"; return; }
      S.rebooting = true; render(); waitForNode(4000);
    } }, usable ? t("switchSlot", other.label, other.version) : t("switchSlotNone"))));
}

function updatePage() {
  const m = S.ms, progress = el("i"), label = el("p", { class: "muted", role: "status" }), file = el("input", { type: "file", accept: ".bin", id: "fw-file" });
  const result = el("p", { class: "muted", role: "status" });
  return el("div", { class: "grid wide" },
    card(t("updateTitle"), m ? kv([[t("firmware"), m.version], [t("slot"), (S.status && S.status.partition) || "—"]]) : null, el("p", { class: "muted" }, t("updateHelp")),
      el("div", { class: "field" }, el("label", { for: "fw-file" }, t("chooseFile")), file), el("div", { class: "progress" }, progress), label, result,
      el("div", { class: "actions" }, el("button", { class: "b primary", type: "button", disabled: !isAdmin(), onclick: () => {
        if (!file.files[0]) { result.textContent = t("chooseFirst"); return; }
        uploadFirmware(file.files[0], progress, label, r => {
          if (r.ok) { result.textContent = t("updateDone", r.version); S.rebooting = true; render(); waitForNode(6000); }
          else { result.textContent = errorText(r.error); result.className = "err"; }
        });
      } }, t("upload")))),
    slotCard());
}

// Copies text to the clipboard. navigator.clipboard only exists on a secure page (HTTPS) and the panel is often opened over plain HTTP, so there is a fallback through a hidden text area.
function copyText(text, button) {
  const done = ok => {
    const out = button.querySelector(".copy-note");
    out.textContent = ok ? t("copied") : t("copyFailed"); button.classList.toggle("bad", !ok);
    setTimeout(() => { out.textContent = ""; button.classList.remove("bad"); }, 1800);
  };
  const fallback = () => {
    const area = el("textarea", { class: "offscreen", "aria-hidden": "true", tabIndex: -1 }); area.value = text;
    document.body.appendChild(area); area.select();
    let ok = false;
    try { ok = document.execCommand("copy"); } catch (error) { ok = false; }
    area.remove(); done(ok);
  };
  if (navigator.clipboard && window.isSecureContext) navigator.clipboard.writeText(text).then(() => done(true), fallback); else fallback();
}

function copyButton(getText) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 24 24"); svg.setAttribute("width", "16"); svg.setAttribute("height", "16"); svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("fill", "none"); svg.setAttribute("stroke", "currentColor"); svg.setAttribute("stroke-width", "1.8"); svg.setAttribute("stroke-linecap", "round"); svg.setAttribute("stroke-linejoin", "round");
  svg.innerHTML = '<rect x="8" y="8" width="12" height="12" rx="2"></rect><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"></path>';
  return el("button", { class: "b copy-btn", type: "button", "aria-label": t("copyLog"), title: t("copyLog"), onclick: e => copyText(getText(), e.currentTarget) }, svg, el("span", { class: "copy-note", role: "status" }));
}

function logPage() {
  return el("div", { class: "grid wide" }, card(t("logTitle"), el("pre", { class: "log", id: "log-box", tabIndex: 0 }, S.log.text),
    el("div", { class: "actions" }, el("button", { class: "b", type: "button", onclick: async () => { S.log = { next: 0, text: "" }; await refreshLog(); } }, t("refresh")), copyButton(() => S.log.text))));
}

function systemPage() {
  const m = S.ms, confirmBox = el("input", { id: "reset-confirm", placeholder: "RESET", autocomplete: "off", "aria-describedby": "reset-help" });
  return el("div", { class: "grid wide" },
    card(t("stNode"), kv([[t("nodeId"), S.session.node_id], [t("firmware"), m ? m.version : "—"], [t("uptime"), m ? formatUptime(m.uptime_s) : "—"], [t("address"), m && m.ip ? m.ip : "—"], [t("mac"), S.session.mac || "—"], [t("author"), "JuanenRac · Electro Hobby 3D"], [t("license"), "GPL-3.0-or-later"]])),
    card(t("maintenance"), el("div", { class: "actions" }, el("button", { class: "b", type: "button", disabled: !isAdmin(), onclick: () => confirm(t("confirmAsk")) && reboot() }, t("rebootNode"))),
      el("h3", {}, t("factoryTitle")), el("p", { class: "muted", id: "reset-help" }, t("factoryHelp")),
      el("div", { class: "actions" }, el("label", { class: "sr-only", for: "reset-confirm" }, t("factoryTitle")), confirmBox, el("button", { class: "b danger", type: "button", disabled: !isAdmin(), onclick: async () => {
        const r = await api("POST", "factory-reset", { confirm: confirmBox.value });
        if (r.ok) { S.rebooting = true; render(); setTimeout(() => location.reload(), 6000); } else alert(errorText(r.data.error));
      } }, t("factoryTitle")))));
}

// ---- data, polling and the shell -------------------------------------------------------------------------------------------------------

async function refreshStatus() {
  const r = await api("GET", "machine/status");
  S.ms = r.ok ? r.data : null;
}
async function refreshNodeStatus() {
  const r = await api("GET", "status");
  if (r.ok) S.status = r.data;
}
async function refreshLog() {
  const r = await api("GET", "log?from=" + S.log.next);
  if (!r.ok) return;
  S.log.text = (S.log.text + r.data.text).slice(-24000); S.log.next = r.data.next;
  const box = document.getElementById("log-box");
  if (box) { const bottom = box.scrollTop + box.clientHeight >= box.scrollHeight - 30; box.textContent = S.log.text; if (bottom) box.scrollTop = box.scrollHeight; }
}

let pollTimer = null, tick = 0;
async function poll() {
  if (!S.session || !S.session.authenticated || S.rebooting) return;
  tick += 1;
  if (S.page !== "status" && tick % 3 !== 0) return;   // the status page is read every second, the others every third
  await refreshStatus();
  if (S.page === "status") patchStatus();
  else if (S.page === "log") await refreshLog();
  updatePills();
}

function updatePills() {
  const box = document.getElementById("pills");
  if (!box) return;
  const m = S.ms;
  box.replaceChildren(...(m ? [pill(m.ip || t("linkDown"), m.ip ? "ok" : "bad"), pill(m.client ? t("stClientYes") : t("stClientNo"), m.client ? "ok" : ""), m.armed ? pill(t("stArmed"), "warn") : null, pill("v" + m.version)] : [pill(t("stLost"), "bad")]).filter(Boolean));
}

function shell(content) {
  const nav = el("nav", { class: "nav", "aria-label": t("navMenu") }, PAGES.map(p => el("button", { type: "button", class: p.id === S.page ? "active" : "", "aria-current": p.id === S.page ? "page" : undefined, onclick: () => go(p.id) }, el("span", { class: "ico", "aria-hidden": "true" }, p.icon), el("span", { class: "nav-label" }, t(p.label)))));
  const page = PAGES.find(p => p.id === S.page);
  barNode = el("div", { class: "savebar", hidden: true });
  return el("div", { class: "shell" },
    el("a", { class: "skip", href: "#main", onclick: e => { e.preventDefault(); const m = document.getElementById("main"); if (m) m.focus(); } }, t("skip")),
    // the menu is on the left of a wide screen and on top of a narrow one (a phone): the brand, the tools of the person and the pages
    el("aside", { class: "side" },
      el("div", { class: "brand" }, el("span", { class: "brand-mark", "aria-hidden": "true" }, "EH3D"), el("div", {}, el("strong", {}, t("appName")), el("small", {}, S.session.node_id))),
      el("div", { class: "tools" },
        el("span", { class: "who" }, S.session.user + " · " + roleName(S.session.role)),
        langPicker(true), themeButton(),
        el("button", { class: "b", type: "button", onclick: async () => { await api("POST", "logout", {}); S.session.authenticated = false; start(); } }, t("signOut"))),
      nav),
    el("main", { id: "main", tabIndex: -1 }, el("div", { class: "page-head" }, el("h1", {}, t(page.label)), el("div", { class: "pills", id: "pills" })), content),
    barNode);
}

const themeButton = () => el("button", { class: "b", type: "button", onclick: toggleTheme }, themeName());

function langPicker(persist) {
  return el("select", { "aria-label": t("language"), class: "lang", onchange: e => {
    lang = Number(e.target.value);
    try { localStorage.setItem("eh3d_lang", LANGS[lang][0]); } catch (error) { /* ignore */ }
    document.documentElement.lang = LANGS[lang][0];
    if (persist) persistLanguage();
    render();
  } }, LANGS.map((l, i) => el("option", { value: String(i), selected: i === lang }, l[1])));
}

function go(id) { S.page = id; S.message = S.message && S.message.kind === "ok" ? null : S.message; S.touched = new Set(); location.hash = "#/" + id; }

async function enterPage(id) {
  if (id === "status" || id === "machine") { if (!S.machine) await loadMachine(); }
  if (id === "update" || id === "network") await refreshNodeStatus();
  if (id === "log") await refreshLog();
  if (id === "status") await refreshStatus();
}

function render() {
  const active = document.activeElement, focusId = active && active.id ? active.id : "";
  const selection = active && typeof active.selectionStart === "number" && active.type !== "number" && active.type !== "password" ? [active.selectionStart, active.selectionEnd] : null;
  if (S.rebooting) { $app.replaceChildren(el("div", { class: "center" }, el("div", { class: "login", role: "status" }, el("h1", {}, t("restarting")), el("p", { class: "muted" }, t("loading"))))); return; }
  if (!S.session || S.session.setup) { $app.replaceChildren(setupScreen()); return; }
  if (!S.session.authenticated) { $app.replaceChildren(loginScreen()); return; }
  let content;
  switch (S.page) {
    case "machine": content = machinePage(); break;
    case "network": content = networkPage(); break;
    case "users": content = usersPage(); break;
    case "update": content = updatePage(); break;
    case "log": content = logPage(); break;
    case "system": content = systemPage(); break;
    default: content = statusPage();
  }
  $app.replaceChildren(shell(content));
  document.title = t(PAGES.find(p => p.id === S.page).label) + " - " + t("appName");
  updatePills();
  if (S.page === "status") patchStatus();
  refreshBar();
  const again = focusId ? document.getElementById(focusId) : null;
  if (again && again !== document.activeElement) { again.focus(); if (selection && typeof again.setSelectionRange === "function") try { again.setSelectionRange(selection[0], selection[1]); } catch (error) { /* not a text field */ } }
}

// ---- login and set-up screens -----------------------------------------------------------------------------------------------------------

// A password field with a button to show what was typed: the login and set-up screens are where a mistyped password locks someone out.
function passwordField(labelKey, inputAttrs) {
  const input = el("input", Object.assign({ type: "password" }, inputAttrs));
  const toggle = el("button", { type: "button", class: "b eye-toggle", "aria-pressed": "false", onclick: () => {
    input.type = input.type === "password" ? "text" : "password"; toggle.setAttribute("aria-pressed", input.type === "text" ? "true" : "false");
  } }, t("showShort"));
  toggle.setAttribute("aria-label", t("showPassword"));
  return el("div", { class: "field" }, el("label", { for: inputAttrs.id }, t(labelKey)), el("div", { class: "password-row" }, input, toggle));
}

// The panel remembers, if asked, only the user name (never the password: that belongs to the browser's own password manager).
const REMEMBER_USER_KEY = "eh3d_remember_user";
function rememberedUser() { try { return localStorage.getItem(REMEMBER_USER_KEY) || ""; } catch (error) { return ""; } }
function rememberUserName(on, user) { try { if (on && user) localStorage.setItem(REMEMBER_USER_KEY, user); else localStorage.removeItem(REMEMBER_USER_KEY); } catch (error) { /* storage may be blocked: the panel works without it */ } }

function brandBlock() {
  return el("div", { class: "brand center-brand" }, el("span", { class: "brand-mark", "aria-hidden": "true" }, "EH3D"), el("div", {}, el("strong", {}, t("appName")), el("small", {}, S.session.node_id)));
}

function loginScreen() {
  const form = { user: rememberedUser(), password: "", remember: false };
  let keepUser = form.user !== "";
  const message = el("p", { class: "err", role: "alert" });
  const submit = async e => {
    e.preventDefault();
    const r = await api("POST", "login", form);
    if (r.ok) { rememberUserName(keepUser, form.user.trim()); await start(); return; }
    message.textContent = r.data.error === "too_many_attempts" && r.data.wait_s ? errorText("too_many_attempts") + " (" + r.data.wait_s + " s)" : errorText(r.data.error);
  };
  return el("div", { class: "center" }, el("form", { class: "login", id: "login-form", name: "login", method: "post", action: "#", onsubmit: submit },
    brandBlock(), el("h1", {}, t("signIn")),
    el("div", { class: "field" }, el("label", { for: "login-user" }, t("user")), el("input", { id: "login-user", name: "username", value: form.user, autocomplete: "username", autocapitalize: "none", spellcheck: false, autofocus: true, required: true, oninput: e => { form.user = e.target.value; } })),
    passwordField("password", { id: "login-password", name: "password", autocomplete: "current-password", required: true, oninput: e => { form.password = e.target.value; } }),
    el("label", { class: "check", for: "login-remember-user" }, el("input", { type: "checkbox", id: "login-remember-user", name: "remember_user", checked: keepUser, onchange: e => { keepUser = e.target.checked; if (!keepUser) rememberUserName(false, ""); } }), el("span", {}, t("rememberUser"))),
    el("label", { class: "check", for: "login-remember" }, el("input", { type: "checkbox", id: "login-remember", name: "remember", onchange: e => { form.remember = e.target.checked; } }), el("span", {}, t("rememberMe"))),
    el("small", { class: "muted" }, t("loginNote")),
    message, el("button", { class: "b primary", type: "submit" }, t("signIn")),
    el("div", { class: "row" }, langPicker(false), themeButton())));
}

function setupScreen() {
  const form = { code: "", user: "admin", password: "", language: LANGS[lang][0], wifi_ssid: "", wifi_password: "" };
  const message = el("p", { class: "err", role: "alert" });
  const submit = async e => {
    e.preventDefault();
    form.language = LANGS[lang][0];
    const r = await api("POST", "setup", form);
    if (r.ok) { S.rebooting = true; render(); waitForNode(6000); return; }
    message.textContent = errorText(r.data.error);
  };
  return el("div", { class: "center" }, el("form", { class: "login", id: "setup-form", onsubmit: submit },
    brandBlock(), el("h1", {}, t("setupTitle")),
    el("p", { class: "muted" }, t("setupIntro")), S.session.setup_ssid ? el("p", { class: "hint" }, t("setupWifi") + ": " + S.session.setup_ssid) : null,
    S.session.mac ? el("p", { class: "hint mono" }, t("mac") + ": " + S.session.mac) : null,
    el("div", { class: "field" }, el("label", { for: "setup-code" }, t("setupCode")), el("input", { id: "setup-code", autocomplete: "off", autocapitalize: "characters", spellcheck: false, required: true, oninput: e => { form.code = e.target.value.trim().toUpperCase(); } })),
    el("div", { class: "field" }, el("label", { for: "setup-user" }, t("adminName")), el("input", { id: "setup-user", name: "username", value: "admin", autocomplete: "username", required: true, oninput: e => { form.user = e.target.value; } })),
    passwordField("newPassword", { id: "setup-password", name: "password", autocomplete: "new-password", required: true, oninput: e => { form.password = e.target.value; } }),
    el("h3", {}, t(hasEthernet() ? "setupWifiOptionalTitle" : "setupWifiTitle")), el("p", { class: "hint" }, t(hasEthernet() ? "setupWifiOptionalHelp" : "setupWifiHelp")),
    el("div", { class: "field" }, el("label", { for: "setup-ssid" }, t("ssid")), el("input", { id: "setup-ssid", autocomplete: "off", maxLength: 32, oninput: e => { form.wifi_ssid = e.target.value; } })),
    passwordField("wifiPassword", { id: "setup-wifi-password", autocomplete: "off", oninput: e => { form.wifi_password = e.target.value; } }),
    message, el("button", { class: "b primary", type: "submit" }, t("createAdmin")),
    el("div", { class: "row" }, langPicker(false), themeButton())));
}

// ---- start ----------------------------------------------------------------------------------------------------------------------------

async function start() {
  const r = await api("GET", "session");
  if (!r.ok) { $app.replaceChildren(el("div", { class: "center" }, el("p", { class: "note bad", role: "alert" }, t("unreachable")))); setTimeout(start, 3000); return; }
  S.session = r.data;
  pickLanguage(S.session.language);
  if (S.session.authenticated) {
    await Promise.all([loadConfig(), loadMachine(), refreshStatus()]);
    const users = isAdmin() ? await api("GET", "users") : null;
    if (users && users.ok) S.users = users.data;
    S.page = (location.hash.replace("#/", "") || "status");
    if (!PAGES.some(p => p.id === S.page)) S.page = "status";
    await enterPage(S.page);
  }
  render();
  clearInterval(pollTimer);
  pollTimer = setInterval(poll, 1000);
}

window.addEventListener("hashchange", async () => {
  const id = location.hash.replace("#/", "");
  if (PAGES.some(p => p.id === id) && S.session && S.session.authenticated) { S.page = id; await enterPage(id); render(); }
});
start();
