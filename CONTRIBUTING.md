
## Splitting work into commits
Every commit must build on its own. When a change spans a header and its
users, keep it in **one** commit rather than staging a header edit whose
callers do not exist yet - a commit that does not compile breaks `git bisect`,
which is the main reason to keep commits small in the first place. Verify by
compiling each staged state, not by eye: this project has shipped a broken
intermediate exactly once, and the check below would have caught it.

    # for each staged commit, from a clean tree
    g++ -std=c++17 -fsyntax-only -I NativePort/Sources NativePort/Sources/*.cpp
