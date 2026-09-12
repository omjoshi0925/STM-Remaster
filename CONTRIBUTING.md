# Working on STM-Remaster

## Ground rules
1. **The original data is the source of truth.** If the game files say it,
   implement that. Never invent a constant the data already contains.
2. **Verify before shipping.** Every format claim gets a host test that runs
   against the real assets (`hosttests/run_host_tests.sh <Assets>`); every
   sprite rectangle gets a rendered proof in `docs/`.
3. **No game assets in this repository.** `.pack`, `.ipa`, `.apk`, `.so`,
   media files and extracted `Assets/` are gitignored and stay that way.
4. **Report honestly.** The CHANGELOG ledger has five sections - implemented,
   verified locally, requires device validation, known limitations, not yet
   implemented - and a thing only moves up a section when it is true.

## Before you commit
    hosttests/run_host_tests.sh ~/path/to/Assets      # all suites green
    Tools/verify_assets.sh      ~/path/to/Assets      # extraction sane
    cd build-ios && xcodebuild -project SpiderManTotalMayhemNative.xcodeproj \
        -target SpiderManTotalMayhem -sdk iphoneos CODE_SIGNING_ALLOWED=NO build

## Adding a format finding
Document it in the matching `docs/FORMAT_*.md` with the byte offsets, add a
case to `hosttests/format_regression_test.cpp` so a future change cannot
silently break it, and note the discovery in the CHANGELOG.

## Renderer changes
`Renderer.mm` cannot be compiled on a host, so it is checked structurally
(brace/paren balance, every `[self ...]` selector defined, CPU/MSL struct
strides matching). Keep all vertex-struct members 4-byte aligned: Metal
rounds packed structs and a stride mismatch shows up as scrambled geometry
on the device, not as a compile error.

## Lessons already paid for
- `str.replace` that does not match is silent - assert every patch anchor.
- Build and run the suites **from the tree you are about to ship**, not from
  a local scratch copy; that check has caught two shipped-bundle defects.
- Do not guess atlas rectangles statistically. Decode the atlas, look at it,
  crop the candidate, look again (`Tools/inspect_atlas.py`).
