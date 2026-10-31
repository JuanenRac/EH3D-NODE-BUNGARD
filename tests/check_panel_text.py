#!/usr/bin/env python3
"""Check the panel's texts: every row has the eight languages, every key the panel uses has a row, no row is unused, nothing names another project, and the packed panel fits.
Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.

Usage:  python tests/check_panel_text.py

The panel shows `e_<code>` for an error the node answers with and `p_<code>` next to a field the node refuses; a code without a row would reach the person as the bare code.
The codes are read from the C++ (`send_error(r, N, "code")`, `error = "code"` and the problem codes of the machine settings), so a new code cannot be added without its eight translations.
"""
import gzip
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PANEL = ROOT / "panel"
LANGUAGES = 8
CODES = ["en", "es", "de", "fr", "it", "ja", "zh", "pt"]
LIMIT_BYTES = 120 * 1024   # the gzip of index.html, app.js (with text.js) and style.css (the summary, the Wi-Fi, the update from GitHub and the help made it grow past 90 KB)
FORBIDDEN = re.compile(r"armor|a\.r\.m\.o\.r|mqtt|zigbee|broker", re.I)
# the codes the machine settings answer with next to a field, and the ones the panel itself raises
MACHINE_PROBLEMS = ["out_of_range", "duplicate_pin", "reserved_pin", "not_a_number", "too_long", "required"]


def rows(text):
    found = {}
    for match in re.finditer(r"^  ([A-Za-z0-9_]+): \[(.*)\],?$", text, re.M):
        found[match.group(1)] = re.findall(r'"((?:[^"\\]|\\.)*)"', match.group(2))
    return found


def balanced_calls(source, name):
    """The text between the parentheses of every call of `name(` (a bare function, not a method)."""
    calls = []
    for match in re.finditer(r"(?<![\w.$])" + re.escape(name) + r"\(", source):
        depth, i, in_string = 1, match.end(), ""
        while i < len(source) and depth:
            c = source[i]
            if in_string:
                if c == "\\":
                    i += 1
                elif c == in_string:
                    in_string = ""
            elif c in "\"'`":
                in_string = c
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            i += 1
        calls.append(source[match.end():i - 1])
    return calls


def first_argument(call):
    """The text of a call up to its first top-level comma (a label may be chosen by a condition: `single ? "a" : "b"`)."""
    depth, in_string = 0, ""
    for i, c in enumerate(call):
        if in_string:
            if c == in_string:
                in_string = ""
        elif c in "\"'`":
            in_string = c
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and depth == 0:
            return call[:i]
    return call


def used_keys(app):
    """Every text key the panel names: inside t( ... ), the first argument of the field helpers, and the tables of pages, lamps and lines."""
    keys = set()
    for call in balanced_calls(app, "t"):
        keys |= set(re.findall(r'"([A-Za-z][A-Za-z0-9_]*)"', call))
    for helper in ("field", "pinField", "passwordField", "labelled"):
        for call in balanced_calls(app, helper):
            keys |= set(re.findall(r'"([A-Za-z][A-Za-z0-9_]*)"', first_argument(call)))
    keys |= set(re.findall(r'label: "([A-Za-z0-9_]+)"', app))
    keys |= set(re.findall(r'group: "([A-Za-z0-9_]+)"', app))
    keys |= set(re.findall(r'\["((?:lamp|line)[A-Za-z]+)", \d+', app))
    return keys


def check_rows(table, problems):
    for key, strings in table.items():
        if len(strings) != LANGUAGES:
            problems.append(f"{key}: {len(strings)} texts instead of {LANGUAGES}")
            continue
        for code, text in zip(CODES, strings):
            if not text.strip():
                problems.append(f"{key}: an empty text in {code}")
            if FORBIDDEN.search(text):
                problems.append(f"{key}: the text in {code} names something this node does not have ({FORBIDDEN.search(text).group(0)})")
        marks = [sorted(re.findall(r"\{\d+\}", text)) for text in strings]
        if any(m != marks[0] for m in marks):
            problems.append(f"{key}: the placeholders differ between the languages")


