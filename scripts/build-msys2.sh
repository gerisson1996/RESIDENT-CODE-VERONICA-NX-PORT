#!/bin/bash
# Native Windows build with MSYS2's mingw-w64 GCC (C:\msys64).
#   scripts/build-msys2.sh 32|64 [extra cmake -D args / ninja args after --]
# Output: build32/pc/recvx.exe or build64/pc/recvx.exe.
set -eu
bits=${1:?usage: build-msys2.sh 32|64}; shift
root=$(cd "$(dirname "$0")/.." && pwd)
msys=${MSYS2_ROOT:-/c/msys64}
case $bits in
  32) tc=mingw-i686.cmake;   pre=$msys/mingw32 ;;
  64) tc=mingw-x86_64.cmake; pre=$msys/mingw64 ;;
  *) echo "32 or 64" >&2; exit 2 ;;
esac
dir=${BUILD_DIR:-$root/build$bits}
cargs=(); nargs=()
while [ $# -gt 0 ]; do
  if [ "$1" = -- ]; then shift; nargs=("$@"); break; fi
  cargs+=("$1"); shift
done
export PATH="$pre/bin:$msys/mingw64/bin:$PATH"
cmake -S "$root" -B "$dir" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$root/cmake/$tc" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" "${cargs[@]}"
ninja -C "$dir" "${nargs[@]:-recvx}"
