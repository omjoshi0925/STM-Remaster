
## Splitting work into commits
Every commit must build on its own. When a change spans a header and its
users, keep it in **one** commit rather than staging a header edit whose
callers do not exist yet - a commit that does not compile breaks `git bisect`,
which is the main reason to keep commits small in the first place. Verify by
compiling each staged state, not by eye: this project has shipped a broken
intermediate exactly once, and the check below would have caught it.

    # for each staged commit, from a clean tree
    g++ -std=c++17 -fsyntax-only -I NativePort/Sources NativePort/Sources/*.cpp

## CI must be green
A red compile-check or ios-build run is a broken build, not noise. Both
workflows ran red for two weeks (2026-09-19 to Milestone 22.1) while the
Linux-side checks passed, because those checks never compiled Objective-C++:
a g++ syntax pass over the `.cpp` files plus `Tools/mm_lint.py` cannot see an
undeclared local or a misplaced brace inside `Renderer.mm`. Before pushing a
change that touches a `.mm` file, run `make check` on the Mac (it compiles
every `.mm` against the iOS SDK with the Xcode flags), and look at the
Actions tab after pushing; errors appear as annotations with file and line
(docs/CI.md).
