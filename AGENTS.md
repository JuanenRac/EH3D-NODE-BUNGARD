# AGENTS.md: rules for anyone who changes EH3D-NODE-BUNGARD

EH3D-NODE-BUNGARD: the firmware that turns the RS-232 port of a Bungard CCD control unit into a network port. Software licence: GPL-3.0-or-later; every source file starts with its header.

## 1. Before you start

1. Read `docs/STATUS.md`: what exists, what is missing, what the last change decided.
2. Read `docs/SCOPE.md` (what the program is for and what it never decides) and `docs/REQUIREMENTS.md` (the register of requirements and the test that shows each one).
3. Read `docs/NOT_YET.md` before adding an idea: it may be there on purpose.

## 2. Build and test

- The host tests need a C++17 compiler and CMake: `cmake -S tests -B build/host`, `cmake --build build/host`, then `ctest --test-dir build/host`. The firmware builds with ESP-IDF 5.5 in the CI (`firmware-build.yml`), not on a machine that is short of memory.
- Run before every commit, with the same flags as the CI: the host tests with `-Wall -Wextra -Wpedantic -Werror`, `python -I -m unittest discover -s tools -p "test_*.py"`, `python -I tools/ci_validate.py` and `python tools/make_structure.py --check --readmes`.
- A heavy build runs in the CI, not on a machine that is short of memory; a change that adds a file adds it to `docs/structure.json` and regenerates the tree and the picture.

## 3. Code rules

- A frame that cannot be understood is counted and skipped; the decoder never overruns and never guesses.
- Every way the machine is stopped tells the hardware to switch the spindle off whatever the session believes.
- The settings that keep the machine safe start closed, and a rule of safety only ever refuses or warns: a change that would relax one after a trouble is a defect.
- No task keeps a big copy of the settings on its small stack, and every task has the stack its job needs (`tests/check_task_safety.py`).
- Every text of the panel exists in the eight languages and every error the firmware can answer with has a text (`tests/check_panel_text.py`).
- Every value that is tuned (a limit, a tolerance, a time) is a setting in a configuration file, not a constant in the code; every option of security has a switch with the conservative value as its default.
- Output carries no date, host name or user name that the person did not give.

## 4. What the program must not do

- It does not decide that a move is safe, and it does not replace the emergency stop or the limit switches of the machine.
- It does not keep a token in a log or send it back out: the settings give it to nobody once it is set.
- Its documents and its code do not name anything private: no dates, no internal notes, no names of people or of tools that wrote the change.

## 5. Evidence

- A state in `docs/REQUIREMENTS.md` is `validated` only when a person has run the capability on the real machine it names. Nothing is green that has never run on the real thing.
- A requirement names a test that exists; when a requirement changes state, the row and `docs/STATUS.md` change in the same commit, and `tools/test_requirements.py` says whether they agree.

## 6. Documents and languages

- The README exists in eight languages (English, Spanish, German, French, Italian, Japanese, Chinese and Portuguese of Portugal) and every word of the window and of the messages exists in the eight where the program has them.
- The file tree and the pictures are generated from `docs/structure.json` and `docs/architecture.json`; they are not edited by hand. The count of tests in the READMEs is the real one.
- Each change has a line in the `[Unreleased]` part of `CHANGELOG.md`, in English.

## 7. Clean room

- Ideas, flows and behaviours that can be seen from outside may be taken from other programs; their code, their texts, their files and their wording are not copied.
- Formats are learned from public specifications and from files of this project. The files of another program are read, never written.

## 8. Reporting

- A problem with a part of the program is reported with a small file that shows it. Real boards and real machine files are private: do not paste them into a report.
