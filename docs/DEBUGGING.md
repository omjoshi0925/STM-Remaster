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

Cinematics: `cinematic '<tag>': N threads, S s` when a script starts and `cinematic '<tag>' finished` when it ends; a load failure prints the script path and reason.

Audio: `sound slot tables: original (63 enemy slots, 38 hero slots)` means the decoded tables drive event choice; `name convention` means a config failed to load.

Cinematics: `cinematic camera track <file> (S s)` when a script drives the view from a camera BDAE; `QTE n success -> cinematic id` or `fail` when a window resolves.

Cinematics (Milestone 21): `cinematic actor <id>: <mesh> plays <file> from S s (D s, skinned|rigid, N batches)` per object thread bound; `does not ship` or `no scene node` when one cannot bind; `cinematic camera <file> (S s), far F, next N[, level end]` for the PlayDAECamera; `cinematic hero resumes at (x, y, z)` where play continues; `level N complete (authored end)` when a level-end script finishes; `chained cinematic N has no script` if a ^ID^Cinematic^Next is unresolved. `level strings: N subtitle lines` on level load.

Scripts (Milestone 22): `trigger '<tag>' -> <script>` on an enter edge; `script arms trigger N` / `disarms trigger N` for EnableTrigger/DisableTrigger; `script saves at checkpoint N`; `script GetDamage D -> HP H`; `script shows health of N`; `script unlocks skill '<id>' (skills not implemented)`; `cinematic '<tag>' (<path>): N threads, S s` for every script that starts, including zero-length beats, and `level_start` / `epilogue` / `next_N` / `qte_N` as tags for the ones no trigger fired; `level N complete (after the epilogue)`. A gated script prints nothing until its If* conditions hold: run `Tools/script_graph.py <Assets> levelnew_01` to see what a beat is waiting for.
