# xlsStrings

MAIN.map is a newline-separated list of string keys (592 lines in the 1.0.1
build; the last, STR_<name>_STR_NUM, is the count key). MAIN_<LANG>.data is
**binary**, not line-paired text (Milestone 21 correction):

    u32 count                     591 for MAIN_EN, 18 for levelnew_01_EN
    u32 offset[count]             ascending, relative to the end of this table
    UTF-16LE strings              NUL-terminated, string i at offset[i]

Key i pairs with string i. Decoded properly, 589 of the 591 MAIN strings have
English text (the earlier "448 empty values" was an artefact of reading the
binary as lines): STR_GAME_NAME = "Ultimate Spider-Man: Total Mayhem",
STR_LEVELNEW_1_NAME = "SAND IN YOUR FACE", STR_LEVELNEW_2_NAME =
"RHINO-SERIOUS RAMPAGE", STR_BOSS_NAME_01 = "SANDMAN".

Each level has its own table, levelnew_NN.map + levelnew_NN_<LANG>.data,
holding the cinematic subtitle lines that ShowMessage names by
$LEVEL_STRINGID (Level 1: 18 lines, STR_PROLOGUE_SPIDERMAN_01 = "My
spider-sense has been going wild all morning! What the heck's wrong?!";
Level 2: 10). Tutorial.map/.data holds the tutorial prompts. The runtime
loads MAIN at boot and merges the level table on level load
(StringTable::merge).

Keys the runtime uses: STR_GAME_NAME, STR_LEVELNEW_<n>_NAME for the twelve
chapter cards, STR_VOLUME_<n>_TITLE for the comic collection. Values are
loaded from the user's own files at runtime and never stored in this repo.
Tools/list_strings.py lists keys and value lengths.
