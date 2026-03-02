#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include "ContactRecord.h"
#include "PlayerHurtbox.h"

#include "PlayerStateMachines.h"   // PlayerBrain / PlayerAPI / LocoId等
#include "PlayerInputSnapshot.h"   // InputSnapshot型
#include "PlayerVfx.h"
#include "PlayerWeaponComponent.h"
#include "PlayerMotorComponent.h"
#include "PlayerViewComponent.h"
#include "PlayerWeaponVisualComponent.h"
#include "WeaponSlot.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }
class CollisionManager;
class BulletManager;
class Enemy;
class HUDManager;

struct HurtboxTuning
{
	K4E::Vector3 localOffset{ 0,0,0 };   // 部位ローカルでの中心オフセット
	K4E::Vector3 halfSize{ 0.2f,0.2f,0.2f };
	K4E::Vector3 rotOffset{ 0,0,0 };     // 必要なら（基本0でOK）
	float damageMul = 1.0f;
	bool enabled = true;
};

struct FallDamageSettings
{
	bool  enabled = true;
	float startY = -50.0f;          // ここを -n にする
	float damagePerSecond = 20.0f;  // 1秒あたりダメージ
};

/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~Player() = default;

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	void DrawShadow();

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	// 衝突判定を行う
	void OnCollision(K4E::Collider* other) override;

	// ワールド変換の取得
	K4E::WorldTransformEx* GetWorldTransform() { return &body_.transform; }

	// 一人称視点の有効/無効を設定
	void SetFirstPersonView(bool enabled) { view_.SetFirstPersonView(enabled); }

	// 衝突管理クラスのセット
	void SetCollisionManager(CollisionManager* mgr) { collisionManager_ = mgr; }

	// 敵の弾に当たったときの処理
	void OnHitByEnemyBullet(K4E::Collider* bullet, PlayerHitPart part, float mul);

	// BulletManager と ShootCamera のセット
	void SetBulletManager(BulletManager* mgr) { bulletManager_ = mgr; }
	void SetShootCamera(K4E::Camera* cam) { view_.SetShootCamera(cam); }

	// HUDとの連携（クロスヘア/HP演出の通知先）
	void SetHUDManager(HUDManager* hud) { hudManager_ = hud; }

	// WeaponMasterData の読み込みディレクトリを外部から指定したい場合
	// 例: "Resources/JSON/weapons" (primary/backup/... のカテゴリフォルダがあるroot)
	void SetWeaponMasterDirectory(const std::filesystem::path& dir) { weapon_.SetMasterDirectory(dir); }

	// デバッグカメラのオンオフ（オンのときはFPSカメラを直接操作して移動する）
	void SetDebugCamera(bool on) { isDebugCamera_ = on; }

	// ---- HUD用：リロード円などに使える ----
	// WeaponSystemの実装差があるので「isReloading」「reloadTimer」「reloadSec」をそのまま返す。
	bool GetReloadUI(bool& outIsReloading, float& outReloadTimer, float& outReloadSec) { return weapon_.GetReloadUI(outIsReloading, outReloadTimer, outReloadSec); }

	// WeaponSystemへのアクセス
	void EquipWeaponById(int32_t weaponID) { weapon_.EquipWeaponById(weaponID); }

	PlayerWeaponComponent& GetWeaponComponent() { return weapon_; }

	// HPの取得
	float GetHP() const { return hp_; }
	float GetMaxHP() const { return maxHp_; }

	bool GetReticleUI(FWeaponReticleData& outReticle, float& outSpread, bool& outIsADS) const { return weapon_.GetReticleUI(outReticle, outSpread, outIsADS); }

	// HUD用：敵への命中/撃破演出通知（Enemy/Bullet側から呼ぶ）
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

