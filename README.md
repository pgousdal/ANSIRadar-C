# ANSIRadar-C

Lightweight C99 ADS-B radar for classic 80x25 ANSI/CP437 BBS terminals.

## Overview

ANSIRadar-C is the standalone C99 edition of ANSIRadar, optimized for classic BBS terminals and a fixed 80x25 screen. It reads local readsb/dump1090-compatible JSON snapshots or CSV replay fixtures, projects aircraft onto an aspect-correct polar radar, and renders a compact ANSI screen with an aircraft table.

The original Python project remains separate:
https://github.com/pgousdal/ANSIRadar

## Features

- C99 implementation with a minimal runtime footprint
- Fixed 80x25 BBS layout
- ANSI rendering with CP437 and ASCII character profiles
- Aspect-correct polar projection
- Deterministic aircraft ordering and collision handling
- Aircraft status and table views
- Configurable source refresh pacing and stale-source indication
- Deterministic `--once` rendering
- Local readsb/dump1090-compatible JSON input
- CSV replay and fixture-oriented test coverage

## Build

Using the Makefile:

```sh
make
```

Using CMake:

```sh
cmake -S . -B build
cmake --build build
```

The resulting executable is `ansiradar80`.

## Test

```sh
make test
```

For a CMake build:

```sh
ctest --test-dir build --output-on-failure
```

## Usage

The C edition accepts local files only; it does not provide URL or network fetching.

Render one readsb JSON snapshot with CP437 and color:

```sh
./ansiradar80 --source readsb --file fixtures80/readsb.json \
  --receiver-lat 58.3405 --receiver-lon 6.2812 --range 100 \
  --width 80 --height 25 --charset cp437 --color always --once
```

Render the same snapshot in ASCII without color:

```sh
./ansiradar80 --file fixtures80/readsb.json \
  --receiver-lat 58.3405 --receiver-lon 6.2812 --range 50 \
  --width 80 --height 25 --charset ascii --color never --once
```

Run a continuously refreshed local source:

```sh
./ansiradar80 --source dump1090 --file /var/lib/readsb/aircraft.json \
  --receiver-lat 58.3405 --receiver-lon 6.2812 --range 100 \
  --width 80 --height 25 --charset cp437 --color always --refresh 2
```

Use `./ansiradar80 --help` for all options. Receiver coordinates and `--file` are required, and dimensions must remain 80x25.

## BBS Use

ANSIRadar-C is intended for classic ANSI/CP437 BBS terminals and native deployments where a small local C program is preferred. Direct Mystic DOOR32 integration is not implemented in this C edition. A terminal or door wrapper can invoke the executable and provide a local snapshot file.

## Relationship to ANSIRadar

ANSIRadar-C is the standalone C99 edition.

The Python edition, including the Mystic DOOR32 runtime and broader source/replay tooling, lives in:
https://github.com/pgousdal/ANSIRadar

## Repository Metadata

Recommended GitHub description:

> Lightweight C99 ADS-B radar for classic 80x25 ANSI/CP437 BBS terminals.

Suggested topics: `ads-b`, `bbs`, `ansi`, `cp437`, `c99`, `retrocomputing`, `terminal`, `radar`, `readsb`, `dump1090`.

## License

See [LICENSE](LICENSE).
