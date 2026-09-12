#!/bin/bash
# Copies the boot movies out of the original IPA into Assets/videos/.
# The app plays Gameloft-Logo.m4v then Spiderman-Trailer.m4v at launch.
#
# usage: extract_videos.sh <path to .ipa> <Assets dir>
set -e
IPA="${1:?usage: extract_videos.sh <ipa> <Assets dir>}"
DST="${2:?usage: extract_videos.sh <ipa> <Assets dir>}/videos"
mkdir -p "$DST"
unzip -o -j "$IPA" 'Payload/*.app/Gameloft-Logo.m4v' 'Payload/*.app/Spiderman-Trailer.m4v' -d "$DST"
ls -la "$DST"
