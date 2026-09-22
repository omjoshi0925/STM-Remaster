# FAQ

Why not an emulator? The goal is a native app that renders the original
data on a modern GPU, not a shim around an old binary.

Why Metal? It is the iOS-native API and the renderer stays small.

Where are the assets? Not here, ever. You extract your own copy into
Assets/ (docs/ASSET_EXTRACTION.md).

Why so many host tests? The renderer cannot be built off-device, so every
format and gameplay claim is proven against the real files on a Mac first.

What is a milestone? A shipped, verified step with a five-section ledger in
the CHANGELOG saying what is real and what is not.
