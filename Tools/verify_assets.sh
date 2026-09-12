#!/bin/bash
# Checks an Assets/ tree for everything the runtime expects, before building.
# Exits non-zero on a missing requirement; warnings do not fail.
A="${1:?usage: verify_assets.sh <Assets dir>}"
fail=0; warn=0
need_dir() { if [ -d "$A/$1" ]; then echo "  ok   $1 ($(find "$A/$1" -type f | wc -l | tr -d ' ') files)"; else echo "  MISS $1 - $2"; fail=1; fi; }
want_dir() { if [ -d "$A/$1" ]; then echo "  ok   $1"; else echo "  warn $1 - $2"; warn=1; fi; }
need_file() { if [ -f "$A/$1" ]; then echo "  ok   $1"; else echo "  MISS $1 - $2"; fail=1; fi; }
want_file() { if [ -f "$A/$1" ]; then echo "  ok   $1"; else echo "  warn $1 - $2"; warn=1; fi; }
echo "Assets check: $A"
need_dir entities        "entities.pack - characters, props"
need_dir levelnew_01     "levelnew_01.pack - Level 1"
need_dir levelnew_02     "levelnew_02.pack - Level 2"
need_dir configs         "configs.pack - enemy stats, states"
need_dir sprites         "sprites.pack - HUD atlas and fonts"
need_dir xlsStrings      "xlsStrings.pack - level names and UI text"
need_file sprites/interface.tga        "HUD atlas"
need_file sprites/font_outline_big.tga "UI font"
need_file xlsStrings/MAIN.map          "string keys"
need_file xlsStrings/MAIN_EN.data      "string values"
need_file entities/meshes_bin/spiderman_mesh.bdae "hero mesh"
need_file entities/meshes_bin/spiderman_anim.bdae "hero animation"
want_dir comic1          "comic pages for story beats"
want_dir videos          "boot movies (Tools/extract_videos.sh)"
want_file levelnew_01/meshes_bin/lvl01_sky.bdae "Level 1 skybox"
for n in 03 04 05 06 07 08 09 10 11 12; do
  [ -d "$A/levelnew_$n" ] || { echo "  warn levelnew_$n - L1/L2 reference textures from other level packs"; warn=1; }
done
echo
if [ "$fail" -ne 0 ]; then echo "RESULT: missing required assets (see MISS lines)"; exit 1; fi
echo "RESULT: all required assets present${warn:+ (with warnings)}"
