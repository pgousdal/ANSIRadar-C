#!/bin/sh
set -eu

DOOR32=${1:?missing DOOR32.SYS path}

exec /home/mystic/doors/ansiradar-c/ansiradar80 \
  --door32 "$DOOR32" \
  --file /run/readsb/aircraft.json \
  --receiver-lat 58.3405 \
  --receiver-lon 6.2812 \
  --charset cp437 \
  --color always \
  --width 80 \
  --height 25
