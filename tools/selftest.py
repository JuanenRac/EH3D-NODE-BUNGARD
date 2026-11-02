#!/usr/bin/env python3
"""Hardware-day self-test of the EH3D-NODE-BUNGARD node (standard library only, Windows and Linux).

Read-only by default: version, state, settings, the settings export (compared with itself, never applied), backup networks, OTA state, the control link
(Hello, Info, Status) and the spindle state. Nothing is written to the node, nothing is moved.

With --active (and a typed SI, or --yes) it also runs the tests that arm the machine link and turn the spindle: the spindle by the queue, the Abort and the
panel's stop button, each timed from the order to the stopped state. Every active test ends by stopping the spindle and disarming, and the end of the run
checks it with a fresh connection.

    python tools/selftest.py --host 192.168.0.50 --token TOKEN --user admin --password PASS
    python tools/selftest.py --host 192.168.0.50 --token TOKEN --active

The report selftest_bungard_<time>.txt is written to the current folder: one line per step (OK, FALLO, OMITIDO) with the measured value.
"""
import argparse
import http.client
import json
import os
import socket
import ssl
import struct
import sys
import time

NODE = "bungard"

# ---- COMMON BEGIN (identical in every node repo; do not edit one copy alone) ----
OK = "OK"
FAIL = "FALLO"
SKIP = "OMITIDO"


class Fail(Exception):
    """A step that did not pass."""


class Skip(Exception):
    """A step that could not run (and why)."""


class Report:
    """One line per step. Every text that goes out is cleaned of the secrets the run knows."""

    def __init__(self, node, host):
        self.node, self.host = node, host
        self.secrets = []
        self.lines = []
        self.counts = {OK: 0, FAIL: 0, SKIP: 0}
        self.started = time.strftime("%Y-%m-%d %H:%M:%S")

    def hide(self, *values):
        for v in values:
            if isinstance(v, str) and len(v) >= 4 and v not in self.secrets:
                self.secrets.append(v)

    def clean(self, text):
        text = str(text)
        for s in sorted(self.secrets, key=len, reverse=True):
            text = text.replace(s, "***")
        return text

    def add(self, status, name, detail=""):
        line = "%-9s%s" % (status, name) + (" | " + detail if detail else "")
        line = self.clean(line)
        try:
            print(line, flush=True)
        except (OSError, UnicodeError):
            pass   # a closed pipe or a console that cannot show a character must not stop the run: the report file has the line
        self.lines.append(line)
        self.counts[status] += 1

    def step(self, name, fn):
        """Runs fn; it returns the measured value (text), or raises Fail / Skip."""
        try:
            self.add(OK, name, fn() or "")
        except Skip as e:
            self.add(SKIP, name, str(e))
        except Fail as e:
            self.add(FAIL, name, str(e))
        except Exception as e:  # a broken answer or a dead node is a failed step, never a crash
            self.add(FAIL, name, "%s: %s" % (type(e).__name__, e))

    def text(self):
        head = ["selftest %s" % self.node, "host: %s" % self.host, "started: %s" % self.started, ""]
        tail = ["", "OK=%d FALLO=%d OMITIDO=%d" % (self.counts[OK], self.counts[FAIL], self.counts[SKIP])]
        return self.clean("\n".join(head + self.lines + tail)) + "\n"

    def write(self, out_dir):
        path = os.path.join(out_dir, "%s_%s_%s.txt" % (getattr(self, "prefix", "selftest"), self.node, time.strftime("%Y%m%d_%H%M%S")))
        with open(path, "w", encoding="utf-8") as f:
            f.write(self.text())
        return path


def confirm(prompt, yes=False, ask=input):
    """The interactive gate of every active test: the person types SI. --yes (asked for expressly) skips the question; no answer is a no."""
    print(prompt, flush=True)
    if yes:
        print("(--yes: confirmed by whoever runs this)", flush=True)
        return True
    try:
        return ask("Escribe SI para seguir: ").strip() == "SI"
    except (EOFError, KeyboardInterrupt):
        return False


