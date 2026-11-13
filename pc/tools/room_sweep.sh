#!/bin/bash
# room_sweep.sh BIN_A BIN_B OUTDIR ISO [ROOM...]
# layout_check.sh over many rooms: for each ROOM (stage:room, e.g. 0:6; default: every room file
# listed in ROOMS, one "RM_SRRV" name per line) both binaries are teleported there at the start of
# a new game and their frame dumps compared. Prints one line per room and a summary; exit status 1
# if any room differs or did not run. Env: LAST (default 700), ROOMS (file), and layout_check.sh's.
set -u
A=$1; B=$2; OUT=$3; ISO=$4; shift 4
here=$(dirname "$(realpath "$0")")
if [ $# -eq 0 ]; then
  set -- $(sed -nE 's/^RM_([0-9])([0-9]{2}).*/\1:\2/p' "${ROOMS:?ROOMS=file of RM_xxxx names, or give stage:room arguments}" | sort -u)
fi
mkdir -p "$OUT"; : > "$OUT/summary.txt"; bad=0
for r in "$@"; do
  stg=$((10#${r%%:*})); rom=$((10#${r##*:}))
  res=$(TELEPORT="1:$stg:$rom:0" LAST=${LAST:-700} MODES=${MODES:-vanilla} bash "$here/layout_check.sh" "$A" "$B" "$OUT/r$stg-$rom" "$ISO" 2>&1 | tr '\n' ' ')
  case "$res" in *" 0 differ "*) case "$res" in *"did not reach"*) st=SHORT;; *) st=OK;; esac;; *) st=DIFF;; esac
  if [ "$st" = DIFF ]; then   # once more: a window that opened at another size, a busy machine
    res=$(TELEPORT="1:$stg:$rom:0" LAST=${LAST:-700} MODES=${MODES:-vanilla} bash "$here/layout_check.sh" "$A" "$B" "$OUT/r$stg-$rom" "$ISO" 2>&1 | tr '\n' ' ')
    case "$res" in *" 0 differ "*) st=OK2;; esac
  fi
  case "$st" in OK*) ;; *) bad=1;; esac
  echo "$st $stg:$rom  $res" | cut -c1-240 | tee -a "$OUT/summary.txt"
  case "$st" in OK*) rm -rf "$OUT/r$stg-$rom" 2>/dev/null;; esac
done
exit $bad
