# BDAE skinning and animation

Skin: a joint table (Spider-Man 38 joints, thugs about 30, Sandman 21,
Rhino 36) with per-joint inverse bind matrices, a bind-shape matrix, and
per-vertex ushort4 indices plus float4 weights. Bone slots are triple
buffered on the device (kBoneSlot 2560 bytes = 40 matrices) because the GPU
reads them while the next frame is written.

Animation: 46 channels for Spider-Man, one per scene node, all sampled on a
shared timeline; clips are a 12-byte table of name, start ms and end ms
(242 clips for the hero). Enemy rigs share clip names across archetypes
(idle, idle_at1_idle, idle_hurt_idle, air_to_onground) so one picker serves
all of them; the knife thug shares thug_bat_anim.

Poses are computed on the CPU per frame per visible actor; with 37 enemies
that is the largest CPU cost in a frame today (see docs/PERFORMANCE.md).
