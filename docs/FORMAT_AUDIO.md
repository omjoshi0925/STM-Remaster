# Audio formats

## VoxSounds.bin — the event table
A field-stream `.bin` (see FORMAT_CONFIGS.md) holding 493 records, each an
event name followed by its clip path and flags:

    M_DOWNTOWN_CALM            music/m_downtown_calm.wav     flags 0x20001
    SFX_PUNCH_IMPACT_1         VFX/.../punch_impact_1.wav    flags ...

Naming is systematic and is what the runtime resolves against:
- `M_*` music beds (`M_DOWNTOWN_CALM/_MIXED`, `M_BOSS_<BOSS>`, `M_WIN`,
  `M_LOSE`, `M_SCORE_SCREEN`, `M_TITLE`)
- `SFX_<STAT>_HURT_1..3`, `SFX_<STAT>_DIES`, `SFX_<STAT>_VOICE_n` per enemy
  archetype, matching the `EnemysAttributeConfigs` row names
- hero effects: `SFX_PUNCH_IMPACT_n`, `SFX_PUNCH_SWOOSH_n`, `SFX_WEB_THROW_n`,
  `SFX_HURT_n`, `SFX_DIE`, `SFX_LAND`, `SFX_JUMP_SWOOSH_n`
- pickups and UI: `SFX_ORBS_COLLECT`, `SFX_SPECIAL_COLLECT`,
  `SFX_SPIDER_LOGO_IN/_OUT`, `SFX_SPIDER_SENSE_IN/_OUT`, `SFX_SCORE_WHOOSH`

`Tools/dump_vox.py <Assets> --check` lists the table and verifies every clip
is present.

## sounds.pack — the clips
501 RIFF WAV files in `music/` (31), `VFX/` (96) and `sfx/` (6 category
folders), all **IMA ADPCM, WAVE format tag 17**, 4-bit, 22.05 kHz mono for
effects and 32 kHz stereo for music beds. Core Audio will not reliably open
format 17 in a WAV container, so `Audio.cpp` decodes it directly:

- `fmt ` chunk gives channels, sample rate and `blockAlign`
- each block starts with a per-channel preamble (int16 predictor, uint8 step
  index, uint8 pad), then interleaved 4-byte groups of 8 nibbles per channel
- standard DVI/IMA step and index tables; low nibble first

Decoded output is interleaved PCM16 (`WavClip`), which the platform layer
converts to float buffers for AVAudioEngine.

## Slot vocabularies (not yet linked)
`BehaviorSoundMapList.bin` names the 63 enemy/boss behaviour sound slots the
original AI triggers (`Voice_1..12`, `hurt1..3`, `dies`, `attack_swoosh`,
`attack_strike`, `gun_shoot`, plus boss-specific slots such as
`sand_hand_attack`, `rhino_fs_1`, `venom_scream`, `green_goblin_roar`).
`MC_SOUND.bin` names the 38 hero slots (`k_mc_sfx_swoosh_punch`,
`k_mc_sfx_land`, `k_mc_sfx_web_throw`, `k_mc_sfx_wall_climb`, ...).
The numeric linkage from a slot to a VoxSounds event is **not decoded**; the
runtime resolves per-archetype events by name convention instead
(`VoxTable::soundsFor`). Decoding that linkage would replace our event
choices with the original ones - a good next step.
