# EH3D-NODE-BUNGARD: requirements

This register is the single source of the state of each capability: `STATUS.md` and the README files cite it and never state a state that it does not. A state is one of `planned`, `prototype`, `implemented` and `validated`. `validated` means that a person has run the capability on a real machine and the evidence of the manifest says `bench` or `field`; nothing here is validated, because the firmware has not been flashed or run on a board, nothing has been connected to a machine, and the carrier board is a design on paper.

The register has 40 requirements: 22 implemented, 11 prototype, 7 planned and 0 validated. By progress: 22 done, 11 partial, 2 pending and 5 blocked. High coverage is not proof of correctness, so each row says how it is checked: the test that names it, and what it shows.

## How to read the register

* **ID:** `NBGD-NNN`, never reused and never renumbered; a new capability takes the next number.
* **State:** `implemented` is code that is complete and tested on a computer, `prototype` is part of the capability, `planned` is not written.
* **Progress:** `done`, `partial`, `pending` (can be written without hardware) or `blocked` (waits for hardware or for another program of the suite).
* **Evidence:** `simulator` when the test runs against a model of the machine and of the line in memory, `host-test` when it checks a calculation, a reader or a screen-less piece, `none` when nothing is written yet.
* **Test:** the acceptance criterion: `file::name` is the test function `name` of `tests/file.cpp` (it must be called from its `main`); a name with a path and `.py`, such as `tools/file.py::name`, is a test of a Python file. The test must exist. For a capability that is not written the cell says `planned:` and the name of the test that will show it; the day that test exists, the row must change.
* **Limits:** what is not done or not proven; a row that is `partial` or `blocked` always says it.

The check is `tools/test_requirements.py`: it reads this table, finds each test and fails when a state, a test or a count does not agree.

## The control link

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBGD-001 | A frame goes through the encoder and the decoder at every size, and the encoder refuses what does not fit. | prototype | partial | host-test | `test_more::the_encoder_refuses_what_does_not_fit` | Tried against the host model of the line; never on a real RS-232 line. |
| NBGD-002 | Noise before and between frames is skipped. | prototype | partial | host-test | `test_more::noise_before_and_between_frames_is_skipped` | Host model only. |
| NBGD-003 | A damaged frame is counted and the next good one is found. | prototype | partial | host-test | `test_more::a_damaged_frame_is_counted_and_the_next_good_one_is_found` | Host model only. |
| NBGD-004 | A length above the maximum is refused at the header. | implemented | done | host-test | `test_more::a_length_above_the_maximum_is_refused_at_the_header` | - |
| NBGD-005 | The frames follow the shared test vectors, the same file the client of the server is tested with. | implemented | done | host-test | `tools/test_link_contract.py::test_the_two_sides_test_the_same_vectors` | - |
| NBGD-006 | The types that the document names are those of the node. | implemented | done | host-test | `tools/test_link_contract.py::test_the_documented_types_are_the_ones_of_the_node` | - |
| NBGD-007 | The settings that keep the machine safe start closed. | implemented | done | host-test | `tools/test_link_contract.py::test_the_settings_that_keep_the_machine_safe_start_closed` | - |

## Session, queue and stop

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBGD-008 | A program logs in with a token, arms the machine and the session rules hold. | prototype | partial | simulator | `core_tests::session_tests` | Against a model of the machine; never against a control unit. |
| NBGD-009 | A push to the queue is taken whole or not at all, and the queue keeps its order across the end of its buffer. | implemented | done | host-test | `test_more::queue_takes_a_push_whole_or_not_at_all` | - |
| NBGD-010 | The send loop goes on after partial sends and gives up at the limit. | prototype | partial | simulator | `core_tests::send_loop_tests` | Against a model of the line; never on a real line. |
| NBGD-011 | The spindle rules hold: a speed cap, no reversing and a delay. | prototype | partial | simulator | `core_tests::spindle_tests` | Against a model of the spindle; the 0-10 V signal has never been measured. |
| NBGD-012 | A queued spindle command is subject to the same rules. | prototype | partial | simulator | `core_tests::queued_spindle_tests` | Against a model of the spindle. |
| NBGD-013 | Every way the machine is stopped switches the spindle off whatever the session believes. | prototype | partial | simulator | `core_tests::safety_invariants` | Against a model of the spindle; never on a real machine. |
| NBGD-014 | A link that is cut or silent stops the machine. | prototype | partial | simulator | `core_tests::link_cut_tests` | Against a model of the line and the machine; never on a real machine. |
| NBGD-015 | The drain waits for the machine and stops it once. | prototype | partial | simulator | `test_more::drain_waits_for_the_machine_and_stops_it_once` | Against a model of the machine. |
| NBGD-016 | The home stop is handled. | prototype | partial | simulator | `core_tests::home_tests` | Against a model of the lamps; the lamps have never been read on a board. |
| NBGD-017 | The self-check can be read by the person and the node's own check is read. | implemented | done | host-test | `tools/test_selftest.py::test_the_node_own_check_is_read` | - |
| NBGD-018 | A write without the active mode is refused, and a denied confirmation runs nothing. | implemented | done | host-test | `tools/test_selftest.py::test_write_without_active_is_refused` | - |
| NBGD-019 | An active check that fails still sends the stop and reports not safe. | implemented | done | host-test | `tools/test_selftest.py::test_active_failure_still_sends_the_stop_and_reports_not_safe` | Against a mock node. |

