#!/bin/bash
# Compile and run one host suite. usage: Tools/run_one_test.sh <suite name> <Assets dir>
# e.g. Tools/run_one_test.sh milestone14 ~/path/Assets   or   Tools/run_one_test.sh math_unit
set -e
t="${1:?suite name}"; A="${2:-}"
cd "$(dirname "$0")/../hosttests"
SRC=../NativePort/Sources
c++ -std=c++17 -O2 -I "$SRC" -o "/tmp/stm_one" $SRC/*.cpp "${t}_test.cpp"
"/tmp/stm_one" ${A:+"$A"}
