# EH3D-NODE-BUNGARD: status

## Current state

EH3D-NODE-BUNGARD has its rules written and tested on a computer: the frames of the link, the session and the arming, the stop on every way out, the spindle rules, the queue and the pace of the line, the settings and their ranges, the shared base (users, sessions, the network plan, JSON, the node guard), the update and the panel texts in eight languages. The firmware builds without warnings for both boards. It has not been flashed or run on a board, nothing has been connected to a machine, and the carrier board that brings the RS-232 converter, the lamp inputs and the drive of the spindle together is a design on paper.

## Capabilities

The official state of each capability is in `REQUIREMENTS.md`. The register has 40 requirements: 22 implemented, 11 prototype, 7 planned and 0 validated. By progress: 22 done, 11 partial, 2 pending and 5 blocked. This page never states a state that `REQUIREMENTS.md` does not.

## Open questions

* Which signing key signs the update, and whether the update without a signature stays allowed.
* Whether the node should read an emergency-stop input of its own, and where it would be wired.

## What it needs from other repositories

* A board of each kind (S3 with Wi-Fi and S3 with Ethernet) and the carrier board, for anything that is not tried on a computer.
* A Bungard CCD machine and its control unit, and a person at it, for any test with the machine.
* `EH3D-SERVER` and the desktop programs, which talk to this node.

## Known technical debt and omitted tests

* The firmware has never run on a board: the timing of the line, the lamps and the 0-10 V signal are untried.
* The panel has been exercised in a browser against a mock node, not against a real one.
* The count of checks of the host programs is measured in the CI and by the programs themselves.
