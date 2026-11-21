# =============================================================================
# EH3D-NODE-BUNGARD - tools/test_requirements.py
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
# GPL-3.0-or-later - see LICENSE
# =============================================================================
"""The documents that say what the program is, what it must do and where it stands, and the register of requirements: every requirement has an identifier, a state that agrees with its progress and its evidence, and names a test that exists (or, when it is not written yet, the test that will show it). The register is the one source of the state of each capability, so STATUS.md and the README files must agree with it, and the documents are public: they carry no date and no reference to anything private."""
from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PREFIX = "NBGD"
DEFAULT_DIR = "tests"
EXT = ".cpp"
TEST_PATTERN = r"^\s*static void %s\(\)"
CALL_PATTERN = r"^\s*%s\(\);"
PY_PATTERN = r"^\s*(?:async\s+)?def %s\("
MIN_ROWS = 30
MIN_IDEAS = 6
DOCUMENTS = ("docs/SCOPE.md", "docs/REQUIREMENTS.md", "docs/STATUS.md", "docs/NOT_YET.md", "AGENTS.md")
READMES = ("README.md", "README_spa.md", "README_deu.md", "README_fra.md", "README_ita.md", "README_jpn.md", "README_zho.md", "README_por.md")
NEVER_WORDS = ('emergency stop', 'limits')


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8").replace("\r\n", "\n")


def rows() -> list[dict[str, str]]:
    found = []
    for line in read("docs/REQUIREMENTS.md").split("\n"):
        if not line.startswith("| " + PREFIX + "-"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        assert len(cells) == 7, "a row has seven cells: " + line
        found.append({"id": cells[0], "state": cells[2], "progress": cells[3], "evidence": cells[4], "test": cells[5].strip("`"), "limits": cells[6]})
    return found


def has_test(stem: str, name: str) -> bool:
    """Whether the file that `stem` names has a test called `name`: DEFAULT_DIR/<stem>, or the path from the root when the stem has a slash."""
    if stem.endswith(".py"):
        path, pattern, call = ROOT / stem, PY_PATTERN, ""
    else:
        path = ROOT / (stem + EXT) if "/" in stem else ROOT / DEFAULT_DIR / (stem + EXT)
        pattern, call = TEST_PATTERN, CALL_PATTERN
    if not path.is_file():
        return False
    text = path.read_text(encoding="utf-8")
    if re.search(pattern % re.escape(name), text, re.M) is None:
        return False
    return not call or re.search(call % re.escape(name), text, re.M) is not None


class RequirementsTest(unittest.TestCase):
    def test_the_five_documents_exist_and_say_something(self) -> None:
        for d in DOCUMENTS:
            text = read(d)
            self.assertGreater(len(text.strip()), 400, d + " is nearly empty")
            self.assertTrue(text.startswith("#"), d + " does not start with a title")
        self.assertIn("\nStatus:", read("docs/SCOPE.md"))

    def test_every_requirement_has_an_identifier_that_is_used_once_and_in_order(self) -> None:
        found = rows()
        self.assertGreaterEqual(len(found), MIN_ROWS)
        for i, r in enumerate(found):
            self.assertEqual(r["id"], "%s-%03d" % (PREFIX, i + 1), "the identifiers run on without a gap")

    def test_every_state_agrees_with_its_progress_and_its_evidence(self) -> None:
        want = {"done": "implemented", "partial": "prototype", "pending": "planned", "blocked": "planned"}
        for r in rows():
            self.assertEqual(r["state"], want[r["progress"]], r["id"])
            if r["evidence"] in ("simulator", "host-test"):
                self.assertNotEqual(r["state"], "planned", r["id"])
            else:
                self.assertEqual(r["evidence"], "none", r["id"])
                self.assertEqual(r["state"], "planned", r["id"])
            # nothing has run on a real machine, so nothing is validated
            self.assertNotEqual(r["state"], "validated", r["id"])
            if r["progress"] in ("partial", "blocked"):
                self.assertTrue(len(r["limits"]) > 3 and r["limits"] != "-", r["id"] + " says no limit")

    def test_every_requirement_names_a_test_that_exists(self) -> None:
        for r in rows():
            if r["state"] == "planned":
                # not written yet: the cell names the test that will show it, and that test must not exist yet
                self.assertTrue(r["test"].startswith("planned:"), r["id"])
                spec = r["test"][len("planned:"):]
                stem, name = spec.split("::")
                self.assertTrue(name and re.fullmatch(r"\w+", name), r["id"])
                self.assertFalse(has_test(stem, name), r["id"] + ": the test " + spec + " exists, so the row must say more than planned")
            else:
                stem, name = r["test"].split("::")
                self.assertTrue(has_test(stem, name), r["id"] + ": " + stem + " has no test " + name)

    def test_the_register_and_the_status_page_say_the_same_numbers(self) -> None:
        found = rows()
        n = lambda key, value: sum(1 for r in found if r[key] == value)  # noqa: E731
        sentence = "The register has %d requirements: %d implemented, %d prototype, %d planned and 0 validated." % (len(found), n("state", "implemented"), n("state", "prototype"), n("state", "planned"))
        by_progress = "By progress: %d done, %d partial, %d pending and %d blocked." % (n("progress", "done"), n("progress", "partial"), n("progress", "pending"), n("progress", "blocked"))
        for doc in ("docs/REQUIREMENTS.md", "docs/STATUS.md"):
            text = read(doc)
            self.assertIn(sentence, text, doc)
            self.assertIn(by_progress, text, doc)

    def test_the_readmes_link_the_documents(self) -> None:
        for name in READMES:
            text = read(name)
            for d in DOCUMENTS:
                self.assertIn(d, text, name + " does not link " + d)

    def test_the_documents_are_public_without_a_date_or_a_private_reference(self) -> None:
        # the words are built here so that this file does not hold them itself
        forbidden = ["son" + "net", "aud" + "it", "cla" + "ude", "anthro" + "pic", "C:" + "\\Users", "Docu" + "ments\\GitHub", "Docu" + "ments/GitHub"]
        months = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]
        for d in DOCUMENTS:
            text = read(d)
            lower = text.lower()
            for word in forbidden:
                self.assertNotIn(word.lower(), lower, d + " holds " + word)
            self.assertIsNone(re.search(r"20\d\d-\d\d-\d\d", text), d + " holds a date")
            for m in months:
                self.assertIsNone(re.search(m + r" 20[2-3]\d", text), d + " holds a date: " + m)

    def test_the_not_yet_page_says_why_and_when_for_every_idea(self) -> None:
        lines = [l for l in read("docs/NOT_YET.md").split("\n") if l.startswith("| ") and not l.startswith("| Idea") and not l.startswith("|---")]
        self.assertGreaterEqual(len(lines), MIN_IDEAS)
        for l in lines:
            cells = [c.strip() for c in l.strip().strip("|").split("|")]
            self.assertEqual(len(cells), 3, l)
            self.assertTrue(all(len(c) > 3 for c in cells), l)

    def test_the_scope_says_what_the_program_never_decides(self) -> None:
        text = read("docs/SCOPE.md").lower()
        for want in ("## purpose", "## what it does", "## what it will not do", "## where it sits", "## what it never decides"):
            self.assertIn(want, text, "SCOPE.md has no section " + want)
        for word in NEVER_WORDS:
            self.assertIn(word, text)


if __name__ == "__main__":
    unittest.main()
