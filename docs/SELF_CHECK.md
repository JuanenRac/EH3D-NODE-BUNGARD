# The self-check of the node

The node judges its own state and says it in two keys of `GET /api/v1/machine/status`: `selfcheck` (`ok`, `warn`, `fail`, or `off` when the check is switched off) and `selfcheck_codes` (the codes behind the verdict, separated by commas, empty when there are none). The judgement is a pure function of a snapshot of the node and of its settings (`core/self_check.hpp`): it touches no hardware, so it is tested on a computer, and a host test (`tests/test_self_check.cpp`) reads this page and fails when a code is in the page and not in the firmware, or the other way round.

The verdict is `fail` when any code of severity `fail` is raised, `warn` when the worst is `warn`, `ok` when nothing above `info` is raised. A code is never reused for another meaning.

None of this has run on a board: the codes are checked against a stand-in for the hardware, and a control session driven by the tests.

| Code | Severity | Raised when | Setting to look at |
|---|---|---|---|
| `BND-N01` | warn | the network is not up (after the start-up grace time) | |
| `BND-S01` | warn | the node has no token and open control is off: it refuses to be controlled | `machine.link.token` |
| `BND-S02` | warn | open control is on: anybody on the network can drive the machine | `machine.link.allow_open` |
| `BND-S03` | info | the node finishes the job it holds when the program goes away | `machine.link.finish_on_loss` |
| `BND-S04` | warn | an armed session may stay silent for longer than the limit before the node stops | `machine.link.idle_ms` |
| `BND-Q01` | warn | the queue for the control unit is almost full | `machine.selfcheck.queue_percent` |
| `BND-P01` | fail | the spindle turns faster than its top speed | `machine.spindle.max_rpm` |
| `BND-P02` | info | this node has no spindle output | `machine.spindle.max_rpm` |
| `BND-D01` | info | the node is finishing the job it holds | |

## The limits

They are settings of the `machine.selfcheck` section, never numbers in the code. A change is taken at the next status request, with no restart. The panel does not show this section yet: it is read and written through the settings document (`GET` and `PUT /api/v1/machine`, and the backup of the settings).

| Setting | Factory value | Range | Meaning |
|---|---|---|---|
| `enabled` | `true` | | switches the check on or off |
| `queue_percent` | 90 | 50 to 100 | the queue this full (in whole percents) raises `BND-Q01` |
| `idle_ms_max` | 10000 | 200 to 60000 | a link idle time above this raises `BND-S04` |
| `grace_s` | 30 | 0 to 600 | after the start the network gets this long before `BND-N01` |
