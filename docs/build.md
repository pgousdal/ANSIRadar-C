# Build

ANSIRadar-C targets portable C99 and has no external runtime dependencies beyond the standard C library and math library. Builds use `-Wall -Wextra -Wpedantic`.

## Make

```sh
make
make test
make clean
```

Set `CC=clang` or another C99 compiler when required.

## CMake

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake keeps the C standard extensions disabled and builds the `ansiradar80` executable, its core library, and the test executable.