def secret_values(doc):
    """Every string kept under a key that names a password or a token, so that no report can print one."""
    found = []
    if isinstance(doc, dict):
        for k, v in doc.items():
            if isinstance(v, str) and v and any(w in k.lower() for w in ("password", "token", "secret", "psk", "key")):
                found.append(v)
            else:
                found.extend(secret_values(v))
    elif isinstance(doc, list):
        for v in doc:
            found.extend(secret_values(v))
    return found


class Http:
    """A small HTTP client. It writes nothing to the node unless allow_write is set (the login is the only exception: it opens a session)."""

    def __init__(self, host, port=80, scheme="http", insecure=False, timeout=5.0, bearer=""):
        self.host, self.port, self.scheme, self.insecure, self.timeout, self.bearer = host, int(port), scheme, insecure, timeout, bearer
        self.cookie = ""
        self.allow_write = False

    def request(self, method, path, body=None, headers=None, login=False):
        if method not in ("GET", "HEAD") and not self.allow_write and not login:
            raise RuntimeError("write refused: %s %s needs --active" % (method, path))
        if self.scheme == "https":
            ctx = ssl.create_default_context()
            if self.insecure:
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
            conn = http.client.HTTPSConnection(self.host, self.port, timeout=self.timeout, context=ctx)
        else:
            conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        hdr = {"X-Requested-With": "eh3d"}
        if self.bearer:
            hdr["Authorization"] = "Bearer " + self.bearer
        if self.cookie:
            hdr["Cookie"] = self.cookie
        if headers:
            hdr.update(headers)
        data = None
        if body is not None:
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
            hdr.setdefault("Content-Type", "application/json")
        try:
            conn.request(method, path, body=data, headers=hdr)
            resp = conn.getresponse()
            payload = resp.read()
            for k, v in resp.getheaders():
                if k.lower() == "set-cookie" and "=" in v and "expires=thu, 01 jan 1970" not in v.lower():
                    self.cookie = v.split(";", 1)[0]
            return resp.status, payload
        finally:
            conn.close()

    def get_json(self, path):
        status, payload = self.request("GET", path)
        if status != 200:
            raise Fail("GET %s answered HTTP %d" % (path, status))
        try:
            doc = json.loads(payload.decode("utf-8"))
        except ValueError:
            raise Fail("GET %s did not answer valid JSON" % path)
        if not isinstance(doc, dict):
            raise Fail("GET %s did not answer a JSON object" % path)
        return doc

    def login(self, user, password):
        status, _ = self.request("POST", "/api/v1/login", {"user": user, "password": password}, login=True)
        if status != 200:
            raise Fail("login refused (HTTP %d)" % status)
        if not self.cookie:
            raise Fail("login gave no session")


def check_panel(rep, http, user, password):
    """The read-only battery of the panel API shared by the nodes: version, state, settings, export, backup networks, OTA. Returns the status document or None."""
    box = {}

    def session():
        d = http.get_json("/api/v1/session")
        box["session"] = d
        return "node_id=%s, keys=%d" % (d.get("node_id", "?"), len(d))

    rep.step("panel: session (no login)", session)
    if not user or not password:
        for n in ("panel: login", "panel: status", "panel: settings", "panel: settings export is repeatable", "panel: backup Wi-Fi networks", "panel: OTA state"):
            rep.add(SKIP, n, "give --user and --password (or EH3D_USER / EH3D_PASSWORD)")
        return None

    rep.step("panel: login", lambda: http.login(user, password) or "session opened")
    if not http.cookie:
        return None

    def status():
        d = http.get_json("/api/v1/status")
        box["status"] = d
        if not d.get("version"):
            raise Fail("the status has no version")
        return "version=%s uptime_s=%s ip=%s" % (d.get("version"), d.get("uptime_s", "?"), d.get("ip", "?"))

    rep.step("panel: status", status)

    def settings():
        d = http.get_json("/api/v1/config")
        return "%d sections: %s" % (len(d), ",".join(sorted(d)[:12]))

    rep.step("panel: settings", settings)

    def export():
        a = http.get_json("/api/v1/config/export")
        rep.hide(*secret_values(a))
        b = http.get_json("/api/v1/config/export")
        if a != b:
            raise Fail("two exports in a row differ")
        # what the import would send back: the same document, written and read again, must be the same (nothing is applied here)
        if json.loads(json.dumps(a, sort_keys=True)) != a:
            raise Fail("the export does not survive a write/read round trip")
        box["export"] = a
        return "identical twice, round trip equal, %d sections, nothing applied" % len(a)

    rep.step("panel: settings export is repeatable", export)

    def backup():
        doc = box.get("export")
        if doc is None:
            raise Skip("no export to read")
        sta = doc.get("sta")
        if not isinstance(sta, dict):
            raise Fail("the export has no sta section")
        nets = sta.get("backup", [])
        if not isinstance(nets, list) or any(not isinstance(n, dict) or not n.get("ssid") for n in nets):
            raise Fail("the backup network list is malformed")
        return "%d backup networks saved" % len(nets)

    rep.step("panel: backup Wi-Fi networks", backup)

    def ota():
        d = http.get_json("/api/v1/ota/progress")
        return "state=%s" % d.get("state", "?")

    rep.step("panel: OTA state", ota)
    return box.get("status")


