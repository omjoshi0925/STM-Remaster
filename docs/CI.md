# Continuous integration

Two workflows run on every push and pull request (macos-latest, Xcode 26.x):

| workflow | what it proves |
|---|---|
| compile-check | `Tools/mm_lint.py`; every engine `.cpp`; every `.mm` against the iOS SDK with the Xcode project's flags (`-x objective-c++ -std=gnu++17 -fobjc-arc -fmodules`); every host suite compiles; the asset-free unit suites pass |
| ios-build | the real device build: `cmake -G Xcode` then `xcodebuild -sdk iphoneos CODE_SIGNING_ALLOWED=NO` |

No game assets are needed or present on the runner, so the data-driven host
suites (`run_host_tests.sh`) stay local.

## Reading a failure
Every compiler error becomes a red annotation on the run's summary page, with
file and line. When the device build fails without a compiler error (link,
signing, project generation) the last 25 lines of the build log appear as
warnings instead. Locally, `make check` (or `Tools/check_staged.sh`) runs the
same commands and prints every error.

## History
compile-check failed on every push from its first run (2026-09-19) to
Milestone 22.1. The failures were real:
- the enemy update loop in Renderer.mm had lost its braces, so the hit
  handling sat outside the loop (`f` undeclared, `continue` not in a loop);
  the device build has been broken since Milestone 15;
- Milestone 22's `startCinematic` rewrite dropped its `levelDir` declaration;
- `typeof` is a GNU extension the strict `-std=c++17` check rejected (now
  `__typeof__`, and CI uses gnu++17 like Xcode);
- `mkdtemp`/`mkstemp` need `<unistd.h>` under macOS libc++.
The old step stopped at the first failing file and printed nothing useful, and
the Linux-side checks used for bundles never compiled Objective-C++, which is
how these got through. ios-build now runs the device build itself.
