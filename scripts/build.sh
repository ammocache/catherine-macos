#!/bin/zsh
# Build the game. Requires the ReXGlue SDK at ../tools/rexglue-sdk with patches/ applied.
REPO=${0:A:h:h}
SDK=${REXSDK_DIR:-$REPO/../tools/rexglue-sdk}
cd "$REPO" || exit 1
if [ ! -d out/build/mac-arm64-release ]; then
  cmake --preset mac-arm64-release -DREXSDK_DIR="$SDK" -DREXGLUE_ROOT="$SDK" || exit 1
fi
caffeinate -i cmake --build --preset mac-arm64-release --target catherine -j 6 || exit 1
D=out/build/mac-arm64-release
# The GPU plugin is built inside the SDK; copy it next to the game and re-sign it
# (macOS refuses to load a modified library with a stale signature).
cp "$SDK/out/mac-arm64/librexgpu-xenos.dylib" "$D/librexgpu-xenos.dylib"
codesign --force --sign - "$D/librexgpu-xenos.dylib"
"$REPO/scripts/make_app.sh" || exit 1
echo "Build done. Play with: scripts/play.sh  (or open out/Catherine.app)"
