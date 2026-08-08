# Mystic example

Install `ansiradar80` at the path used by the example wrapper, make the
wrapper executable, and configure a Mystic door/menu entry to invoke it with
the node's `DOOR32.SYS` path as its first argument. Do not assume a fixed
dropfile path when more than one node is active.

The wrapper assumes readsb writes `/run/readsb/aircraft.json`; adjust that path
and the receiver coordinates for the installation.