public:	// ---- FSMから呼ばれる最小API（PlayerAPIがここを呼ぶ）----

	bool FSM_IsGrounded() const { return motor_.IsGrounded(); }
	bool FSM_IsSprinting() const { return motor_.IsSprinting(); }
	float FSM_VerticalVelocity() const { return motor_.VerticalVelocity(); }
	void FSM_SetMoveInput(float x, float z) { motor_.SetMoveInput(x, z); }
	void FSM_SetSprint(bool on) { motor_.SetSprint(on); }
	void FSM_Jump() { motor_.Jump(); }
	void FSM_StartDash() { motor_.StartDash(view_.GetYaw(), /*isAds*/ inputSnap_.aimHeld); }
	bool FSM_CanStartDash() const { return motor_.CanStartDash(); }
	bool FSM_IsDashFinished() const { return motor_.IsDashFinished(); }

	// combat（今はスタブでOK）
	bool FSM_CanFire() const { return weapon_.CanFire(inputSnap_); }
	void FSM_FireOnce();

	bool FSM_IsReloadFinished() const { return weapon_.IsReloadFinished(); }
	void FSM_StartReload() { weapon_.StartReload(); }
	bool FSM_IsMeleeFinished() const;
	void FSM_StartMelee();
	void FSM_SetAiming(bool on) { view_.SetAiming(on); }
	void FSM_SetStunned(bool on);

private: /// ---------- メンバ関数 ---------- ///

	// リロードキャンセル
	void TryCancelReloadInternal();

	void SyncHurtboxes();

	// ---- ダメージ多段防止（弾IDのTTL管理） ----
	void TickRecentBulletHits(float dt);
	bool IsRecentBulletHit(uint32_t id) const { return recentBulletHits_.find(id) != recentBulletHits_.end(); }
	void MarkRecentBulletHit(uint32_t id);

	void ApplyFallDamage(float deltaTime);

private: /// ----------メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // 入力クラス
	CollisionManager* collisionManager_ = nullptr; // 衝突管理クラス
	BulletManager* bulletManager_ = nullptr;
	HUDManager* hudManager_ = nullptr; // HUDへの通知先（任意）

	// 視点・一人称視点制御
	PlayerViewComponent view_{};

	// ---- Weapon system ----
	PlayerWeaponComponent weapon_{};

	// 武器の見た目（PlayerViewComponent から武器のワールド変換をもらって右手に同期させる）
	PlayerWeaponVisualComponent weaponVisual_{};

	// FSM
	PlayerAPI api_{};
	PlayerBrain brain_{};
	InputSnapshot inputSnap_{};
	LocoId prevLocoId_ = LocoId::Idle;

	bool runCarry_ = false;

	PlayerMotorComponent motor_;

	K4E::ContactRecord contactRecord_; // 接触記録
	std::string skinTexturePath_ = "steve.png"; // スキンテクスチャパス

	// 体力
	float maxHp_ = 100.0f;
	float hp_ = 100.0f;

	// ---- 演出（VFX） ----
	PlayerVfx vfx_{};

	float hitscanRange_ = 100.0f; // デバッグ用レイ
	float shotDebugTimer_ = 0.0f;

	// --- カメラリコイル ---
	float recoilPitchDegHip_ = 1.35f;	  // 腰撃ちのカメラピッチ反動
	float recoilYawDegHip_ = 0.85f;   // 腰撃ちのカメラヨー反動
	float recoilPitchDegAds_ = 0.75f; // ADSのカメラピッチ反動
	float recoilYawDegAds_ = 0.5f;	  // ADSのカメラヨー反動

	std::array<std::unique_ptr<PlayerHurtbox>, 6> hurtboxes_{};
	std::array<HurtboxTuning, 6> hbTuning_{};
	int hbSelected_ = 0;
	bool hbDebugDraw_ = true;

	bool isDebugCamera_ = false;

	// 弾IDの最近ヒット管理（TTLで掃除）
	std::unordered_map<uint32_t, float> recentBulletHits_;
	float recentBulletHitTTL_ = 0.25f;

	// ---- Spawn tuning ----
	K4E::Vector3 spawnPos_{ 0,0,0 };
	bool hasSpawnPos_ = false;
	K4E::Vector3 spawnOffset_{ 0,0,0 };

	FallDamageSettings fallDamageSettings_{};

};

