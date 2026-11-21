# EH3D-NODE-BUNGARD: not yet

Good ideas that are not in the current version. Writing them here keeps anybody from implementing them early or twice.

| Idea | Why not yet | Revisit when |
|---|---|---|
| A signed update | A signing key is still missing, and the update from GitHub checks only the SHA-256 that the release publishes | the key exists and the node verifies it |
| An emergency-stop input | The carrier board is a design on paper and the stop line is wired through the control unit of the machine | the carrier board is built and the owner decides the wiring |
| A view of the limit switches | The node sees them only through the lamps of the control unit | the control unit gives them on a line the node can read |
| A second machine on the same node | The node drives one RS-232 line and one spindle | a machine needs it and a board has a second port |
| Encrypted control link | The link is on a trusted network with a token; encryption adds a certificate to look after | the node is reached from outside the workshop |
| Over-the-air update of the panel apart from the firmware | The panel is packed into the image and updated with it | the panel changes more often than the firmware |
