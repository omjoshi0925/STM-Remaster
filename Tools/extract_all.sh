#!/bin/bash
# Full extraction in one go: every pack, the boot movies, then verify.
# usage: Tools/extract_all.sh <OriginalPacks dir> <Assets dir> [ipa]
set -e
SRC="${1:?usage: extract_all.sh <OriginalPacks> <Assets> [ipa]}"
DST="${2:?usage: extract_all.sh <OriginalPacks> <Assets> [ipa]}"
HERE="$(cd "$(dirname "$0")" && pwd)"
"$HERE/extract_packs.sh" "$SRC" "$DST"
[ -n "$3" ] && "$HERE/extract_videos.sh" "$3" "$DST"
"$HERE/verify_assets.sh" "$DST"
