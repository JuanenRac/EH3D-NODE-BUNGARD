# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
"""Contract tests of the control link, run without the board: the frames, the error codes and the defaults of this node against what its documentation says and, when the
program that drives the node (EH3D-DRILLPRO) is next to this repository, against its reading of the same link.

Nothing here has been tried on a node on a machine: it compares texts and checks the shared test vectors with a second implementation of the CRC.
"""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FRAME = ROOT / "core" / "frame.hpp"
VECTORS = ROOT / "tests" / "testdata" / "frames.json"
CONFIG = ROOT / "core" / "node_config.hpp"
PROTOCOL_DOC = ROOT / "docs" / "PROTOCOL.md"
CLIENT = ROOT.parent / "EH3D-DRILLPRO" / "protocol" / "src" / "node.rs"
CLIENT_VECTORS = ROOT.parent / "EH3D-DRILLPRO" / "protocol" / "tests" / "testdata" / "frames.json"
CLIENT_DOC = ROOT.parent / "EH3D-DRILLPRO" / "protocol" / "src" / "connection.rs"
SPINDLES = ROOT.parent / "EH3D-COMMON" / "crates" / "materials" / "src" / "spindle.rs"
NUMBERS = {"one": 1, "two": 2, "three": 3, "four": 4, "five": 5, "six": 6, "ten": 10}


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def snake(camel: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", camel).upper()


def node_enum(name: str) -> dict:
    """The members of `enum class <name>` of core/frame.hpp: {CamelName: value}."""
    text = read(FRAME)
    body = text[text.index(f"enum class {name}"):]
    body = body[:body.index("};")]
    return {m.group(1): int(m.group(2), 0) for m in re.finditer(r"^\s*([A-Za-z]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", body, re.M)}


def default(name: str) -> int:
    """The default of a member of the link settings in core/node_config.hpp (`int idle_ms = 3000;`)."""
    return int(re.search(rf"\bint {name}\s*=\s*(\d+);", read(CONFIG)).group(1))


class FrameTests(unittest.TestCase):
    def test_the_vectors_are_what_the_format_says(self):
        data = json.loads(read(VECTORS))
        self.assertEqual(data["sync"], 0xB5)
        self.assertGreater(len(data["vectors"]), 10)
        for v in data["vectors"]:
            payload = bytes.fromhex(v["payload"])
            body = bytes([v["type"]]) + len(payload).to_bytes(2, "little") + payload
            expected = bytes([0xB5]) + body + crc16(body).to_bytes(2, "little")
            self.assertEqual(bytes.fromhex(v["frame"]), expected, v["name"])

    def test_every_type_of_the_node_has_a_vector_or_is_a_report(self):
        data = json.loads(read(VECTORS))
        seen = {v["type"] for v in data["vectors"]}
        for name, value in node_enum("Type").items():
            if value < 0x80:  # the orders the program sends must all have a vector
                self.assertIn(value, seen, f"no test vector for the order {name}")

    def test_the_documented_types_are_the_ones_of_the_node(self):
        doc = read(PROTOCOL_DOC)
        rows = {int(m.group(1), 16): m.group(2) for m in re.finditer(r"^\| 0x([0-9A-Fa-f]{2}) \| (\w+) \|", doc, re.M)}
        types = {value: name for name, value in node_enum("Type").items()}
        self.assertEqual(set(rows), set(types))
        for value, name in rows.items():
            self.assertEqual(name, types[value])

    def test_the_documented_errors_are_the_ones_of_the_node(self):
        doc = read(PROTOCOL_DOC)
        row = re.search(r"^\| 0x84 \| Error \|.*\|(.*)\|$", doc, re.M).group(1)
        documented = {int(n) for n in re.findall(r"(\d+) [a-z]", row)}
        self.assertEqual(documented, set(node_enum("Error").values()))

    def test_the_node_gives_the_largest_payload_the_document_says(self):
        self.assertIn("The largest payload is 2048 bytes", read(PROTOCOL_DOC))
        self.assertEqual(int(re.search(r"kMaxPayload = (\d+);", read(FRAME)).group(1)), 2048)


class DefaultsTests(unittest.TestCase):
    def test_the_defaults_the_document_names_are_the_ones_of_the_node(self):
        doc = read(PROTOCOL_DOC)
        hello = re.search(r"`machine\.link\.hello_timeout_ms` \((\w+) seconds by default", doc)
        bad = re.search(r"`machine\.link\.max_bad_hellos` wrong Hellos \((\w+) by default", doc)
        idle = re.search(r"`machine\.link\.idle_ms` \((\w+) seconds by default", doc)
        self.assertEqual(NUMBERS[hello.group(1)] * 1000, default("hello_timeout_ms"))
        self.assertEqual(NUMBERS[bad.group(1)], default("max_bad_hellos"))
        self.assertEqual(NUMBERS[idle.group(1)] * 1000, default("idle_ms"))
        self.assertIn("port 5800 by default", doc)
        self.assertEqual(default("port"), 5800)

    def test_the_settings_that_keep_the_machine_safe_start_closed(self):
        text = read(CONFIG)
        self.assertRegex(text, r"bool allow_open = false;")
        self.assertRegex(text, r"bool finish_on_loss = false;")

    def test_what_the_node_accepts_is_narrower_than_what_it_reads(self):
        # the document of the settings reads a wide range and the validation of the whole document narrows it: the narrow ones are the ones a person can rely on
        text = read(CONFIG)
        self.assertIn('"link.idle_ms", "range"', text)
        self.assertRegex(text, r"idle_ms < 200 \|\| m\.link\.idle_ms > 60000")
        self.assertRegex(text, r"port < 1024 \|\| m\.link\.port > 65535")
        self.assertRegex(text, r"queue_bytes < 1024 \|\| m\.link\.queue_bytes > 4194304")


@unittest.skipUnless(CLIENT.is_file(), "EH3D-DRILLPRO is not next to this repository")
class ClientTests(unittest.TestCase):
    """The program that drives the node reads the same link: its constants must be the ones of the node."""

    def test_the_client_has_the_same_frame(self):
        rust = read(CLIENT)
        self.assertEqual(int(re.search(r"pub const SYNC: u8 = (0x[0-9A-Fa-f]+);", rust).group(1), 16), 0xB5)
        self.assertEqual(int(re.search(r"pub const MAX_PAYLOAD: usize = (\d+);", rust).group(1)), 2048)

    def test_the_client_has_the_same_types(self):
        rust = read(CLIENT)
        kinds = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"pub const ([A-Z_]+): u8 = (0x[0-9A-Fa-f]+);", rust[rust.index("pub mod kind"):])}
        theirs = {snake(name): value for name, value in node_enum("Type").items()}
        self.assertEqual(kinds, theirs)

    def test_the_client_explains_every_error_of_the_node(self):
        rust = read(CLIENT)
        text = rust[rust.index("fn error_text"):]
        text = text[:text.index("_ => \"unknown error\"")]
        codes = {int(m.group(1)) for m in re.finditer(r"^\s*(\d+) =>", text, re.M)}
        self.assertEqual(codes, set(node_enum("Error").values()))

    def test_the_two_sides_test_the_same_vectors(self):
        self.assertTrue(CLIENT_VECTORS.is_file())
        self.assertEqual(json.loads(read(VECTORS)), json.loads(read(CLIENT_VECTORS)))

    def test_the_client_names_the_port_of_the_node(self):
        self.assertIn("node_address = 192.168.0.50:5800", read(CLIENT_DOC))
        self.assertEqual(default("port"), 5800)


