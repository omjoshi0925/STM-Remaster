# Device test checklist

Run after every renderer or asset change. Tick in order.

Boot
- [ ] Gameloft logo clip plays, tap skips it
- [ ] Trailer plays, tap skips it
- [ ] Chapter card shows the original level name in the yellow font

HUD
- [ ] Pause bubble top left, tapping it dims the scene and shows PAUSED
- [ ] Spider-Man portrait, green health bar in its frame, web meter
- [ ] Blue joystick in its ring, three red buttons with white icons
- [ ] No debug text, no stray red circles in the scene

Gameplay
- [ ] Left drag moves, character faces movement direction
- [ ] Punch lands, +10 popup, mashing shows the COMBO art
- [ ] Web button drains the meter and snaps a nearby thug
- [ ] Thugs shout on aggro, distinct hurt and death sounds
- [ ] Music plays, shifts when three or more thugs engage
- [ ] Checkpoint popup and health restore when reached
- [ ] Bonus tokens collect with a sound
- [ ] Console shows trigger lines as you walk the level

Death and completion
- [ ] Death shows the message, tap retries at a restore point
- [ ] Reaching every checkpoint shows score and checkpoints, tap loads Level 2

Report: screenshot of chapter card, gameplay, and the Xcode console filtered on TotalMayhem.
