#!/usr/bin/env node
/**
 * EH3D-NODE-BUNGARD - tools/panel_shots.mjs
 * Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
 *
 *   node tools/panel_mock.mjs 8092 --user admin:adminpass123 --auto-login      (in another window)
 *   node tools/panel_shots.mjs --url http://127.0.0.1:8092 --out images/screenshots [--theme dark|light] [--lang en]
 *
 * Takes the pictures of the README from the web panel served by the stand-in node: it opens Microsoft Edge without a window, chooses the theme and the language,
 * opens each page and saves a PNG. It uses its own temporary profile and closes only the Edge it started.
 */
import { spawn } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";

const args = Object.fromEntries(process.argv.slice(2).reduce((a, v, i, all) => (v.startsWith("--") ? [...a, [v.slice(2), all[i + 1]]] : a), []));
const url = args.url;
const out = args.out ?? "images/screenshots";
const lang = args.lang ?? "en";
const theme = args.theme ?? "dark";
const edge =
  args.edge ??
  ["C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe", "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe"].find((p) => existsSync(p));
if (!url || !edge) {
  console.error("usage: node tools/panel_shots.mjs --url URL [--out DIR] [--theme dark|light] [--lang en] [--pages a,b,c] [--edge PATH]");
  process.exit(2);
}
const PAGES = (args.pages ?? "overview,status,machine,wifi,update,help").split(",");
const port = 9300 + Math.floor(Math.random() * 500);
const profile = mkdtempSync(path.join(tmpdir(), "eh3d-studio-shots-"));
mkdirSync(out, { recursive: true });
const browser = spawn(edge, [`--remote-debugging-port=${port}`, `--user-data-dir=${profile}`, "--headless=new", "--hide-scrollbars", "--window-size=1400,860", "about:blank"], { stdio: "ignore" });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function targets() {
  for (let i = 0; i < 60; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
      const page = list.find((t) => t.type === "page");
      if (page) return page.webSocketDebuggerUrl;
    } catch {
      /* the browser is still starting */
    }
    await sleep(250);
  }
  throw new Error("the browser did not start");
}

try {
  const ws = new WebSocket(await targets());
  await new Promise((r) => (ws.onopen = r));
  let id = 0;
  const waiting = new Map();
  ws.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.id && waiting.has(d.id)) waiting.get(d.id)(d);
  };
  const send = (method, params = {}) =>
    new Promise((resolve) => {
      const n = ++id;
      waiting.set(n, resolve);
      ws.send(JSON.stringify({ id: n, method, params }));
    });
  await send("Page.enable");
  await send("Emulation.setDeviceMetricsOverride", { width: 1400, height: 860, deviceScaleFactor: 1, mobile: false });
  await send("Page.navigate", { url: `${url}/` });
  await sleep(800);
  // the panel keeps the theme and the language in the storage of the page
  await send("Runtime.evaluate", {
    expression: `localStorage.setItem("eh3d_theme", ${JSON.stringify(theme)}); localStorage.setItem("eh3d_lang", ${JSON.stringify(lang)});`,
  });
  for (const p of PAGES) {
    await send("Page.navigate", { url: `${url}/#/${p}` });
    await send("Page.reload");
    await sleep(1800);
    const shot = await send("Page.captureScreenshot", { format: "png" });
    writeFileSync(path.join(out, `panel-${p}.png`), Buffer.from(shot.result.data, "base64"));
    console.log("saved", p);
  }
  ws.close();
} finally {
  browser.kill();
  await sleep(500);
  try {
    rmSync(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 500 });
  } catch {
    /* Edge still holds a file for a moment; the temporary profile is cleaned with the temporary folder */
  }
}
