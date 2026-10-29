# Security policy

EH3D-NODE-BUNGARD is firmware for a node that is reachable over the network and, once it is wired to a machine, can make that machine move and a spindle turn. It takes input from the network in two places: the control link (TCP) and the web panel (HTTP and HTTPS).

What the firmware does about it, and what is tested on a computer:

* The control link refuses everything but a login with the access token, takes bytes for the machine only while a program has armed it, and stops (queue emptied, drivers and spindle off) when the session goes quiet or the connection drops.
* The panel needs a user; the first administrator is made with a set-up code that only the USB console shows; a wrong password is slowed down and a source address that keeps failing is blocked for a while. Nothing secret is compiled into the image.
* The panel never moves the machine, and it refuses to save settings while a program holds the machine armed.
* An update goes into the slot that is not running and is confirmed only when the node can be reached; otherwise the boot loader goes back.

What it does **not** do: it has no emergency-stop input yet, it does not see the machine's limit switches except through the lamps, its updates are not signed, and it has had no independent review. Nothing has run on a board or on a machine yet. It is not a safety system.

## Reporting a problem

Please report a vulnerability privately to **electrohobby3d@gmail.com**, with the steps that show it. Do not open a public issue for it. You will get an answer as soon as possible; this is a one-person project.