def check_languages(text, problems):
    listed = re.findall(r'\["([a-z]{2})", "[^"]+"\]', text.split("const L = {")[0])
    if listed != CODES:
        problems.append(f"LANGS lists {listed}, expected {CODES}")


def check_firmware_codes(table, problems):
    """Every error code the firmware can answer with has its e_ row (when the firmware sources are there)."""
    codes = set()
    for name in ("web_server.cpp", "api_shared.cpp", "machine_api.cpp", "github_update.cpp"):
        source = ROOT / "main" / name
        if source.is_file():
            text = source.read_text(encoding="utf-8", errors="replace")
            codes |= set(re.findall(r'send_error\(r, \d+, "([a-z_0-9]+)"', text))
            codes |= set(re.findall(r'error = "([a-z_0-9]+)"', text))
            codes |= set(re.findall(r'finish_copy\("([a-z_0-9]+)"', text))
    codes = {c for c in codes if not c.endswith("_")}   # "http_" + status: chosen at run time
    for code in sorted(codes):
        if "e_" + code not in table:
            problems.append(f"the firmware answers \"{code}\" and the panel has no text e_{code}")
    # what the Wi-Fi station reports as its last error is told by the se_ rows
    network = ROOT / "main" / "network.cpp"
    stations = set()
    if network.is_file():
        stations = set(re.findall(r'error = "([a-z_0-9]+)"', network.read_text(encoding="utf-8", errors="replace")))
    for code in sorted(stations):
        if "se_" + code not in table and "e_" + code not in table:
            problems.append(f"the Wi-Fi code \"{code}\" has no text (se_{code} for the station, e_{code} for a search)")
    return len(codes) + len(stations)


def main():
    problems = []
    text = (PANEL / "text.js").read_text(encoding="utf-8")
    app = (PANEL / "app.js").read_text(encoding="utf-8")
    table = rows(text)
    check_languages(text, problems)
    check_rows(table, problems)

    for name in ("index.html", "app.js", "style.css", "text.js"):
        body = (PANEL / name).read_text(encoding="utf-8")
        for match in re.finditer(r"armor|a\.r\.m\.o\.r", body, re.I):
            problems.append(f"panel/{name} mentions {match.group(0)}")
            break

    used = used_keys(app)
    for key in sorted(used):
        if key.endswith("_"):
            continue   # the start of a key chosen at run time
        if key not in table:
            problems.append(f"app.js uses the text \"{key}\" and text.js has no row for it")
    dynamic = ("e_", "p_", "se_", "reason", "help_", "navHelpTab_")   # chosen at run time from the code the node sends
    for key in sorted(table):
        if key not in used and not key.startswith(dynamic):
            problems.append(f"text.js has the row \"{key}\" and nothing uses it")
    for code in MACHINE_PROBLEMS:
        if "p_" + code not in table:
            problems.append(f"the machine settings can answer \"{code}\" and the panel has no text p_{code}")
    for code in ("too_many_attempts", "wrong_credentials", "unauthorized", "invalid", "other"):
        if "e_" + code not in table:
            problems.append(f"e_{code} is missing")
    codes = check_firmware_codes(table, problems)

    packed = sum(len(gzip.compress(data, 9, mtime=0)) for data in (
        (PANEL / "index.html").read_bytes(),
        (PANEL / "text.js").read_bytes() + b"\n" + (PANEL / "app.js").read_bytes(),
        (PANEL / "style.css").read_bytes()))
    if packed > LIMIT_BYTES:
        problems.append(f"the packed panel is {packed} bytes, over the {LIMIT_BYTES} limit")

    if problems:
        print("PANEL_TEXT=FAIL")
        for problem in problems:
            print("  " + problem)
        return 1
    print(f"PANEL_TEXT=PASS {len(table)} rows in {LANGUAGES} languages, {len(used)} keys used, {codes} firmware error codes with a text, {packed} bytes packed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
