#!/bin/bash
# Sync this repo's sources into the local workbench (which holds the game
# Assets that never enter git), regenerate the Xcode project, and open it.
#   ./scripts/apply_to_workbench.sh [/path/to/workbench]
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WB="${1:-$HOME/Downloads/SpiderMan_Native_ARM64_Port_Workbench}"
[ -d "$WB/NativePort" ] || { echo "No workbench at '$WB'"; exit 1; }
[ -d "$WB/Assets" ] || echo "WARNING: '$WB/Assets' missing - run Tools/prepare_from_ipa.py first."

STAMP=$(date +%Y%m%d-%H%M%S)
if [ -d "$WB/NativePort/Sources" ]; then
  cp -R "$WB/NativePort/Sources" "$WB/NativePort/Sources.backup-$STAMP"
  echo "Backed up Sources -> Sources.backup-$STAMP"
fi
mkdir -p "$WB/NativePort/Sources" "$WB/NativePort/_superseded"
# retire milestone-1 files the current engine replaces (GLOB would compile them)
for f in BDAE324.cpp BDAE324.hpp GameRuntime.cpp GameRuntime.hpp; do
  [ -f "$WB/NativePort/Sources/$f" ] && mv "$WB/NativePort/Sources/$f" "$WB/NativePort/_superseded/" && echo "superseded: $f"
done
cp "$REPO/NativePort/Sources/"* "$WB/NativePort/Sources/"
cp "$REPO/NativePort/CMakeLists.txt" "$REPO/NativePort/Info.plist.in" "$WB/NativePort/"
echo "Sources synced from repo."

cd "$WB"
rm -rf build-ios && mkdir build-ios && cd build-ios
cmake -G Xcode ../NativePort \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=BHV8AWKA75 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic
echo; echo "Xcode project regenerated. Opening..."
open SpiderManTotalMayhemNative.xcodeproj
