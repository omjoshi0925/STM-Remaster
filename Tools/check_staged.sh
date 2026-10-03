#!/bin/bash
# Compile-checks the working tree exactly the way CI does, before committing.
# usage: Tools/check_staged.sh   (from the repo root, on the Mac)
# Every file is checked and every error printed; exit status is 1 if any fail.
fail=0
for f in NativePort/Sources/*.cpp; do
  clang++ -std=c++17 -fsyntax-only -ferror-limit=0 -I NativePort/Sources "$f" 2>&1 | grep -E ': error:' && fail=1
done
if SDK=$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null); then
  for f in NativePort/Sources/*.mm; do
    clang++ -x objective-c++ -std=gnu++17 -fobjc-arc -fmodules -fsyntax-only -ferror-limit=0 \
            -isysroot "$SDK" -target arm64-apple-ios16.0 -I NativePort/Sources "$f" 2>&1 | grep -E ': error:' && fail=1
  done
else
  echo "note: no iOS SDK (not on a Mac with Xcode) - Objective-C++ files skipped"
fi
for t in hosttests/*.cpp; do
  clang++ -std=c++17 -fsyntax-only -ferror-limit=0 -I NativePort/Sources "$t" 2>&1 | grep -E ': error:' && fail=1
done
[ $fail = 0 ] && echo "compile check passed" || echo "compile check FAILED"
exit $fail
