# configs.pack formats

## Field-stream .bin (EnemysAttributeConfigs, EnemysAttackConfigs, ...)
Layout: `u32 recordCountHint`, then a flat stream of fields:
- string: `u16 length` + ASCII bytes (no terminator)
- number: 4 bytes (float or u32 by context)
- **strings may start 2 bytes into a 4-byte phase** (odd-length strings shift
  alignment); parsers must test for a string at `off` and `off+2` before
  consuming a numeric, or they silently drop records (we recovered 25/25
  attribute records only after this fix).
Records begin at an UPPERCASE name (`THUG_KNIFE`, `SANDMAN`, `RHINO`...).

### EnemysAttributeConfigs.bin (25 records)
Numeric slots after the name: `f0` HP, `f1` move speed, `f2` vision radius
(7200 typical), `f7` melee range, `f9` ranged-attack range (gun 1200,
RHINO 3000). Example: THUG_KNIFE 40 HP / 150 speed / 200 melee.

### EnemysAttackConfigs.bin (90 rows)
Named attack rows; first numeric field is small (0-3 on `*_NONE`/reaction
rows). **Row-to-enemy linkage is not yet decoded** — engine damage values
remain placeholders on `EnemyStats.damage`.

### AttackIntervalTimeConfigs.bin
Melee attack interval 2000 ms.

### MC_STATE.bin
131 hero state names (includes `k_state_move_onwall` and the wall/web
traversal family — the data side of unimplemented movement).

## GS_*.json
Standard JSON **with // line comments** (strip before parsing; all 14 parse).
Screen definitions: buttons, dynamic text/sprite bindings.

## xlsStrings
`MAIN.map` = newline-separated string keys; `MAIN_<LANG>.data` = values,
**line-paired by index** (592 entries in EN). Level names:
`STR_LEVELNEW_<n>_NAME`.
