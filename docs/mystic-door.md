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

## Testing

Test the installed executable first with `--version` and `--help`, then use
SyncTERM against a test node. Verify `q`, closing the client, and time expiry.
Run two node sessions at once and confirm each has independent selection,
pause, and range state. The offline native socketpair tests cover the transport
and runtime without Mystic, SyncTERM, Telnet, or a readsb daemon.

Optional diagnostics go to a file only with `--debug-log`; use a separate file
for each node. Troubleshooting output is never sent to the caller socket.
