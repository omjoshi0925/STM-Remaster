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

Cinematics
- [ ] Trigger_Lv1_Start plays a letterboxed script with SKIP
- [ ] A TAP! prompt during a QTE; tapping and lapsing lead to different scripts
- [ ] Walking into a hostage awards +100

Cinematic actors and subtitles (Milestone 21)
- [ ] Prologue: Spider-Man swings in from above and lands beside the spawn (not offset or rotated away from the scene)
- [ ] Three thugs and a cop stand near the girl and animate at 16 s; the police car drives in at 35 s
- [ ] Subtitles appear in the lower bar in the outlined font ("My spider-sense has been going wild..."), Spider-Man's portrait on his lines
- [ ] The camera follows the authored track (not the gameplay orbit) for the whole 53 s, then the chained script plays before play resumes
- [ ] Play resumes with Spider-Man where the animation left him; the knocked-out thugs stay down
- [ ] Skipping the prologue also lands him at the end position
- [ ] Level 1 end: Sandman, the car and Rhino animate; the level completes when the script ends
- [ ] Level 2 end: Rhino's own animation replaces the boss; two cops and the car arrive
- [ ] No T-posed characters standing in the world before their scene

Script graph (Milestone 22)
- [ ] Tapping the chapter card starts the prologue at once (no walking into a volume first)
- [ ] After the prologue, the tutorial cards appear in sequence (PUNCH, WEB, JUMP prompts) and a Timer -1 card waits for a tap; SKIP is hidden while a card waits
- [ ] The three-thug beat: the next door/beat does not open until the thugs are down (gated script), then it fires
- [ ] A scripted save: dying after a Save beat respawns at that checkpoint, not the previous one
- [ ] Scripts that fade to black do so (BlackEnable) and scripts that forbid skipping ignore taps
- [ ] Camera shake on the crash beat
- [ ] Level 1 end: after the boss script the epilogue (girl, camera flash) plays, then the score screen
- [ ] Console shows `script arms trigger` lines as beats chain; no `unknown trigger` lines

Boss phases, slow motion, level ends (Milestone 23)
- [ ] Level 2 Rhino: at about two thirds health a phase script plays, again at one third
- [ ] Slow-motion beats (the crash, boss knock-downs) visibly slow the scene and its sounds, then return to normal speed
- [ ] A tutorial card that waits for a tap freezes the scene behind it; tapping resumes from the same moment
- [ ] Walking into a second trigger during a scene: that beat plays right after the scene, not never
- [ ] Touring every checkpoint does not end Level 1 early; the level ends after the boss script and the epilogue
