#!/bin/zsh
# Package the built game into out/Catherine.app (a normal macOS app bundle).
REPO=${0:A:h:h}
B=$REPO/out/build/mac-arm64-release
APP=$REPO/out/Catherine.app
[ -x "$B/catherine" ] || { echo "Build the game first (scripts/build.sh)"; exit 1; }
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
rm -rf "$APP/Contents/_CodeSignature"   # stale bundle seal would block launch
cp "$B/catherine" "$B"/*.dylib "$APP/Contents/MacOS/"
rm -rf "$APP/Contents/MacOS/vulkan" "$APP/Contents/Resources/vulkan"
cp -R "$B/vulkan" "$APP/Contents/Resources/vulkan"   # bundled MoltenVK + Vulkan loader
rm -rf "$APP/Contents/Resources/fonts"; cp -R "$REPO/assets/fonts" "$APP/Contents/Resources/fonts"
# App icon: a personal icon in local-assets/ (never committed) takes priority
# over a project icon in assets/ (if one is ever added).
ICON_PLIST=""
rm -f "$APP/Contents/Resources/AppIcon.icns"
for set in "$REPO/local-assets/AppIcon.iconset" "$REPO/assets/AppIcon.iconset"; do
  if [ -d "$set" ]; then
    iconutil -c icns "$set" -o "$APP/Contents/Resources/AppIcon.icns" && \
      ICON_PLIST="<key>CFBundleIconFile</key><string>AppIcon</string>"
    break
  fi
done
# Settings live in ~/Library/Application Support/Catherine, never inside the app.
rm -f "$APP/Contents/MacOS/catherine.toml"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Catherine</string>
  <key>CFBundleDisplayName</key><string>Catherine</string>
  <key>CFBundleIdentifier</key><string>io.github.catherine-recomp</string>
  <key>CFBundleExecutable</key><string>catherine</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0.0</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
  $ICON_PLIST
</dict></plist>
PLIST
# Sign every library first, then the whole app (ad-hoc signature, fine for local use).
for f in "$APP/Contents/MacOS"/*.dylib "$APP/Contents/Resources/vulkan/lib"/*.dylib; do
  codesign --force --sign - "$f" || { echo "codesign failed on $f"; exit 1; }
done
codesign --force --sign - "$APP" || { echo "codesign failed on app"; exit 1; }
touch "$APP"   # make Finder/Dock pick up icon changes
echo "App ready: $APP"
