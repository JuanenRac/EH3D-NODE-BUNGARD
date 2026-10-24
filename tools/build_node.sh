#!/usr/bin/env bash
# EH3D-NODE-BUNGARD - builds the firmware in the official ESP-IDF container.
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#
#   tools/build_node.sh [s3-eth|s3-wifi]      # writes dist/eh3d-node-bungard-BOARD.bin (one merged image, flashed at address 0)
#
# BOARD is s3-eth (Waveshare ESP32-S3-ETH, W5500 Ethernet with PoE) or s3-wifi (ESP32-S3-WROOM-1 N16R8, Wi-Fi only). An image is
# for ONE board: flash it only to that board.
#
# Nothing secret goes into the image: the access token of the control link, the Wi-Fi network and the pins are settings edited from the node's
# panel after it has been flashed. Without a token the node refuses to be controlled.
# Run it from Linux, or from WSL on Windows (Docker needed).
set -euo pipefail
BOARD="${1:-s3-eth}"
[[ "$BOARD" == "s3-wifi" || "$BOARD" == "s3-eth" ]] || { echo "the board is s3-wifi or s3-eth, not '$BOARD'" >&2; exit 2; }
IMAGE="eh3d-node-bungard-$BOARD"
IDF_IMAGE="${EH3D_IDF_IMAGE:-espressif/idf:v5.5.5}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULTS="sdkconfig.defaults;sdkconfig.board.$BOARD"

DOCKER="docker"
docker info >/dev/null 2>&1 || DOCKER="sudo docker"
mkdir -p "$ROOT/dist" "$ROOT/build"
# One build at a time and only some of the cores, so the computer keeps answering while it builds.
$DOCKER run --rm --cpuset-cpus="${EH3D_BUILD_CPUSET:-0-3}" -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT":/project -w /project "$IDF_IMAGE" bash -c "
  set -e
  BUILD=build/$IMAGE
  idf.py -B \$BUILD -D SDKCONFIG=\$BUILD/sdkconfig -D 'SDKCONFIG_DEFAULTS=$DEFAULTS' set-target esp32s3 >/dev/null
  idf.py -B \$BUILD -D SDKCONFIG=\$BUILD/sdkconfig -D 'SDKCONFIG_DEFAULTS=$DEFAULTS' build
  cd \$BUILD
  python -m esptool --chip esp32s3 merge_bin -o /project/dist/$IMAGE.bin @flash_args
"
ls -l "$ROOT/dist/$IMAGE.bin"
sha256sum "$ROOT/dist/$IMAGE.bin" | cut -d' ' -f1 | sed "s/^/sha256 /"
echo "flash it (to a $BOARD board only) with: python -m esptool --chip esp32s3 -p COMx write_flash 0x0 dist/$IMAGE.bin"
