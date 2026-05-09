# Wave Enemy AI Review and Incremental Improvement Plan

## 1. Current code review

### Enemy / EnemyBase responsibilities
- `EnemyBase` owns the reusable physical/visual enemy foundation: collider behavior, humanoid model pieces, HP, damage, hit flash, death break-apart, and bullet collision dispatch.
- `Enemy` owns tactical behavior on top of `EnemyBase`: finite-state machine, perception, firing, movement, A* assisted waypointing, cover selection, evade/retreat/stuck controllers, and animation state.
- The current split is mostly healthy: death, HP bar compatibility, and collider lifetime remain in `EnemyBase`, while FPS combat decisions stay in `Enemy` and state classes.

### Current state management
- The active states are `Idle`, `CombatMove`, `Shoot`, `Search`, and `Dead`.
- `Idle` wanders and transitions to `CombatMove` when the player is seen.
- `CombatMove` updates last-seen memory, evaluates retreat/evade/cover plans, repositions when LOS is blocked, and enters `Shoot` when distance and LOS allow.
- `Shoot` fires only while the target remains visible/shootable and periodically returns to `CombatMove` for repositioning.
- `Search` moves toward the last seen/investigation point and sweeps locally until the search timer expires.

### Wave / scene integration
- `WaveManager` currently only spawns positions; it has no enemy role, trait, weapon, or squad metadata.
- `CharacterWorld` injects target, bullet manager, collision manager, and effect system into each spawned enemy.
- `GamePlayScene` can stay lean if wave role data is converted into an `EnemySpawnRequest` and interpreted inside `CharacterWorld` / enemy setup code.

## 2. Problem list

1. **Damage awareness was incomplete**: base bullet damage reduced HP and played effects, but AI memory/debug data did not consistently preserve attack origin or force a search/combat response when the player was behind cover.
2. **Shoot state could keep fighting at bad spacing**: LOS/fire range existed, but too-close spacing was not a hard reason to leave `Shoot`.
3. **Debug visibility was insufficient**: there was no consolidated per-enemy ImGui panel for current AI state, LOS, last hit time, last known player position, and attack availability.
4. **Weapon-counterplay is not yet represented**: player weapon state exists elsewhere, but enemy tactical inputs do not yet consume weapon category, reload state, or ammo pressure.
5. **Wave roles are not data-driven**: all wave enemies are spawned as the same `Enemy` behavior profile.
6. **Navigation is pragmatic but not authored**: AABB-based A* and stuck recovery exist, but there are no designer-authored lanes, cover nodes, or stage tactical annotations.

## 3. Priority improvements

### P0: Minimal FPS readability
- On damage, record hit position, attack origin/direction, and enter combat/search immediately.
- Require LOS for shooting and leave `Shoot` if the target is too close or too far.
- Add ImGui AI telemetry per enemy.

### P1: Tunable combat feel
- Introduce a small `EnemyCombatTuning` data asset/profile for fire interval, burst count, accuracy cone, ideal range, retreat threshold, and reaction time.
- Add burst fire and reload windows to prevent perfectly regular single shots.
- Add suppression-style movement: if recently hit, strafe/retreat or move to cover for a short duration.

### P2: Cover and navigation authoring
- Add lightweight `StageTacticalNode` / `CoverPoint` authoring: position, normal, exposure direction, radius, tags.
- Let `EnemyCoverSelector` prefer authored cover points before sampling around the enemy.
- Add waypoint lanes or grid cells per stage before considering full NavMesh.

### P3: Weapon-aware AI
- Expose a read-only `PlayerWeaponSnapshot`: weapon category, effective range, reload state, ammo in magazine, recently fired, and muzzle direction.
- Add enemy reactions:
  - melee/shotgun: backpedal, split laterally, avoid clustering.
  - rifle/sniper: prefer hard cover and short peeks.
  - player reload/empty: rush or wide-flank.

### P4: Wave roles and squads
- Extend wave data with enemy role (`Rush`, `Shooter`, `Cover`, `Tank`, `Support`) and optional squad id.
- Map roles to trait profiles and combat tunings during `CharacterWorld::SpawnEnemy`.
- Add simple squad spacing to avoid all enemies taking the same cover or path.

## 4. Classes to add/change

### Change existing
- `Enemy`: owns current tactical state and should keep receiving only compact inputs/snapshots.
- `EnemyBase`: keep damage/death/visual ownership; avoid adding combat decisions here.
- `EnemyCombatMoveState` / `EnemyShootState` / `EnemySearchState`: continue to host state-specific transitions.
- `WaveManager`: extend spawn entries with enemy role and profile id later.
- `CharacterWorld`: translate wave spawn requests into initialized enemy profiles.

### Add later
- `EnemyPerception`: LOS/FOV/last-known-position and damage stimulus memory.
- `EnemyCombatController`: burst/reload/accuracy/fire permission decisions.
- `EnemyNavigation`: waypoint/grid/cover path planning wrapper.
- `EnemyTacticalProfile`: role-level ranges, aggression, cover preference, weapon behavior.
- `PlayerWeaponSnapshot`: safe read-only player weapon context for AI.
- `StageTacticalNode` / `CoverPoint`: designer-authored navigation/cover hints.

## 5. Proposed state transition model

```text
Idle
  -> CombatMove        when player is seen
  -> Search            when damaged but player is not visible

Search
  -> CombatMove        when player is reacquired
  -> Idle              when search timer expires
  -> Dead              when HP reaches 0

CombatMove
  -> Shoot             when LOS is clear, target is within ideal/fire range, and cooldown allows
  -> Search            when target is lost beyond grace time
  -> CombatMove        while approaching, retreating, strafing, or moving to cover
  -> Dead              when HP reaches 0

Shoot
  -> CombatMove        when too close, too far, LOS blocked, hit reaction, stay timer exceeded, or low HP retreat triggers
  -> Search            when target is lost
  -> Dead              when HP reaches 0

Dead
  -> removable cleanup after death visual finishes
```

## 6. Minimal implementation steps for a more FPS-like fight

1. Add damage stimulus memory and immediate hostility/search transitions.
2. Gate attacks through `CanShootTarget` so world LOS blocks shooting.
3. Force `Shoot` to exit when the target is closer than ideal range, letting `CombatMove` backpedal/strafe.
4. Keep `CombatMove` as the spacing state: approach if too far, retreat if too close, strafe when in band.
5. Add per-enemy ImGui telemetry to tune distance bands and LOS behavior.

## 7. Roadmap for pathfinding, cover, and weapon reactions

### Step A: Current-code friendly pathing
- Continue using `EnemyAStarNavigator` plus stuck recovery.
- Add stage-authored waypoint nodes only for problematic rooms/corridors.
- Cache node-to-node visibility and simple neighbor links.

### Step B: Cover MVP
- Start with sampled cover positions around the enemy/player using existing world AABBs.
- Add authored `CoverPoint`s for reliable designer control.
- Implement peek cycle as: move to cover -> hide -> peek offset -> shoot burst -> hide.

### Step C: Weapon reactions
- Add `PlayerWeaponSnapshot` and update it once per frame from player weapon code.
- Feed only the snapshot into enemy AI; do not let enemies depend directly on player internals.
- Add low-risk reactions first: reload rush, shotgun backpedal, rifle cover preference.

### Step D: Wave roles
- Extend `WaveSpawnEntry` with `EnemyRole role` and `std::string profileId`.
- Use roles to choose trait/combat/nav profiles.
- Add mixed waves: early `Rush + Shooter`, mid `Cover + Shooter`, late `Tank + Support + flanker`.
