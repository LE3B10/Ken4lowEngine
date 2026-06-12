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
#include <PlayerMeleeComponent.h>
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
///
/// GamePlayWorld内のCharacterWorldが所有するプレイヤー本体。
/// 入力、移動、視点、武器、近接、被ダメージ、死亡、HUD連携を各コンポーネントへ委譲し、
/// GamePlayScene/Worldからはこのクラスを通してプレイヤー状態を参照・更新する。
/// 外部参照はBindDependenciesまたは個別Setterで受け取り、所有権は持たない。
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	~Player() = default;

	// 体パーツ、カメラ、武器、FSM、ダメージ、死亡、HUD連携の初期状態を構築する。
	void Initialize() override;
	// 入力取得、武器更新、移動/視点更新、死亡演出、表示同期を1フレーム進める。
	void Update(float deltaTime) override;
	// プレイヤー本体と武器表示を、現在の一人称/三人称状態に合わせて描画する。
	void Draw() override;
	// Player Debug向けに移動・武器・被ダメージなどの調整UIを描画する。
	void DrawImGui() override;
	void DrawPlayerDebugImGui();
	void DrawWeaponDebugImGui();

	// シャドウパス用にプレイヤー本体と武器の影を描画する。
	void DrawShadow();
	// ライト行列をプレイヤー本体/武器表示へ同期する。
	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	// 敵弾、敵本体、アイテムなどCollider経由の接触通知を受け取る。
	void OnCollision(K4E::Collider* other) override;
	void OnCollisionEnter(const K4E::CollisionHit& hit) override;
	void OnOverlapBegin(const K4E::CollisionHit& hit) override;

	K4E::WorldTransformEx* GetWorldTransform() { return &body_.transform; }

	void SetFirstPersonView(bool enabled) { view_.SetFirstPersonView(enabled); }

	// Input/Collision/Bullet/HUD/Camera/SEなど、Playerが参照する外部依存をまとめて受け取る窓口。
	void BindDependencies(const PlayerDependencies& deps);

	// 既存呼び出し互換のため残す。所有権は受け取らず、World側の寿命に従う。
	void SetCollisionManager(CollisionManager* mgr) { refs_.collisionManager = mgr; }
	void SetBulletManager(BulletManager* mgr) { refs_.bulletManager = mgr; }
	void SetShootCamera(K4E::Camera* cam) { view_.SetShootCamera(cam); }
	void SetHUDManager(HUDManager* hud) { refs_.hudManager = hud; }

	// 敵弾ヒット時に、部位倍率を反映してダメージとフィードバックを発生させる。
	void OnHitByEnemyBullet(K4E::Collider* bullet, PlayerHitPart part, float mul);

	void SetWeaponMasterDirectory(const std::filesystem::path& dir) { weapon_.SetMasterDirectory(dir); }
	void SetDebugCamera(bool on) { runtime_.isDebugCamera = on; }

	// 現在武器のリロード状態をHUD用に取得する。戻り値falseなら表示用情報なし。
	bool GetReloadUI(bool& outIsReloading, float& outReloadTimer, float& outReloadSec)
	{
		return weapon_.GetReloadUI(outIsReloading, outReloadTimer, outReloadSec);
	}

	void EquipWeaponById(int32_t weaponID) { weapon_.EquipWeaponById(weaponID); }

	PlayerWeaponComponent& GetWeaponComponent() { return weapon_; }

	float GetHP() const { return damage_.GetHP(); }
	float GetMaxHP() const { return damage_.GetMaxHP(); }
	// 外部から受けたダメージをPlayerDamageComponentへ渡し、必要なら死亡シーケンスを開始する。
	void ApplyDamage(float amount, const K4E::Vector3* attackerPosition = nullptr);
	void Heal(float amount) { damage_.Heal(amount); }
	int AddReserveAmmo(int amount) { return weapon_.AddReserveAmmo(amount); }
	int GetCurrentWeaponMagazineAmmo() const { return weapon_.GetMagazineAmmo(); }
	int GetCurrentWeaponReserveAmmo() const { return weapon_.GetReserveAmmo(); }
	int GetCurrentWeaponMaxReserveAmmo() const { return weapon_.GetMaxReserveAmmo(); }
	bool AddCurrentWeaponAmmo(int amount) { return weapon_.AddCurrentWeaponAmmo(amount); }
	bool CanCurrentWeaponRecoverAmmo() const { return weapon_.CanCurrentWeaponRecoverAmmo(); }
	std::string GetCurrentWeaponName() const { return weapon_.GetCurrentWeaponName(); }

	PlayerBrainComponent& GetBrainComponent() { return brainComponent_; }
	const PlayerBrainComponent& GetBrainComponent() const { return brainComponent_; }

	PlayerWeaponVisualComponent& GetWeaponVisualComponent() { return weaponVisual_; }
	const PlayerWeaponVisualComponent& GetWeaponVisualComponent() const { return weaponVisual_; }

	bool GetReticleUI(FWeaponReticleData& outReticle, float& outSpread, bool& outIsADS) const
	{
		return weapon_.GetReticleUI(outReticle, outSpread, outIsADS);
	}

	// ヒットマーカー表示用に、敵へ命中したことをHUDへ通知する。
	void NotifyEnemyHitUI(bool isHeadshot = false);
	// キル確定マーカー表示用に、敵撃破をHUDへ通知する。
	void NotifyEnemyKillUI(bool isHeadshot = false);

	// WeaponSlotが描画する弾数・武器名・選択状態をHUDスナップショットとして取得する。
	bool GetWeaponSlotHUD(WeaponSlot::HudSnapshot& out) const;

	// ステージスポーン地点を設定し、Initialize後の初期位置補正やリトライ再配置に使う。
	void SetSpawnPosition(const K4E::Vector3& worldPos);
	void SetSpawnOffset(const K4E::Vector3& offset);
	void SetViewLookAngles(float pitchRad, float yawRad);
	void SyncViewToPlayer();

	void SetStageWorldAABBs(const std::vector<K4E::AABB>* aabbs) { motor_.SetWorldAABBs(aabbs); }
	void SetWorldCollisionSettings(const K4E::WorldCollisionSettings& s) { motor_.SetWorldCollisionSettings(s); }

	void SetFallDamageSettings(const FallDamageSettings& s) { fallDamageSettings_ = s; }

	// Stage Editor等で編集された武器マスタを、現在実行中の武器コンポーネントへ即時反映する。
	void ApplyEditedWeaponDataFromEditor(int32_t weaponID, const FWeaponMasterData& data);

	void ForceRefreshWeaponVisual() { weaponVisual_.ForceRefresh(); }
	void StartWeaponEquipAnimation() { weaponVisual_.StartEquipAnimation(); }
	bool IsWeaponEquipAnimating() const { return weaponVisual_.IsEquipAnimating(); }
	// イントロ中に非表示で温めた腕/武器表示を、プレイ開始タイミングで切り替える。
	void SetStartGameplayVisualsVisible(bool visible);
	// イントロ終了直後の生成負荷を避けるため、武器表示を事前更新しておく。
	void WarmupStartGameplayVisuals();

	void SetOnHitSECallback(std::function<void()> cb) { audio_.onHit = std::move(cb); }
	void SetOnFireSECallback(std::function<void()> cb) { audio_.onFire = std::move(cb); }
	void SetOnReloadSECallback(std::function<void()> cb) { audio_.onReload = std::move(cb); }
	void SetOnDeathSECallback(std::function<void()> cb) { audio_.onDeath = std::move(cb); }
	void SetOnDamageTakenCallback(std::function<void()> cb) { onDamageTaken_ = std::move(cb); }

	// GamePlaySceneがゲームオーバーへ移る準備ができたかを死亡演出側から取得する。
	bool IsGameOverReady() const { return death_.IsGameOverReady(); }
	bool ConsumeGameOverReady() { return death_.ConsumeGameOverReady(); }

	bool IsDeathActive() const { return death_.IsActive(); }
	bool IsDeathSequenceFinished() const { return death_.IsFinished(); }

	K4E::Camera* GetCamera() { return view_.GetCamera(); }
	const K4E::Camera* GetCamera() const { return const_cast<Player*>(this)->view_.GetCamera(); }

	// 弾切れUIの表示状態をHUD用に取得する。戻り値falseなら表示用情報なし。
	bool GetNoAmmoUI(bool& outVisible) const;

public:	// ---- FSMから呼ばれる最小API ----

	// 以下はPlayerBrain/FSMがPlayer内部状態を直接触らないための薄い操作窓口。
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
	void StartHitFlash();
	void UpdateHitFlash(float deltaTime);

	bool IsCurrentWeaponMeleeForView() const;

private: /// ----------メンバ変数 ---------- ///

	PlayerExternalRefs refs_{};

	PlayerViewComponent view_{};

	PlayerWeaponComponent weapon_{};
	PlayerWeaponVisualComponent weaponVisual_{};
	PlayerWeaponController weaponController_{};
	PlayerCombatComponent combat_{};
	PlayerMeleeComponent melee_{};

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
	std::function<void()> onDamageTaken_{};

	float hitFlashTimer_ = 0.0f;
	float hitFlashDuration_ = 0.18f;
	float hitFlashIntensity_ = 2.2f;

	PlayerAudioCallbacks audio_{};
};