## Settings and the shared base

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBGD-020 | The token never leaves the node. | implemented | done | host-test | `test_machine::test_token_never_leaves` | - |
| NBGD-021 | The settings are read within their ranges and pin conflicts are refused. | implemented | done | host-test | `test_machine::test_ranges` | - |
| NBGD-022 | Reserved pins are refused and a node with no spindle is valid. | implemented | done | host-test | `test_machine::test_pins` | - |
| NBGD-023 | The live settings change what they say and nothing else. | implemented | done | host-test | `test_machine::test_live_settings` | - |
| NBGD-024 | Stored settings survive an update. | implemented | done | host-test | `test_base::test_stored_settings_survive_an_update` | - |
| NBGD-025 | A login is throttled and its blocks are kept. | implemented | done | host-test | `test_shared::test_login_throttle` | - |
| NBGD-026 | Sessions are issued and expire as the rules say, and a remembered one is not taken after half an hour. | implemented | done | host-test | `test_base::test_remembered_session_is_not_taken_after_half_an_hour` | - |
| NBGD-027 | The credentials follow the rules and a user store keeps its users after a restart. | implemented | done | host-test | `test_shared::test_user_store_persistence` | - |
| NBGD-028 | The node guard restarts a hung task, keeps a ledger of the restarts and survives a restart. | implemented | done | host-test | `test_base::test_node_guard_survives_a_restart` | A model of the watchdog; never on the chip. |
| NBGD-029 | The web policy keeps a cookie secure over TLS and never readable by scripts. | implemented | done | host-test | `test_more::the_cookie_is_secure_over_tls_and_never_readable_by_scripts` | - |
| NBGD-030 | The network plan accepts four-number addresses without extra zeros and refuses a mask that is not a run of ones. | implemented | done | host-test | `test_more::masks_are_runs_of_ones_with_room_for_two_hosts` | - |
| NBGD-031 | The JSON reader refuses bad text and the limits of a document are kept. | implemented | done | host-test | `test_shared::test_json_limits` | - |

## Update

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBGD-032 | Only the release files of this repository are downloaded. | implemented | done | host-test | `test_more::only_the_release_files_of_this_repository_are_downloaded` | Against a model of the answers of GitHub. |
| NBGD-033 | The image picked is the one with a version for this board. | implemented | done | host-test | `test_release::picking_the_image` | - |

## Not written yet

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBGD-034 | The firmware is flashed on a board and the link answers over TCP. | planned | blocked | none | `planned:on_board::the_link_answers_on_a_flashed_board` | Needs a board. |
| NBGD-035 | A move is sent to a real control unit through the RS-232 line and the machine moves. | planned | blocked | none | `planned:real_machine::a_move_reaches_a_real_control_unit` | Needs the carrier board, a machine and a person ready to stop it. |
| NBGD-036 | The lamps of the machine are read on a board. | planned | blocked | none | `planned:real_lamps::the_lamps_are_read_on_a_board` | Needs the carrier board. |
| NBGD-037 | The 0-10 V signal and the run and direction lines drive a real spindle. | planned | blocked | none | `planned:real_spindle::the_signal_drives_a_real_spindle` | Needs the carrier board and a spindle. |
| NBGD-038 | The stop on a silent link is shown on a real machine. | planned | blocked | none | `planned:real_stop::a_silent_link_stops_a_real_machine` | Needs a machine and a person ready to stop it. |
| NBGD-039 | The update is signed and the node verifies the signature. | planned | pending | none | `planned:signed_update::the_node_refuses_an_update_with_a_wrong_signature` | Not written; the signing key is missing. |
| NBGD-040 | The node reads an emergency-stop input. | planned | pending | none | `planned:estop_input::the_emergency_input_stops_the_machine` | Not written; the carrier board is a design on paper. |
