#!/bin/sh
# Play the cross-built ts.exe under Wine: CrossOver on a Mac, wine on Linux.
# The host finds the game through game/ (a link to the install, or a copy).
#
#   ./play.sh                       # build/ts.exe --run
#   ./play.sh --run --debuglog      # any host flags
#
# On a Mac CX_BOTTLE picks the bottle (default Steam); WINE another wine
# launcher, anywhere.
cd "$(dirname "$0")"
exe=build/ts.exe
[ -f "$exe" ] || { echo "no $exe: run ./build.sh first" >&2; exit 1; }
[ $# -gt 0 ] || set -- --run
# The game folder's wsock32.dll is IPXEmu, the network games' IPX; Wine's own has none.
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-wsock32=n,b}"
if [ "$(uname)" = Darwin ]; then
  WINE=${WINE:-/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine}
  exec "$WINE" --bottle "${CX_BOTTLE:-Steam}" --workdir "$PWD" "$PWD/$exe" "$@"
fi
exec "${WINE:-wine}" "$PWD/$exe" "$@"
