# Player Migration Status

## Goal

Replace the oversized legacy `Player` incrementally with the Actor/Component based `PlayerActor`.
The legacy runtime remains available until the new runtime passes DebugScene and GamePlay regression checks.

## Responsibility mapping

| Responsibility | Legacy owner | New owner | Current state |
|---|---|---|---|
| Raw input snapshot | `BuildInputSnapshot` / `Player` | `PlayerInputComponent` | Shared snapshot path established |
| Tutorial restrictions | `Player` | `PlayerInputComponent` | Compatibility path established |
| Movement | `PlayerMotorComponent` / `Player` | `PlayerMovementComponent` | Walk, Sprint, Jump, Blink and Rigidbody path integrated |
| Physics body | `Player` owned Rigidbody/Collider | `RigidbodyComponent` / `CharacterColliderComponent` | Runtime collider normalized; old prefab values no longer trusted |
| Camera / look | `PlayerViewComponent` | `PlayerCameraComponent` | Look, owner yaw, ADS FOV and Sprint FOV integrated |
| Health | `PlayerDamageComponent` | `CharacterHealthComponent` | Runtime access and reset path integrated |
| Damage feedback | `Player` / `PlayerVfx` / HUD | `PlayerHudPresenterComponent` plus callback boundary | Basic damage notification integrated |
| Death | `PlayerDeathComponent` | `PlayerActor::OnDeath` | Input, movement, weapon and collider shutdown plus game-over readiness integrated |
| Weapon logic | `PlayerWeaponComponent` | `WeaponComponent` | Ammo, reload, semi/auto and fire cooldown integrated |
| Weapon switching | `PlayerWeaponController` / slot logic | `InventoryComponent` | Direct slot and wheel cycle integrated |
| Combat coordination | legacy FSM / combat helpers | `PlayerInputComponent` + movement/camera/weapon request boundaries | Core request routing integrated; melee hit parity remains |
| HUD state | `Player` / `HUDManager` | `PlayerHudPresenterComponent` | HP, ammo, fire mode, crosshair and damage flash synchronization integrated |
| Weapon presentation | `PlayerWeaponVisualComponent` | weapon view model | Basic view model remains; animation parity still pending |
| Melee | `PlayerMeleeComponent` | dedicated new combat path | Input preserved, actual hit parity still pending |
| Ladder | `Player` / `PlayerMotorComponent` | movement / interaction component | Not migrated yet |
| Shadow / body visual | `BaseCharacter` compatibility path | `HumanoidVisualComponent` | Partially migrated |
| Runtime ownership | `CharacterWorld::unique_ptr<Player>` | `ActorWorld` | Final migration phase |

## Migration order

1. Player responsibility inventory - done
2. Legacy/new component mapping - done
3. Input stabilization - baseline done
4. Movement stabilization - baseline done
5. Camera stabilization - baseline done
6. Health / Damage / Death stabilization - baseline done
7. Weapon / Combat stabilization - core ranged path done; melee parity remains
8. HUD / Presentation cleanup - HUD state synchronization moved to presenter component
9. Full DebugScene validation - next
10. GamePlay integration
11. Real-stage regression testing
12. ActorWorld ownership migration
13. Legacy Player path removal

## DebugScene validation controls

- Normal gameplay input uses the same `BuildInputSnapshot` path as the legacy Player.
- `F6`: apply 25 damage to the new PlayerActor.
- `F7`: heal 25 HP.
- `F8`: reset the new PlayerActor at its current position.

## Compatibility rules

- Do not delete a legacy path until the replacement passes DebugScene and GamePlay regression checks.
- Do not run legacy and new gameplay implementations simultaneously for the same responsibility.
- Prefer small runtime interfaces over new direct dependencies on the concrete legacy `Player` class.
- Keep old runtime behavior available until new PlayerActor parity is verified.
- Old prefab values are migration input, not runtime truth; unstable collider settings are normalized by the new movement path.

## Immediate next step

Run the complete DebugScene validation pass for movement, jump, grounded state, blink, camera, ADS, sprint FOV, damage, death, reset, reload, semi/auto fire and weapon switching. Fix failures in one stabilization batch before moving the new PlayerActor into GamePlay.
