#!/bin/bash
# Compile and run every headless verification suite against real game data.
#   ./run_host_tests.sh /path/to/workbench/Assets
set -e
cd "$(dirname "$0")"
ASSETS="${1:-$HOME/Downloads/SpiderMan_Native_ARM64_Port_Workbench/Assets}"
SRC="../NativePort/Sources"
[ -d "$ASSETS/entities" ] || { echo "Asset root '$ASSETS' has no entities/ - pass the workbench Assets folder"; exit 1; }
CXX="${CXX:-c++}"
echo "== building suites =="
$CXX -std=c++17 -O2 -I "$SRC" -o /tmp/stm_selftest "$SRC/BDAEModel.cpp" bdae_selftest.cpp
$CXX -std=c++17 -O2 -I "$SRC" -o /tmp/stm_level    "$SRC/BDAEModel.cpp" "$SRC/Level.cpp" leveltest.cpp
$CXX -std=c++17 -O2 -I "$SRC" -o /tmp/stm_m4       "$SRC/BDAEModel.cpp" "$SRC/Level.cpp" "$SRC/Character.cpp" milestone4_test.cpp
$CXX -std=c++17 -O2 -I "$SRC" -o /tmp/stm_m5       "$SRC/BDAEModel.cpp" "$SRC/Level.cpp" "$SRC/Character.cpp" milestone5_test.cpp
$CXX -std=c++17 -O2 -I "$SRC" -o /tmp/stm_m6       "$SRC/BDAEModel.cpp" "$SRC/Level.cpp" "$SRC/Character.cpp" "$SRC/Combat.cpp" milestone6_test.cpp
echo "== bdae selftest ==";  /tmp/stm_selftest "$ASSETS/entities/meshes_bin/spiderman_mesh.bdae" "$ASSETS/entities/meshes_bin/spiderman_anim.bdae"
echo "== level test ==";     /tmp/stm_level "$ASSETS"
echo "== milestone 4 ==";    /tmp/stm_m4 "$ASSETS"
echo "== milestone 5 ==";    /tmp/stm_m5 "$ASSETS"
echo "== milestone 6 ==";    /tmp/stm_m6 "$ASSETS"
