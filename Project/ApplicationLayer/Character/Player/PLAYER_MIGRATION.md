# Player Migration Status

## Goal

Replace the current oversized `Player` implementation with the Actor/Component based `PlayerActor` incrementally.
The legacy runtime remains available until the new runtime is verified in DebugScene and GamePlay.

## Responsibility inventory

| Responsibility | Current legacy owner | New target | Status |
|---|---|---|---|
| Raw input snapshot | `BuildInputSnapshot` / `Player` | `PlayerInputComponent` | Shared snapshot path added |
| Tutorial input restrictions | `Player` | `PlayerInputComponent` | Compatibility path added |
| Movement | `PlayerMotorComponent` / `Player` | `PlayerMovementComponent` | Stabilization in progress |
| Camera / look | `PlayerViewComponent` | `PlayerCameraComponent` | Needs parity work |
| Health | `PlayerDamageComponent` | `CharacterHealthComponent` | Needs parity work |
| Damage feedback | `Player` / `PlayerVfx` / HUD | Player damage presentation boundary | Not started |
| Death | `PlayerDeathComponent` | `PlayerActor::OnDeath` plus dedicated presentation | Needs parity work |
| Weapon logic | `PlayerWeaponComponent` | `WeaponComponent` | Needs parity work |
| Weapon presentation | `PlayerWeaponVisualComponent` | weapon view / presentation component | Needs parity work |
| Combat coordination | `PlayerCombatComponent` / FSM API | dedicated combat components | Not started |
| Melee | `PlayerMeleeComponent` | dedicated melee component | Not started |
| HUD state | `Player` / `HUDManager` | runtime state interface / UI controller | In progress |
| Physics body | `Player` owned Rigidbody/Collider | `RigidbodyComponent` / `CharacterColliderComponent` | Collider layout normalized |
| Ladder | `Player` / `PlayerMotorComponent` | movement / interaction component | Not started |
| Shadow / visual body | `BaseCharacter` compatibility path | `HumanoidVisualComponent` | Partially migrated |
| Runtime ownership | `CharacterWorld::unique_ptr<Player>` | `ActorWorld` | Final migration phase |

## Migration order

1. Player responsibility inventory
2. Legacy/new component mapping
3. Input stabilization
4. Movement stabilization
5. Camera stabilization
6. Health / Damage / Death stabilization
7. Weapon / Combat stabilization
8. HUD / Presentation cleanup
9. Full DebugScene validation
10. GamePlay integration
11. Real-stage regression testing
12. ActorWorld ownership migration
13. Legacy Player path removal

## Compatibility rules

- Do not delete a legacy path until the replacement passes DebugScene and GamePlay regression checks.
- Do not run legacy and new gameplay implementations simultaneously for the same responsibility.
- Prefer small runtime interfaces over adding new direct dependencies on the concrete `Player` class.
- New input code should consume the same `InputSnapshot` contract as the legacy runtime so key mappings do not diverge during migration.
- Saved validation prefabs must not silently restore obsolete Player physics dimensions after the new runtime has defined a stable body layout.

## Movement stabilization convention

The Actor/Component Character path treats the `Character Root` as the physical body center.
`PlayerMovementComponent` therefore normalizes the new Player collider to the Root center with an AABB half size of `(0.45, 0.90, 0.45)` and the `DynamicActor` physics layer during initialization.
This intentionally overrides stale DebugPlayer prefab collider dimensions such as the previous half height `1.8` and local Y offset `-0.4`.
Camera height and final visual offsets remain a separate P5 responsibility.

## Current step

P1 and P2 are recorded in this table.
P3 has a shared `InputSnapshot` path for the legacy and Actor/Component Player validation routes.
P4 is in progress: the new Player collider layout is now normalized before simulation, and the Movement debug panel exposes Root Y, Collider center, bottom, and center offset so remaining grounding errors can be measured instead of guessed.
