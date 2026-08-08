CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?= -lm

CORE = csrc/models/aircraft.c csrc/providers/providers.c \
       csrc/renderer/ansi.c csrc/renderer/charset.c csrc/renderer/radar.c csrc/renderer/screen.c \
       csrc/ui/input.c csrc/core/door32.c csrc/core/transport.c

.PHONY: all test clean

all: ansiradar80

ansiradar80: csrc/core/main.c csrc/ui/radar_view.c $(CORE)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

ansiradar80-test: ctests/test_core.c csrc/ui/radar_view.c $(CORE)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

ansiradar80-door-test: ctests/test_door.c csrc/ui/radar_view.c $(CORE)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test: ansiradar80 ansiradar80-test ansiradar80-door-test
	./ansiradar80-test fixtures80/readsb.json
	./ansiradar80-door-test

clean:
	rm -f ansiradar80 ansiradar80-test ansiradar80-door-test
