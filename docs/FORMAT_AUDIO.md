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

## Slot tables (decoded)
`BehaviorSoundMapList.bin` is a matrix: each of the 63 slot rows (`Voice_1..12`,
`hurt1..3`, `dies`, `attack_swoosh`, `attack_strike`, `gun_shoot`, boss slots)
holds one VoxSounds **row index** per archetype column, `0xffffffff` for none.
Column order is read off the `dies` row (`SFX_<ARCHETYPE>_DIES`): THUG_KNIFE,
THUG_BAT, THUG_MOLOTOV, THUG_GUN, THUG_ROCKET, SLEDGER (the hammer thug), then
zombie, charger and goblin columns. 318 filled cells, all valid.

`MC_SOUND.bin` holds the 38 hero slots. Read each row's numbers as a u16
stream: four flag halves, then a variant count, then that many VoxSounds row
indices (`k_mc_sfx_swoosh_punch` -> rows 60, 61 = `SFX_PUNCH_SWOOSH_1/2`;
`k_mc_sfx_hurt` -> three variants). 70 variants, all valid.

The runtime (`BehaviorSoundMap`, `HeroSoundMap`) resolves enemy hurt, death,
voice, attack swoosh and gun shots, and the hero's punch and kick swooshes,
hurt and web throw, from these tables; the name convention only fills empty
cells. `Tools/dump_sound_slots.py` prints both resolved.
