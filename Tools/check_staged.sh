#!/bin/bash
# Compile-checks the working tree the way CI does, before committing.
# usage: Tools/check_staged.sh   (from the repo root)
set -e
for f in NativePort/Sources/*.cpp; do clang++ -std=c++17 -fsyntax-only -I NativePort/Sources "$f"; done
if SDK=$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null); then
  for f in NativePort/Sources/*.mm; do
    clang++ -std=c++17 -fobjc-arc -fsyntax-only -isysroot "$SDK" -target arm64-apple-ios16.0 -I NativePort/Sources "$f"
  done
fi
for t in hosttests/*.cpp; do clang++ -std=c++17 -fsyntax-only -I NativePort/Sources "$t"; done
echo "compile check passed"
