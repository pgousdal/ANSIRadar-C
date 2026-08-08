# Architecture

ANSIRadar-C is organized as a small C99 pipeline:

- `csrc/core/main.c` parses command-line configuration and selects a provider.
- `csrc/core/door32.c` strictly parses Mystic's bounded 11-line dropfile.
- `csrc/core/transport.c` provides local terminal and duplicated connected-socket transports; the UI never reads stdin directly.
- `csrc/providers/providers.c` owns local readsb/dump1090 JSON and CSV replay input. Providers poll into an `AircraftList` and report errors without owning rendering policy.
- `csrc/models/aircraft.c` normalizes aircraft fields and manages bounded lists.
- `csrc/renderer/radar.c` calculates distance and bearing, projects aircraft, orders rows, resolves collisions, and draws the radar/table into a screen buffer.
- `csrc/renderer/screen.c` owns the fixed 80x25 cell buffer.
- `csrc/renderer/ansi.c` converts buffer changes into ANSI escape sequences.
- `csrc/renderer/charset.c` supplies CP437 and ASCII display profiles.
- `csrc/ui/radar_view.c` runs refresh pacing, stale-source state, keyboard input, time-left handling, and overlays through the shared transport.

The provider/parser boundary is intentionally local-file based: input parsing produces normalized aircraft records, while the renderer operates only on those records and `RadarState`.

## Refresh Model

Interactive mode polls when the configured refresh interval expires. A successful poll replaces the current snapshot and marks the source `OK`; a failed later poll keeps the last good snapshot and marks it `STALE`. `--once` performs exactly one poll and one deterministic frame.
