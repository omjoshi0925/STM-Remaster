# Combat model (current)

Enemy stats come from EnemysAttributeConfigs.bin per archetype: HP, move
speed, vision radius, melee range, ranged range. AI states: IDLE, CHASE,
ATTACK, COOLDOWN (2000 ms from AttackIntervalTimeConfigs), HURT, DEAD.
Ranged archetypes (gun, molotov, hammer, big) engage from their ranged range.

Hero: 3-hit chain on original clips, punch_right (10) then far_attack (10)
then backflip_kick (15); a tap during the recover window chains. Reach 260 u
in a 75 degree arc. Web attack: 25 web power, 15 damage, nearest foe within
800 u in front. Knockback: 70 u straight back, navmesh gated.

Placeholders, in one place (EnemyStats.damage): thugs 5, bosses 12 per hit.
The AttackConfigs row linkage that would replace these is undecoded.
