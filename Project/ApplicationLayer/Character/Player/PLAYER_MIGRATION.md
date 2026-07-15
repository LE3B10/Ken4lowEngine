# Player Migration Status

## Goal

Replace the current oversized `Player` implementation with the Actor/Component based `PlayerActor` incrementally.
The legacy runtime remains available until the new runtime is verified in DebugScene and GamePlay.

## Responsibility inventory

| Responsibility | Current legacy owner | New target | Status |
|---|---|---|---|
| Raw input snapshot | `BuildInputSnapshot` / `Player` | `PlayerInputComponent` | In progress |
| Tutorial input restrictions | `Player` | `PlayerInputComponent` | Compatibility path added |
| Movement | `PlayerMotorComponent` / `Player` | `PlayerMovementComponent` | Needs parity work |
| Camera / look | `PlayerViewComponent` | `PlayerCameraComponent` | Needs parity work |
| Health | `PlayerDamageComponent` | `CharacterHealthComponent` | Needs parity work |
| Damage feedback | `Player` / `PlayerVfx` / HUD | Player damage presentation boundary | Not started |
| Death | `PlayerDeathComponent` | `PlayerActor::OnDeath` plus dedicated presentation | Needs parity work |
| Weapon logic | `PlayerWeaponComponent` | `WeaponComponent` | Needs parity work |
| Weapon presentation | `PlayerWeaponVisualComponent` | weapon view / presentation component | Needs parity work |
| Combat coordination | `PlayerCombatComponent` / FSM API | dedicated combat components | Not started |
| Melee | `PlayerMeleeComponent` | dedicated melee component | Not started |
| HUD state | `Player` / `HUDManager` | runtime state interface / UI controller | In progress |
| Physics body | `Player` owned Rigidbody/Collider | `RigidbodyComponent` / `CharacterColliderComponent` | Needs parity work |
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

## Current step

P1 and P2 are recorded in this table.
P3 is in progress: `PlayerInputComponent` can now consume the shared `InputSnapshot`, preserve held/action state needed by later components, and apply tutorial-style input restrictions while DebugScene uses the same snapshot builder as the legacy Player.
