# Debugging on the device

Filter the Xcode console on `TotalMayhem`. Lines you should see at start:
texture index (expect 284 files with all packs), any `texture missing` names,
`HUD atlas ok, font atlas ok`, `enemies placed`, `props: N placed`,
`audio ready: 493 events`, `sky: N batches`, and `level 0 (levelnew_01)`.

During play: `trigger '<tag>' -> <cff>` as volumes fire, `unknown sound
event` if an event name is wrong, `comic page N not found` if comic packs
are missing.

Set `TM_DEBUG_HUD 1` in Renderer.mm to show the status line (HP, score,
enemies, checkpoints, fps, PAUSED).

Failure signatures seen so far: scrambled geometry = a CPU/MSL vertex stride
mismatch; everything dark = vertex colour modulate missing; red circles =
wrong atlas rectangles; no HUD = a texture format the loader rejected; a
flood of undeclared-identifier errors = one missing include broke the ivar
block.