def add_common_args(p, default_port):
    p.add_argument("--host", required=True, help="address of the node")
    p.add_argument("--port", type=int, default=default_port, help="HTTP port of the node (default %d)" % default_port)
    p.add_argument("--https", action="store_true", help="use HTTPS (add --insecure for the node's own certificate)")
    p.add_argument("--insecure", action="store_true", help="do not verify the certificate")
    p.add_argument("--token", default=os.environ.get("EH3D_TOKEN", ""), help="the node's access token (or the variable EH3D_TOKEN); never printed")
    p.add_argument("--user", default=os.environ.get("EH3D_USER", ""), help="panel user (or EH3D_USER)")
    p.add_argument("--password", default=os.environ.get("EH3D_PASSWORD", ""), help="panel password (or EH3D_PASSWORD); never printed")
    p.add_argument("--timeout", type=float, default=5.0, help="seconds to wait for one answer")
    p.add_argument("--out-dir", default=".", help="where the report goes (default: the current folder)")
    p.add_argument("--active", action="store_true", help="also run the tests that move things or start the spindle (asks for confirmation)")
    p.add_argument("--yes", action="store_true", help="answer the confirmation of --active yes (only when you expressly want it)")
# ---- COMMON END ----


# ---- the control link (docs/PROTOCOL.md) ----
T_HELLO, T_ARM, T_DISARM, T_DTR, T_WRITE, T_ABORT, T_PING, T_SPINDLE = 1, 2, 3, 4, 5, 6, 7, 8
T_INFO, T_STATUS, T_RX, T_ERROR = 0x81, 0x82, 0x83, 0x84
STEP_BYTE_S = 10.0 / 38400.0  # one byte on the line at 38400 baud 8N1
ERRORS = {1: "not logged in", 2: "bad token", 3: "not armed", 4: "queue full", 5: "bad frame", 6: "open control refused", 7: "idle timeout",
          8: "no spindle output", 9: "spindle must stop before reversing", 10: "spindle stopped too recently to reverse", 11: "the node is finishing the job it holds"}


