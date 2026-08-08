# Mystic DOOR32 deployment

ANSIRadar-C supports Mystic BBS on Linux through `DOOR32.SYS` communication
type 2, where the communication handle is Mystic's already-connected socket
descriptor. The program duplicates that descriptor, does not change its
blocking flags, and closes only the duplicate.

## Build and install

```sh
make
install -m 0755 ansiradar80 /home/mystic/doors/ansiradar-c/ansiradar80
```

The Mystic service account must be able to execute the binary and read the
local readsb/dump1090 snapshot. Keep one dropfile path per node, as supplied by
Mystic. The program never modifies `DOOR32.SYS`.

## Invocation

```sh
/home/mystic/doors/ansiradar-c/ansiradar80 \
  --door32 /path/to/DOOR32.SYS \
  --file /run/readsb/aircraft.json \
  --receiver-lat 58.3405 --receiver-lon 6.2812 \
  --charset cp437 --color always --width 80 --height 25
```

The wrapper in `examples/mystic/ansiradar80-door.sh` is a starting point for a
Mystic menu entry. Exact Mystic menu macros depend on the installed Mystic
version and are intentionally not prescribed here.

## Descriptor ownership

Communication type 2 passes an already-connected socket descriptor to the
door. The door must be the only process reading that socket while it runs.
`dup()` creates another descriptor for the same socket/open file description; it
does not create a second receive queue. The door leaves Mystic's descriptor
unchanged, reads through its duplicate with per-call `MSG_DONTWAIT`, and closes
only the duplicate on exit.

The example wrapper uses `exec`, so the shell is replaced and cannot remain as
a competing reader. A `poll()` result can still be followed by `EAGAIN` if
another process or thread has read the shared socket between `poll()` and
`recv()`. The native test suite reproduces this with two forked readers. In a
Mystic installation, inspect the exact door invocation and process tree if
debug logs show `recv: EAGAIN after poll`; do not enable `O_NONBLOCK` globally
on the inherited descriptor as a workaround.

## Testing

Test the installed executable first with `--version` and `--help`, then use
SyncTERM against a test node. Verify `q`, closing the client, and time expiry.
Run two node sessions at once and confirm each has independent selection,
pause, and range state. The offline native socketpair tests cover the transport
and runtime without Mystic, SyncTERM, Telnet, or a readsb daemon.

Optional diagnostics go to a file only with `--debug-log`; use a separate file
for each node. Troubleshooting output is never sent to the caller socket.
