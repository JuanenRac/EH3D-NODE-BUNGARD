"""Tests of tools/selftest.py against a toy node (a thread with an HTTP server and a control-link server)."""
import contextlib
import http.server
import io
import json
import os
import socket
import struct
import tempfile
import threading
import time
import unittest

import selftest as st

TOKEN = "toy-token-0123456789"
PASSWORD = "toy-password-xyz"
WIFI_SECRET = "wifi-secret-zzz"


class ToyLink:
    """The control link of a node: Hello, Arm, Spindle (also at the end of the queue), Write, Abort, Disarm and a Status every 100 ms."""

    def __init__(self, deaf=False, rate=40000):
        self.deaf, self.rate = deaf, rate
        self.lock = threading.Lock()
        self.armed, self.rpm, self.fill, self.sent, self.pending = 0, 0, 0, 0, []
        self.seen = []
        self.sock = socket.socket()
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(4)
        self.port = self.sock.getsockname()[1]
        self.alive = True
        threading.Thread(target=self._accept, daemon=True).start()

    def stop_now(self):
        with self.lock:
            self.fill, self.pending, self.rpm, self.armed = 0, [], 0, 0

    def _accept(self):
        while self.alive:
            try:
                conn, _ = self.sock.accept()
            except OSError:
                return
            threading.Thread(target=self._serve, args=(conn,), daemon=True).start()

    def _status(self):
        return st.frame(st.T_STATUS, struct.pack("<IIBBHBBB", self.fill, self.sent, self.armed, 0, self.rpm, 0, 0, 0))

    def _tick(self, dt):
        with self.lock:
            d = min(self.fill, int(self.rate * dt))
            self.fill -= d
            self.sent += d
            for p in list(self.pending):
                if p[0] <= self.sent:
                    self.rpm = p[1]
                    self.pending.remove(p)

    def _serve(self, conn):
        conn.settimeout(0.05)
        buf, ok, last = b"", False, time.time()
        while self.alive:
            try:
                data = conn.recv(8192)
                if not data:
                    return
                buf += data
            except socket.timeout:
                pass
            except OSError:
                return
            while len(buf) >= 6 and buf[0] == 0xB5:
                n = struct.unpack("<H", buf[2:4])[0]
                if len(buf) < 6 + n:
                    break
                kind, payload = buf[1], buf[4:4 + n]
                buf = buf[6 + n:]
                self.seen.append(kind)
                if kind == st.T_HELLO:
                    if payload.decode() != TOKEN:
                        conn.sendall(st.frame(st.T_ERROR, bytes([2])))
                        return
                    ok = True
                    conn.sendall(st.frame(st.T_INFO, bytes([0, 8, 0]) + struct.pack("<II", 38400, 4096)))
                elif not ok:
                    conn.sendall(st.frame(st.T_ERROR, bytes([1])))
                elif kind == st.T_ARM:
                    with self.lock:
                        self.armed = 1
                elif self.deaf and kind in (st.T_ABORT, st.T_DISARM):
                    pass
                elif kind in (st.T_ABORT, st.T_DISARM):
                    self.stop_now()
                elif kind == st.T_WRITE:
                    with self.lock:
                        if self.armed:
                            self.fill += len(payload)
                elif kind == st.T_SPINDLE:
                    rpm = struct.unpack("<H", payload[:2])[0]
                    queued = len(payload) >= 4 and payload[3] & 1
                    if self.deaf and rpm == 0:
                        continue
                    with self.lock:
                        if rpm and not self.armed:
                            continue
                        if queued:
                            self.pending.append((self.sent + self.fill, rpm))
                        else:
                            self.rpm = rpm
            now = time.time()
            self._tick(now - last)
            last = now
            if ok:
                try:
                    conn.sendall(self._status())
                except OSError:
                    return

    def close(self):
        self.alive = False
        self.sock.close()


