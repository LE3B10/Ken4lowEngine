#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include "ContactRecord.h"
#include "PlayerHurtbox.h"
#include "PlayerDeathComponent.h"
#include "PlayerHurtboxComponent.h"

#include "PlayerStateMachines.h"
#include "PlayerFsmApi.h"
#include "PlayerBrainComponent.h"
#include "PlayerInputSnapshot.h"
#include "PlayerVfx.h"
#include "PlayerWeaponComponent.h"
#include "PlayerMotorComponent.h"
#include "PlayerViewComponent.h"
#include "PlayerWeaponVisualComponent.h"
#include "PlayerWeaponController.h"
#include "PlayerCombatComponent.h"
#include "PlayerDamageComponent.h"
#include "WeaponSlot.h"

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine
{
	class Input;
	class Camera;
}
class CollisionManager;
class BulletManager;
class Enemy;
class HUDManager;

struct FallDamageSettings
{
	bool  enabled = true;
	float startY = -50.0f;
	float damagePerSecond = 20.0f;
};

struct PlayerExternalRefs
{
	K4E::Input* input = nullptr;
	CollisionManager* collisionManager = nullptr;
	BulletManager* bulletManager = nullptr;
	HUDManager* hudManager = nullptr;
};

struct PlayerAudioCallbacks
{
	std::function<void()> onHit{};
	std::function<void()> onFire{};
	std::function<void()> onReload{};
	std::function<void()> onDeath{};
};

struct PlayerDependencies
{
	PlayerExternalRefs refs{};
	PlayerAudioCallbacks audio{};
	K4E::Camera* shootCamera = nullptr;
};

struct PlayerRuntimeState
{
	bool runCarry = false;
	bool isDebugCamera = false;
};

/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	~Player() = default;

	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;
	void DrawImGui() override;

	void DrawShadow();
	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	void OnCollision(K4E::Collider* other) override;

	K4E::WorldTransformEx* GetWorldTransform() { return &body_.transform; }

	void SetFirstPersonView(bool enabled) { view_.SetFirstPersonView(enabled); }

	// 依存をまとめて受け取る窓口
	void BindDependencies(const PlayerDependencies& deps);

	// 既存呼び出し互換のため残す
	void SetCollisionManager(CollisionManager* mgr) { refs_.collisionManager = mgr; }
	void SetBulletManager(BulletManager* mgr) { refs_.bulletManager = mgr; }
	void SetShootCamera(K4E::Camera* cam) { view_.SetShootCamera(cam); }
	void SetHUDManager(HUDManager* hud) { refs_.hudManager = hud; }

	void OnHitByEnemyBullet(K4E::Collider* bullet, PlayerHitPart part, float mul);

	void SetWeaponMasterDirectory(const std::filesystem::path& dir) { weapon_.SetMasterDirectory(dir); }
	void SetDebugCamera(bool on) { runtime_.isDebugCamera = on; }

	bool GetReloadUI(bool& outIsReloading, float& outReloadTimer, float& outReloadSec)
	{
		return weapon_.GetReloadUI(outIsReloading, outReloadTimer, outReloadSec);
	}

	void EquipWeaponById(int32_t weaponID) { weapon_.EquipWeaponById(weaponID); }

	PlayerWeaponComponent& GetWeaponComponent() { return weapon_; }

	float GetHP() const { return damage_.GetHP(); }
	float GetMaxHP() const { return damage_.GetMaxHP(); }

	PlayerBrainComponent& GetBrainComponent() { return brainComponent_; }
	const PlayerBrainComponent& GetBrainComponent() const { return brainComponent_; }

	PlayerWeaponVisualComponent& GetWeaponVisualComponent() { return weaponVisual_; }
	const PlayerWeaponVisualComponent& GetWeaponVisualComponent() const { return weaponVisual_; }

	bool GetReticleUI(FWeaponReticleData& outReticle, float& outSpread, bool& outIsADS) const
	{
		return weapon_.GetReticleUI(outReticle, outSpread, outIsADS);
	}

	void NotifyEnemyHitUI(bool isHeadshot = false);
	void NotifyEnemyKillUI(bool isHeadshot = false);

	bool GetWeaponSlotHUD(WeaponSlot::HudSnapshot& out) const;

	void SetSpawnPosition(const K4E::Vector3& worldPos);
	void SetSpawnOffset(const K4E::Vector3& offset);

	void SetStageWorldAABBs(const std::vector<K4E::AABB>* aabbs) { motor_.SetWorldAABBs(aabbs); }
	void SetWorldCollisionSettings(const K4E::WorldCollisionSettings& s) { motor_.SetWorldCollisionSettings(s); }

	void SetFallDamageSettings(const FallDamageSettings& s) { fallDamageSettings_ = s; }

	void ApplyEditedWeaponDataFromEditor(int32_t weaponID, const FWeaponMasterData& data);

	void ForceRefreshWeaponVisual() { weaponVisual_.ForceRefresh(); }

	void SetOnHitSECallback(std::function<void()> cb) { audio_.onHit = std::move(cb); }
	void SetOnFireSECallback(std::function<void()> cb) { audio_.onFire = std::move(cb); }
	void SetOnReloadSECallback(std::function<void()> cb) { audio_.onReload = std::move(cb); }
	void SetOnDeathSECallback(std::function<void()> cb) { audio_.onDeath = std::move(cb); }

	bool IsGameOverReady() const { return death_.IsGameOverReady(); }
	bool ConsumeGameOverReady() { return death_.ConsumeGameOverReady(); }

	bool IsDeathActive() const { return death_.IsActive(); }
	bool IsDeathSequenceFinished() const { return death_.IsFinished(); }

	K4E::Camera* GetCamera() { return view_.GetCamera(); }
	const K4E::Camera* GetCamera() const { return const_cast<Player*>(this)->view_.GetCamera(); }

	bool GetNoAmmoUI(bool& outVisible) const;

