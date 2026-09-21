# Game flow

VIDEO (logo, trailer; tap skips) -> TITLE (chapter card, tap) -> PLAYING
-> DEAD (tap retries at the nearest RestorePoint) or COMPLETE (score
screen, tap loads the next level). COMIC is entered from PLAYING when a Comic
node is reached and returns to PLAYING on tap.

Checkpoints: the original CheckPoint markers, visited within 260 u; reaching
one restores health and moves the respawn point. Completion: the checkpoint
chain today, with the trigger runtime's boss-die rule ready to replace it.
Levels cycle 1 -> 2 -> 1 until more levels are wired.
