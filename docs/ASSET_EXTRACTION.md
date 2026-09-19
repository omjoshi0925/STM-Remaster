# Asset extraction

You need your own legally obtained copy of the game. Nothing here is distributed.

1. Put the .pack files from the app bundle into <workbench>/OriginalPacks/.
2. Extract everything the runtime uses:

       Tools/extract_packs.sh OriginalPacks Assets entities configs sprites xlsStrings comic1 comic2 sounds
       Tools/extract_packs.sh OriginalPacks Assets levelnew_01 levelnew_02 levelnew_03 levelnew_04 levelnew_05 levelnew_06 levelnew_07 levelnew_08 levelnew_09 levelnew_10 levelnew_11 levelnew_12

   Every level pack matters: Level 1 references textures that ship in other packs.

3. Copy the boot movies:

       Tools/extract_videos.sh /path/to/game.ipa Assets

4. Check the tree:

       Tools/verify_assets.sh Assets
       hosttests/run_host_tests.sh Assets

5. Regenerate the Xcode project whenever a new Assets folder appears; CMake globs resources at configure time.