def crc16(data):
    """CRC-16/CCITT-FALSE."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(kind, payload=b""):
    body = bytes([kind]) + struct.pack("<H", len(payload)) + payload
    return b"\xB5" + body + struct.pack("<H", crc16(body))


class Link:
    """One TCP session on the control link. It reads frames and keeps the last Status."""

    def __init__(self, host, port, timeout):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(0.2)
        self.buf = b""
        self.status = None
        self.info = None
        self.errors = []

    def send(self, kind, payload=b""):
        self.sock.sendall(frame(kind, payload))

    def pump(self, wait=0.2):
        """Reads what has arrived (up to `wait` seconds) and parses the frames."""
        end = time.time() + wait
        while True:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    raise Fail("the node closed the link")
                self.buf += chunk
            except socket.timeout:
                pass
            self._parse()
            if time.time() >= end:
                return

    def _parse(self):
        while True:
            at = self.buf.find(b"\xB5")
            if at < 0:
                self.buf = b""
                return
            self.buf = self.buf[at:]
            if len(self.buf) < 6:
                return
            length = struct.unpack("<H", self.buf[2:4])[0]
            if len(self.buf) < 6 + length:
                return
            body, crc = self.buf[1:4 + length], struct.unpack("<H", self.buf[4 + length:6 + length])[0]
            if crc16(body) != crc:
                self.buf = self.buf[1:]
                continue
            kind, payload = body[0], body[3:]
            self.buf = self.buf[6 + length:]
            if kind == T_INFO and len(payload) >= 11:
                self.info = {"version": "%d.%d.%d" % tuple(payload[:3]), "baud": struct.unpack("<I", payload[3:7])[0], "capacity": struct.unpack("<I", payload[7:11])[0]}
            elif kind == T_STATUS and len(payload) >= 15:
                fill, sent = struct.unpack("<II", payload[:8])
                self.status = {"fill": fill, "sent": sent, "armed": payload[8], "lines": payload[9], "rpm": struct.unpack("<H", payload[10:12])[0],
                               "reverse": payload[12], "lamps": payload[13], "flags": payload[14]}
            elif kind == T_ERROR and payload:
                self.errors.append(payload[0])

    def hello(self, token):
        self.send(T_HELLO, token.encode())
        end = time.time() + 3.0
        while time.time() < end and (self.info is None or self.status is None):
            self.pump(0.1)
            if self.errors:
                raise Fail("the node refused the link: %s" % ERRORS.get(self.errors[-1], self.errors[-1]))
        if self.info is None or self.status is None:
            raise Fail("no Info/Status after Hello")

    def wait_for(self, test, limit):
        """Seconds until test(status) holds, or None."""
        t0 = time.time()
        while time.time() - t0 < limit:
            self.pump(0.05)
            if self.status and test(self.status):
                return time.time() - t0
        return None

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def open_link(args, rep):
    link = Link(args.host, args.link_port, args.timeout)
    try:
        link.hello(args.token)
    except Exception:
        link.close()
        raise
    return link


def make_safe(link, wait=1.0):
    """Spindle 0, Abort (queue emptied, drivers off, disarmed), Disarm; then waits a moment for the node to show it. Never raises."""
    try:
        for kind, payload in ((T_SPINDLE, struct.pack("<H", 0)), (T_ABORT, b""), (T_DISARM, b"")):
            link.send(kind, payload)
        link.wait_for(lambda s: s["rpm"] == 0 and not s["armed"] and s["fill"] == 0, wait)
    except (OSError, Fail):
        pass


def check_link(args, rep):
    state = {}

    def hello():
        if not args.token:
            raise Skip("give --token (or EH3D_TOKEN)")
        link = open_link(args, rep)
        try:
            state["info"], state["status"] = dict(link.info), dict(link.status)
        finally:
            link.close()
        i = state["info"]
        return "firmware=%s baud=%d queue=%d" % (i["version"], i["baud"], i["capacity"])

    rep.step("link: Hello, Info and Status", hello)

    def idle():
        s = state.get("status")
        if s is None:
            raise Skip("no link status")
        if s["armed"] or s["rpm"]:
            raise Fail("the machine is armed or the spindle turns at the start (armed=%d rpm=%d)" % (s["armed"], s["rpm"]))
        return "armed=0 spindle_rpm=0 queue=%d lamps=0x%02x" % (s["fill"], s["lamps"])

    rep.step("link: idle at the start (not armed, spindle stopped)", idle)
    return state


def safe_state_check(args, rep, name):
    """A fresh connection: the node must be disarmed with the spindle stopped."""
    def check():
        link = open_link(args, rep)
        try:
            link.pump(0.5)
            s = link.status
            if s["armed"] or s["rpm"] or s["fill"]:
                make_safe(link)
                raise Fail("NOT SAFE: armed=%d rpm=%d queue=%d (a stop was sent again)" % (s["armed"], s["rpm"], s["fill"]))
            return "armed=0 rpm=0 queue=0"
        finally:
            link.close()

    rep.step(name, check)


def run_active(args, rep, ask=input):
    if not args.token:
        rep.add(SKIP, "active tests", "give --token (or EH3D_TOKEN)")
        return
    rpm, secs = args.rpm, args.seconds
    ok = confirm("Voy a ARMAR el enlace de control del nodo y arrancar el husillo a %d rpm durante %g s, tres veces (por la cola, con Abort y con el boton de parada del panel), "
                 "con el nodo conectado a la maquina. Asegurate de que el husillo puede girar libre y de tener el paro de emergencia al alcance. La maquina no se mueve en ningun eje." % (rpm, secs),
                 args.yes, ask)
    if not ok:
        rep.add(SKIP, "active tests", "confirmation not given: nothing active was run")
        return
    def spin_up(link, how):
        link.send(T_ARM)
        link.pump(0.3)
        if not link.status["armed"]:
            raise Fail("the node did not arm (errors: %s)" % [ERRORS.get(e, e) for e in link.errors])
        link.send(T_SPINDLE, struct.pack("<H", rpm) + bytes([0]))
        t = link.wait_for(lambda s: s["rpm"] > 0, 10.0)
        if t is None:
            raise Fail("the spindle never reported turning (%s)" % how)
        return t

    def queued():
        link = open_link(args, rep)
        try:
            link.send(T_ARM)
            link.pump(0.3)
            if not link.status["armed"]:
                raise Fail("the node did not arm")
            total = int(secs / STEP_BYTE_S)
            cap = link.info["capacity"]
            link.send(T_SPINDLE, struct.pack("<HBB", rpm, 0, 1))   # start, applied when the bytes before it have gone out
            sent = 0
            t0 = time.time()
            peak = 0
            last_ping = 0.0
            while sent < total and time.time() - t0 < secs * 3 + 10:
                link.pump(0.05)
                room = cap - link.status["fill"] - 8
                if room >= 256:
                    n = min(512, total - sent, room)
                    link.send(T_WRITE, bytes(n))
                    sent += n
                if time.time() - last_ping > 0.5:
                    link.send(T_PING)
                    last_ping = time.time()
                peak = max(peak, link.status["rpm"])
            link.send(T_SPINDLE, struct.pack("<HBB", 0, 0, 1))     # stop, at the end of the queue
            t_stop = link.wait_for(lambda s: s["fill"] == 0 and s["rpm"] == 0, secs + args.stop_wait)
            if peak == 0:
                raise Fail("the spindle never turned while the queue ran")
            if t_stop is None:
                raise Fail("the spindle did not stop at the end of the queue")
            return "peak=%d rpm, queued %d filler bytes (%.2f s of line), stopped at the end" % (peak, sent, sent * STEP_BYTE_S)
        finally:
            make_safe(link)
            link.close()

    rep.step("active: spindle by the queue (start and stop queued)", queued)
    safe_state_check(args, rep, "active: safe after the queue test")

    def abort():
        link = open_link(args, rep)
        try:
            up = spin_up(link, "abort")
            time.sleep(0.5)
            t0 = time.time()
            link.send(T_ABORT)
            t = link.wait_for(lambda s: s["rpm"] == 0 and not s["armed"], args.stop_wait)
            if t is None:
                raise Fail("Abort did not stop the spindle within %g s" % args.stop_wait)
            return "start to turning %.2f s; Abort to stopped and disarmed %.0f ms" % (up, (time.time() - t0) * 1000)
        finally:
            make_safe(link)
            link.close()

    rep.step("active: Abort stop time", abort)
    safe_state_check(args, rep, "active: safe after Abort")

    def panel_stop():
        if not args.user or not args.password:
            raise Skip("give --user and --password for the panel's stop button")
        h = Http(args.host, args.port, "https" if args.https else "http", args.insecure, args.timeout)
        h.login(args.user, args.password)
        h.allow_write = True
        link = open_link(args, rep)
        try:
            spin_up(link, "panel stop")
            time.sleep(0.5)
            t0 = time.time()
            status, _ = h.request("POST", "/api/v1/machine/stop", {})
            if status != 200:
                raise Fail("the stop button answered HTTP %d" % status)
            t = link.wait_for(lambda s: s["rpm"] == 0 and s["fill"] == 0, args.stop_wait)
            if t is None:
                raise Fail("the stop button did not stop the spindle within %g s" % args.stop_wait)
            return "stop button to stopped %.0f ms" % ((time.time() - t0) * 1000)
        finally:
            make_safe(link)
            link.close()

    rep.step("active: emergency stop from the panel", panel_stop)
    safe_state_check(args, rep, "active: safe at the end of the run")


def main(argv=None, ask=input):
    p = argparse.ArgumentParser(description="Self-test of the EH3D-NODE-BUNGARD node (read-only unless --active).")
    add_common_args(p, 80)
    p.add_argument("--link-port", type=int, default=5800, help="control link port (default 5800)")
    p.add_argument("--rpm", type=int, default=1000, help="spindle speed of the active tests (default 1000)")
    p.add_argument("--stop-wait", type=float, default=5.0, help="seconds to wait for a stop to show (default 5)")
    p.add_argument("--seconds", type=float, default=2.0, help="how long the queued spindle test runs (default 2)")
    args = p.parse_args(argv)
    rep = Report(NODE, args.host)
    rep.hide(args.token, args.password)
    http_ = Http(args.host, args.port, "https" if args.https else "http", args.insecure, args.timeout)
    status = check_panel(rep, http_, args.user, args.password)

    def machine():
        if not http_.cookie:
            raise Skip("needs the panel login")
        m = http_.get_json("/api/v1/machine")
        st = http_.get_json("/api/v1/machine/status")
        if st.get("spindle_rpm", 0) != 0 or st.get("armed"):
            raise Fail("the node says it is armed or the spindle turns (armed=%s rpm=%s)" % (st.get("armed"), st.get("spindle_rpm")))
        return "settings sections=%d; armed=%s spindle_rpm=%s queue=%s draining=%s" % (len(m), st.get("armed"), st.get("spindle_rpm"), st.get("queue"), st.get("draining"))

    rep.step("panel: machine settings and state (spindle stopped)", machine)

    def guard():
        d = (status or {})
        keys = [k for k in d if "guard" in k.lower() or "heart" in k.lower() or "watchdog" in k.lower()]
        if not keys:
            raise Skip("the status carries no guard field; read the log instead")
        return ", ".join("%s=%s" % (k, d[k]) for k in keys)

    rep.step("panel: guard heartbeat", guard)

    def log():
        if not http_.cookie:
            raise Skip("needs the panel login")
        st, payload = http_.request("GET", "/api/v1/log")
        if st != 200:
            raise Fail("log answered HTTP %d" % st)
        text = payload.decode("utf-8", "replace")
        bad = [l for l in text.splitlines() if "panic" in l.lower() or "guru meditation" in l.lower() or "brownout" in l.lower()]
        if bad:
            raise Fail("the log shows %d crash lines" % len(bad))
        return "%d log lines, no panic or brownout" % len(text.splitlines())

    rep.step("panel: log has no crash", log)
    check_link(args, rep)
    rep.add(SKIP, "temperature", "this node reports no temperature; the AUX node does")
    rep.add(SKIP, "G-code / GRBL ($H)", "not served by the node: run it from the EH3D GUI (see docs/HARDWARE_CHECKLIST.md)")
    if args.active:
        run_active(args, rep, ask)
    else:
        rep.add(SKIP, "active tests", "not requested (add --active)")
    path = rep.write(args.out_dir)
    print("report: " + path)
    return 1 if rep.counts[FAIL] else 0


if __name__ == "__main__":
    sys.exit(main())
