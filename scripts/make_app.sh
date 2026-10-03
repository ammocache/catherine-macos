#!/bin/zsh
# Package the built game into out/Catherine.app (a normal macOS app bundle).
REPO=${0:A:h:h}
B=$REPO/out/build/mac-arm64-release
APP=$REPO/out/Catherine.app
[ -x "$B/catherine" ] || { echo "Build the game first (scripts/build.sh)"; exit 1; }
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$B/catherine" "$B"/*.dylib "$APP/Contents/MacOS/"
cp -R "$B/vulkan" "$APP/Contents/MacOS/"   # bundled MoltenVK + Vulkan loader
# Keep the player's settings if the app already has them; otherwise start from the dev config.
[ -f "$APP/Contents/MacOS/catherine.toml" ] || cp "$B/catherine.toml" "$APP/Contents/MacOS/" 2>/dev/null
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Catherine</string>
  <key>CFBundleDisplayName</key><string>Catherine</string>
  <key>CFBundleIdentifier</key><string>io.github.catherine-recomp</string>
  <key>CFBundleExecutable</key><string>catherine</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>0.1.0</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict></plist>
PLIST
# Sign every library first, then the app itself (ad-hoc signature, fine for local use).
for f in "$APP/Contents/MacOS"/*.dylib "$APP/Contents/MacOS/vulkan/lib"/*.dylib; do
  codesign --force --sign - "$f" || { echo "codesign failed on $f"; exit 1; }
done
# (The bundle as a whole is left unsigned for local dev: the Vulkan data folder inside
# Contents/MacOS blocks bundle signing. The executable keeps its linker signature.
# TODO for release: move vulkan/ into Contents/Frameworks + Resources and sign the bundle.)
echo "App ready: $APP"
