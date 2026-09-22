#!/bin/bash
# Zip the repo tree for hand-off, excluding assets, build products and git.
# usage: Tools/bundle_release.sh <name>
set -e
n="${1:?bundle name}"
cd "$(dirname "$0")/.."
zip -r -q "../$n.zip" . -x '.git/*' 'build-ios/*' 'Assets/*' 'OriginalPacks/*' '*.pack' '*.ipa' '*.apk' '*.so' '*.m4v' '*.DS_Store'
echo "../$n.zip"
