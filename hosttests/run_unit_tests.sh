#!/bin/bash
# Asset-free suites: run anywhere, including CI.
set -e
cd "$(dirname "$0")"
SRC=../NativePort/Sources
CXX=${CXX:-c++}
ENGINE="$SRC/BDAEModel.cpp $SRC/Level.cpp $SRC/Character.cpp $SRC/Combat.cpp $SRC/GameFlow.cpp $SRC/Script.cpp $SRC/Audio.cpp"
fail=0
for t in math trigger config_parser gameflow wav; do
  $CXX -std=c++17 -O2 -I "$SRC" -o "/tmp/stm_unit_$t" $ENGINE "${t}_unit_test.cpp"
  "/tmp/stm_unit_$t" | tail -1 || fail=1
done
exit $fail