@unittest.skipUnless(SPINDLES.is_file(), "EH3D-COMMON is not next to this repository")
class SpindleTests(unittest.TestCase):
    """The server judges a job against the spindle the machine carries; for the one driven by this node, its top speed is what `spindle.max_rpm` of the node should be."""

    def test_the_top_speed_of_the_spindle_with_a_drive_is_the_default_of_the_node(self):
        rust = read(SPINDLES)
        top = int(re.search(r"\(v\.max_rpm, v\.power_w, v\.max_z_mm_min\),\s*\(([\d_]+),", rust).group(1).replace("_", ""))
        self.assertEqual(top, int(re.search(r"int max_rpm = (\d+);", read(CONFIG)).group(1)))
        self.assertIn("24000 rpm by default", read(PROTOCOL_DOC))

    def test_the_original_spindle_is_above_what_the_node_gives(self):
        # the original spindle (63000 rpm) is driven by the control unit itself, never by this node: its top speed is above the default of the node
        rust = read(SPINDLES)
        original = int(re.search(r"\(o\.min_rpm, o\.max_rpm, o\.max_bit_mm\), \(([\d_]+), ([\d_]+),", rust).group(2).replace("_", ""))
        self.assertGreater(original, int(re.search(r"int max_rpm = (\d+);", read(CONFIG)).group(1)))


if __name__ == "__main__":
    unittest.main()
