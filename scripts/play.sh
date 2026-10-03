#!/bin/zsh
# Launch the recompiled game. Usage: scripts/play.sh [path-to-game-folder]
REPO=${0:A:h:h}
GAME=${1:-$REPO/../retail-game}
cd "$REPO/out/build/mac-arm64-release" || { echo "Build first: scripts/build.sh"; exit 1; }
exec ./catherine --game_data_root="$GAME" --log_level=warning --log_file="$REPO/out/play.log"
