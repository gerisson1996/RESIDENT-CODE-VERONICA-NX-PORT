#!/bin/bash
# Nintendo Switch build (homebrew NRO) with devkitPro's devkitA64 + libnx + switch-sdl2/mesa.
#   scripts/build-switch.sh [extra cmake -D args / ninja args after --]
# Output: build-switch/pc/recvx.elf (and .nro once linked). Always the position-independent
# 64-bit build: Horizon loads the executable at an address of its choosing.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
dkp=${DEVKITPRO:-/opt/devkitpro}   # run from devkitPro's msys2 shell: its cmake (devkitPro refuses another)
export DEVKITPRO=$dkp
dir=${BUILD_DIR:-$root/build-switch}
cargs=(); nargs=()
while [ $# -gt 0 ]; do
  if [ "$1" = -- ]; then shift; nargs=("$@"); break; fi
  cargs+=("$1"); shift
done
cmake -S "$root" -B "$dir" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$dkp/cmake/Switch.cmake" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" -DRECVX_64=ON -DRECVX_PIE=ON "${cargs[@]}"
ninja -C "$dir" "${nargs[@]:-recvx}"