public:	// ---- FSMから呼ばれる最小API ----

	bool FSM_IsGrounded() const { return motor_.IsGrounded(); }
	bool FSM_IsSprinting() const { return motor_.IsSprinting(); }
	float FSM_VerticalVelocity() const { return motor_.VerticalVelocity(); }
	void FSM_SetMoveInput(float x, float z) { motor_.SetMoveInput(x, z); }
	void FSM_SetSprint(bool on) { motor_.SetSprint(on); }
	void FSM_Jump() { motor_.Jump(); }
	void FSM_StartBlink() { motor_.StartBlink(view_.GetYaw(), /*isAds*/ inputSnap_.aimHeld); }
	bool FSM_CanStartBlink() const { return motor_.CanStartBlink(); }
	bool FSM_IsBlinkFinished() const { return motor_.IsBlinkFinished(); }

	bool FSM_CanFire() const { return weapon_.CanFire(inputSnap_); }
	void FSM_FireOnce();

	bool FSM_IsReloadFinished() const { return weapon_.IsReloadFinished(); }
	void FSM_StartReload();
	bool FSM_IsMeleeFinished() const;
	void FSM_StartMelee();
	void FSM_SetAiming(bool on) { view_.SetAiming(on); }
	void FSM_SetStunned(bool on);

private: /// ---------- 内部構造体 ---------- ///

	struct ReloadContext
	{
		bool isReloading = false;
		float reloadTimer = 0.0f;
		float reloadSec = 0.0f;
	};

	struct InputFrameContext
	{
		InputSnapshot rawSnap{};
		ReloadContext reload{};
	};

	struct MovementContext
	{
		LocoId prev = LocoId::Idle;
		LocoId cur = LocoId::Idle;
		CombatId combat = CombatId::Hip;

		bool isAds = false;
		bool blinkJustStarted = false;
		bool moving = false;
		bool isAirLike = false;
		bool isRunningForFov = false;
		bool isBlinking = false;
		bool isReloading = false;
	};

private: /// ---------- メンバ関数 ---------- ///

	void SyncHurtboxes();

	void ApplyFallDamage(float deltaTime);

	void UpdateInputAndWeapon(float deltaTime);
	void UpdateBrain(float deltaTime);
	void UpdateMovementAndView(float deltaTime);
	void UpdatePresentation(float deltaTime);

	// 入力・武器更新の細分化
	InputFrameContext BuildInputFrameContext(float deltaTime);
	void UpdateWeaponBeforeMotor(float deltaTime, InputFrameContext& ctx);
	void FinalizeInputSnapshotForGameplay(float deltaTime, InputFrameContext& ctx);
	void ApplyWeaponCameraAndMovementTuning();

	// 移動・視点更新の細分化
	MovementContext BuildMovementContext() const;
	void UpdateRunCarry(const MovementContext& ctx);
	void UpdateHudFromMovement(const MovementContext& ctx);
	void SimulateMovement(const MovementContext& ctx, float deltaTime);
	void SyncViewAfterMovement(const MovementContext& ctx, float deltaTime);

	void StartDeath(const K4E::Vector3& launchDirWorld);
	void UpdateDeath(float deltaTime);
	bool IsDead() const { return death_.IsActive(); }

	void ApplyDamageFeedback(const PlayerDamageComponent::DamageFeedback& fb);

private: /// ----------メンバ変数 ---------- ///

	PlayerExternalRefs refs_{};

	PlayerViewComponent view_{};

	PlayerWeaponComponent weapon_{};
	PlayerWeaponVisualComponent weaponVisual_{};
	PlayerWeaponController weaponController_{};
	PlayerCombatComponent combat_{};

	PlayerAPI api_{};
	PlayerBrainComponent brainComponent_{};
	InputSnapshot inputSnap_{};

	PlayerRuntimeState runtime_{};

	PlayerMotorComponent motor_{};

	K4E::ContactRecord contactRecord_;
	std::string skinTexturePath_ = "Characters/steve.dds";

	PlayerVfx vfx_{};

	PlayerDeathComponent death_{};
	PlayerHurtboxComponent hurtbox_{};
	PlayerDamageComponent damage_{};

	K4E::Vector3 spawnPos_{ 0,0,0 };
	bool hasSpawnPos_ = false;
	K4E::Vector3 spawnOffset_{ 0,0,0 };

	FallDamageSettings fallDamageSettings_{};

	PlayerAudioCallbacks audio_{};
};