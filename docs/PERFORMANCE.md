# Performance notes

Per frame in Level 1: 39 level batches (texture keyed), about 500 prop draws
(232 instances, distance culled at 9000 u), 37 skinned enemies posed on the
CPU, the hero, a few sky batches, and one sprite pass with a handful of
texture segments. The debug line shows fps when TM_DEBUG_HUD is on.

Known costs: CPU skinning per enemy per frame; per-draw uniform uploads for
props; decoded music beds around 15 MB of PCM each.

Ideas, in order of payoff: skip posing enemies outside the view frustum,
instance props per archetype, stream music instead of decoding whole beds,
merge props into static batches per room.
