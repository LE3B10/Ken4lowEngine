# Phase 1 - Cleanup and Measurement Audit

Baseline: `master` commit `ee7702d8ad04d74c56c8e3c30557b40fd1e532bd`

Working branch: `feature/phase1-cleanup-profiling`

## Goal

Phase 1 does not immediately delete large groups of files. It first classifies the codebase, records legacy candidates, and adds runtime measurements so later cleanup can be validated against build stability and memory/allocation changes.

## 1. FPS / old-game residue audit

### Confirmed stale project references

- `Engine/Graphics/Camera/FPS/FpsCamera.cpp`
- `Engine/Graphics/Camera/FPS/FpsCamera.h`
- multiple removed Player/Boss/Enemy implementation paths listed in `Directory.Build.targets`
- removed paths are still present in the Visual Studio project/include-directory configuration

`FpsCamera` itself is no longer found by code search; only project/build references remain. These are safe cleanup candidates after the `.vcxproj` / `.filters` entries and include paths are regenerated or edited together.

### Gameplay code that is still real code, not simple residue

The accessible baseline still contains substantial project/gameplay code under `ApplicationLayer`, including:

- `ApplicationLayer/Character/Boss`
- `ApplicationLayer/Character/Enemy`
- `ApplicationLayer/Character/Player`
- `ApplicationLayer/Character/Bullet`
- `ApplicationLayer/Character/CharacterWorld`
- `ApplicationLayer/Scene/GamePlayScene`
- game-specific UI such as Crosshair / WeaponSlot / Reload / NoAmmo

Some of these files are still explicitly compiled by `Ken4lowEngine.vcxproj`. If the target repository is becoming an engine-only repository, they should be moved to a sample/game module or removed only after their engine dependencies have been replaced.

## 2. Engine / Gameplay / Editor classification

### Engine Runtime

Keep as reusable runtime infrastructure:

- `Engine/Core`
- `Engine/Math`
- `Engine/Platform`
- `Engine/Graphics`
- `Engine/Physics`
- `Engine/System/Input`
- `Engine/System/Audio`
- generic `Engine/Scene/Actor` infrastructure
- generic `Engine/Scene/Level` serialization/loading infrastructure

### Gameplay Framework candidates

These are useful abstractions, but they should not live in the lowest-level engine module:

- `Engine/Scene/Actor/Character/CharacterActor`
- `CharacterMovementComponent`
- generic Pawn/Character-style movement primitives

Recommended future destination: `Engine/Gameplay` or a separate `Ken4low.Gameplay` module.

### Project/game-specific code currently mixed into Engine

Review for migration out of Engine Runtime:

- `CharacterHealthComponent`
- `AttackComponent` / `AttackBehaviors`
- `CharacterTargetComponent`
- `HumanoidVisualComponent`
- stage-specific layout/import helpers whose names encode a particular game stage

These features may remain available as optional gameplay modules, but Core/World/Render should not depend on them.

### Editor

Editor-only responsibilities should converge under `Engine/Editor`:

- World Outliner / Details
- Content Browser
- GPU Picking
- Transform Gizmo
- Editor selection state
- Level save/open/autosave UI
- PIE controls

Runtime classes currently containing `DrawImGui`, prefab browser UI, legacy editor windows, or editor selection state are migration candidates. `USE_IMGUI` prevents Release cost, but responsibility is still mixed at source level.

### Application / Sample Project

`ApplicationLayer` should eventually be treated as a project or sample game that consumes the engine rather than as part of the reusable engine itself.

## 3. Legacy inventory

### High-confidence cleanup candidates

1. stale `FpsCamera` project/include references
2. removed Player/Boss/Enemy paths listed only as `Remove=` workarounds in `Directory.Build.targets`
3. stale AdditionalIncludeDirectories entries for deleted folders
4. `ModelManager::FindModel()` if no external caller appears; current implementation is only an alias of `LoadModel()`
5. hand-written `ModelManager::LoadObjFile` parser if Assimp/KMesh paths are the only production import routes
6. `ActorWorld::DrawScreenSpaceSprites()` after callers are migrated to `DrawScreenSpaceUI()`
7. old Actor World / Actor Details compatibility UI after the new Outliner/Details path is fully validated

### Must be migrated before deletion

1. `Engine/Physics/Collision/Legacy`
2. old audio decoder location under `Engine/Misc/Audio` versus the newer `Engine/System/Audio` ownership
3. game-specific Character components currently registered by the engine `ComponentFactory`
4. stage/game-specific Level helpers still referenced by runtime scenes
5. old Visual Studio project/filter entries that are currently masked by `Directory.Build.targets`

## 4. Asset memory measurement introduced in this phase

The profiler will track the major persistent asset payloads first:

- loaded Texture count and estimated texture GPU payload
- loaded Model count and estimated Model CPU/GPU payload
- live decoded Audio clip count and PCM bytes

This is intentionally an estimate, not a full D3D12 residency report. Resource heap alignment, driver residency, transient upload buffers, render targets, particle buffers, and material metadata should be added in later profiling phases.

## 5. Per-frame allocation measurement introduced in this phase

`FrameAllocationTracker` uses the MSVC Debug CRT allocation hook and records, for the last completed frame:

- allocation/reallocation event count
- requested allocation bytes
- peak allocation count per frame
- peak requested bytes per frame

The tracker is Debug-only by design. Release builds return `supported = false` and do not install the CRT hook.

This measurement is process-wide while a frame is active, so allocations from the Editor and worker threads are included. That is useful for detecting real frame churn, but later profiling can add subsystem scopes when attribution is needed.

## Next cleanup pass

After the profiler is visible and the branch builds:

1. record an idle Editor baseline for allocation count/bytes
2. record an empty Level baseline for asset memory
3. load representative assets and verify counters increase/decrease as expected
4. clean stale `.vcxproj` / `.filters` / include-directory entries
5. remove or move one legacy group at a time
6. compare allocations, memory, startup time, and build results after every cleanup group
