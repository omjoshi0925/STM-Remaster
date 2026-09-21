# xlsStrings

MAIN.map is a newline-separated list of string keys. MAIN_<LANG>.data holds
the values in the same order, so key i pairs with value line i (592 keys in
the 1.0.1 build). Many keys have empty English values; they are placeholders
or used only by other languages.

Keys the runtime uses: STR_GAME_NAME, STR_LEVELNEW_<n>_NAME for the twelve
chapter cards, STR_VOLUME_<n>_TITLE for the comic collection. Values are
loaded from the user's own files at runtime and never stored in this repo.
Tools/list_strings.py lists keys and value lengths.