class ToyNode:
    """The panel API of a node, with modes: ok, error (HTTP 500), broken (not JSON)."""

    def __init__(self, link=None, mode="ok"):
        self.mode, self.link, self.writes = mode, link, []
        node = self

        class H(http.server.BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def _send(self, code, doc, cookie=False):
                body = doc if isinstance(doc, bytes) else json.dumps(doc).encode()
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                if cookie:
                    self.send_header("Set-Cookie", "eh3d_session=abc123; Path=/")
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                path = self.path.split("?")[0]
                if node.mode == "error" and path != "/api/v1/session":
                    return self._send(500, {"error": "boom"})
                if node.mode == "broken" and path != "/api/v1/session":
                    return self._send(200, b"{not json")
                if path == "/api/v1/session":
                    return self._send(200, {"node_id": "toy", "authenticated": "eh3d_session" in (self.headers.get("Cookie") or "")})
                if "eh3d_session" not in (self.headers.get("Cookie") or ""):
                    return self._send(401, {"error": "login"})
                docs = {
                    "/api/v1/status": {"version": "0.0.2", "uptime_s": 12, "ip": "127.0.0.1"},
                    "/api/v1/config": {"node": {"name": "toy"}, "sta": {"enabled": True}},
                    "/api/v1/config/export": {"node": {"name": "toy"}, "sta": {"enabled": True, "password": WIFI_SECRET, "backup": [{"ssid": "workshop", "password": WIFI_SECRET}]}},
                    "/api/v1/machine": {"link": {"port": 5800, "token_set": True}},
                    "/api/v1/machine/status": {"armed": bool(node.link.armed) if node.link else False, "spindle_rpm": node.link.rpm if node.link else 0, "queue": 0, "draining": False},
                    "/api/v1/ota/progress": {"state": "idle"},
                }
                if path == "/api/v1/log":
                    body = b"boot ok\nlink up\n"
                    self.send_response(200)
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                    return
                if path in docs:
                    return self._send(200, docs[path])
                self._send(404, {"error": "not_found"})

            def do_POST(self):
                n = int(self.headers.get("Content-Length") or 0)
                body = self.rfile.read(n)
                path = self.path.split("?")[0]
                node.writes.append(path)
                if path == "/api/v1/login":
                    doc = json.loads(body)
                    if doc.get("password") == PASSWORD:
                        return self._send(200, {"ok": True}, cookie=True)
                    return self._send(401, {"error": "wrong_credentials"})
                if path == "/api/v1/machine/stop":
                    if node.link:
                        node.link.stop_now()
                    return self._send(200, {"stopped": True})
                self._send(404, {"error": "not_found"})

        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def run(argv, ask=lambda _: "SI"):
    """Runs the tool; returns (exit code, printed text, report text)."""
    out = io.StringIO()
    with tempfile.TemporaryDirectory() as d, contextlib.redirect_stdout(out):
        code = st.main(argv + ["--out-dir", d], ask=ask)
        files = [f for f in os.listdir(d) if f.startswith("selftest_bungard_")]
        report = ""
        if files:
            with open(os.path.join(d, files[0]), encoding="utf-8") as f:
                report = f.read()
    return code, out.getvalue(), report


def base(node, link=None, extra=()):
    a = ["--host", "127.0.0.1", "--port", str(node.port), "--user", "admin", "--password", PASSWORD, "--timeout", "1", "--stop-wait", "0.6"]
    if link:
        a += ["--token", TOKEN, "--link-port", str(link.port)]
    return a + list(extra)


class SelfTest(unittest.TestCase):
    def test_crc_and_frame(self):
        self.assertEqual(st.crc16(b"123456789"), 0x29B1)
        f = st.frame(st.T_PING)
        self.assertEqual(f[0], 0xB5)
        self.assertEqual(len(f), 6)

    def test_read_only_pass_writes_nothing(self):
        link, node = ToyLink(), None
        node = ToyNode(link)
        try:
            code, out, report = run(base(node, link))
            self.assertEqual(code, 0, report)
            self.assertIn("OK       panel: settings export is repeatable", report)
            self.assertIn("1 backup networks saved", report)
            self.assertIn("OK       link: Hello, Info and Status", report)
            self.assertIn("active tests | not requested", report)
            self.assertEqual(node.writes, ["/api/v1/login"])           # only the session was opened
            self.assertNotIn(st.T_ARM, link.seen)                      # the link was never armed
            self.assertNotIn(st.T_SPINDLE, link.seen)
        finally:
            node.close(); link.close()

    def test_secrets_are_never_printed(self):
        link, node = ToyLink(), None
        node = ToyNode(link)
        try:
            code, out, report = run(base(node, link))
            for secret in (TOKEN, PASSWORD, WIFI_SECRET):
                self.assertNotIn(secret, out)
                self.assertNotIn(secret, report)
        finally:
            node.close(); link.close()

    def test_node_that_does_not_answer(self):
        s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()   # a port nobody listens on
        code, out, report = run(["--host", "127.0.0.1", "--port", str(port), "--token", TOKEN, "--link-port", str(port), "--timeout", "0.5"])
        self.assertEqual(code, 1)
        self.assertIn("FALLO    panel: session (no login)", report)
        self.assertIn("FALLO    link: Hello, Info and Status", report)

    def test_error_and_broken_json(self):
        for mode, word in (("error", "HTTP 500"), ("broken", "valid JSON")):
            node = ToyNode(None, mode)
            try:
                code, out, report = run(["--host", "127.0.0.1", "--port", str(node.port), "--user", "admin", "--password", PASSWORD, "--timeout", "1"])
                # the session route answers, the login works, everything after it fails with the reason
                self.assertEqual(code, 1, mode)
                self.assertIn(word, report)
            finally:
                node.close()

    def test_wrong_password(self):
        node = ToyNode(None)
        try:
            code, out, report = run(["--host", "127.0.0.1", "--port", str(node.port), "--user", "admin", "--password", "nope-nope-nope", "--timeout", "1"])
            self.assertEqual(code, 1)
            self.assertIn("FALLO    panel: login | login refused (HTTP 401)", report)
        finally:
            node.close()

    def test_write_without_active_is_refused(self):
        node = ToyNode(None)
        try:
            h = st.Http("127.0.0.1", node.port)
            with self.assertRaises(RuntimeError):
                h.request("POST", "/api/v1/machine/stop", {})
            self.assertEqual(node.writes, [])
        finally:
            node.close()

    def test_denied_confirmation_runs_nothing_active(self):
        link = ToyLink(); node = ToyNode(link)
        try:
            code, out, report = run(base(node, link, ["--active"]), ask=lambda _: "no")
            self.assertEqual(code, 0, report)
            self.assertIn("confirmation not given", report)
            self.assertNotIn(st.T_ARM, link.seen)
            self.assertNotIn(st.T_SPINDLE, link.seen)
            self.assertNotIn("/api/v1/machine/stop", node.writes)
        finally:
            node.close(); link.close()

    def test_no_stdin_is_a_no(self):
        def eof(_):
            raise EOFError
        self.assertFalse(st.confirm("x", False, eof))
        self.assertTrue(st.confirm("x", True, eof))

    def test_active_pass_times_the_stops_and_ends_safe(self):
        link = ToyLink(); node = ToyNode(link)
        try:
            code, out, report = run(base(node, link, ["--active", "--rpm", "1000", "--seconds", "0.3"]))
            self.assertEqual(code, 0, report)
            self.assertIn("OK       active: spindle by the queue", report)
            self.assertIn("peak=1000 rpm", report)
            self.assertIn("Abort to stopped and disarmed", report)
            self.assertIn("stop button to stopped", report)
            self.assertIn("OK       active: safe at the end of the run", report)
            self.assertEqual((link.rpm, link.armed, link.fill), (0, 0, 0))
        finally:
            node.close(); link.close()

    def test_active_failure_still_sends_the_stop_and_reports_not_safe(self):
        link = ToyLink(deaf=True); node = ToyNode(link)
        try:
            code, out, report = run(base(node, link, ["--active", "--rpm", "1000", "--seconds", "0.2"]))
            self.assertEqual(code, 1)
            self.assertIn("FALLO    active: Abort stop time", report)
            self.assertIn("NOT SAFE", report)
            self.assertIn(st.T_ABORT, link.seen)       # the stop was asked for after the failure
            self.assertIn(st.T_DISARM, link.seen)
        finally:
            node.close(); link.close()

    def test_bad_token_on_the_link(self):
        link = ToyLink(); node = ToyNode(link)
        try:
            code, out, report = run(base(node, link) + ["--token", "wrong-token-123"])
            self.assertEqual(code, 1)
            self.assertIn("bad token", report)
        finally:
            node.close(); link.close()


if __name__ == "__main__":
    unittest.main()
