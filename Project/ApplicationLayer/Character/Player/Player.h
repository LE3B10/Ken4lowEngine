#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include "ContactRecord.h"
#include "FpsCamera.h"
#include "PlayerHurtbox.h"

#include "PlayerStateMachines.h"   // PlayerBrain / PlayerAPI / LocoId等
#include "PlayerInputSnapshot.h"   // InputSnapshot型

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Weapon master data -> runtime weapon system
#if __has_include("WeaponMasterData/WeaponSystem.h")
#include "WeaponMasterData/WeaponSystem.h"
#elif __has_include("WeaponSystem.h")
#include "WeaponSystem.h"
#else
// プロジェクト側の include パスに合わせて修正してください
#include "WeaponSystem.h"
#endif
// WeaponCategory enum
#if __has_include("WeaponMasterData/WeaponMasterData.h")
#include "WeaponMasterData/WeaponMasterData.h"
#elif __has_include("WeaponMasterData.h")
#include "WeaponMasterData.h"
#endif


namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }
class CollisionManager;
class BulletManager;
class Enemy;

struct HurtboxTuning
{
	K4E::Vector3 localOffset{ 0,0,0 };   // 部位ローカルでの中心オフセット
	K4E::Vector3 halfSize{ 0.2f,0.2f,0.2f };
	K4E::Vector3 rotOffset{ 0,0,0 };     // 必要なら（基本0でOK）
	float damageMul = 1.0f;
	bool enabled = true;
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

	// 衝突判定を行う
	void OnCollision(K4E::Collider* other) override;

	// ワールド変換の取得
	K4E::WorldTransformEx* GetWorldTransform() { return &body_.transform; }

	// 一人称視点の有効/無効を設定
	void SetFirstPersonView(bool enabled);

	void SetCollisionManager(CollisionManager* mgr) { collisionManager_ = mgr; }

	// 敵の弾に当たったときの処理
	void OnHitByEnemyBullet(K4E::Collider* bullet, PlayerHitPart part, float mul);

	void SetBulletManager(BulletManager* mgr) { bulletManager_ = mgr; }
	void SetShootCamera(K4E::Camera* cam) { shootCamera_ = cam; }

	// WeaponMasterData の読み込みディレクトリを外部から指定したい場合
	// 例: "Resources/JSON/weapons" (primary/backup/... のカテゴリフォルダがあるroot)
	void SetWeaponMasterDirectory(const std::filesystem::path& dir)
	{
		weaponMasterDir_ = dir;
		weaponLoaded_ = false;
		weaponLoadError_.clear();
		weaponIdList_.clear();
		currentWeaponId_ = 0;
		weaponSys_ = WeaponSystem{};
	}

	void SetDebugCamera(bool on) { isDebugCamera_ = on; }

	// WeaponSystemへのアクセス
	void EquipWeaponById(int32_t weaponID) { (void)EquipWeaponByID(weaponID); }

public:	// ---- FSMから呼ばれる最小API（PlayerAPIがここを呼ぶ）----

	bool  FSM_IsGrounded() const;
	float FSM_VerticalVelocity() const;

	void  FSM_SetMoveInput(float x, float z);
	void  FSM_SetSprint(bool on);
	void  FSM_Jump();
	void  FSM_StartDash();
	bool  FSM_IsDashFinished() const;

	// combat（今はスタブでOK）
	bool  FSM_CanFire() const;
	void  FSM_FireOnce();
	bool  FSM_IsReloadFinished() const;
	void  FSM_StartReload();
	bool  FSM_IsMeleeFinished() const;
	void  FSM_StartMelee();
	void  FSM_SetAiming(bool on);
	void  FSM_SetStunned(bool on);

private: /// ---------- メンバ関数 ---------- ///

	void SimulateLocomotion(float dt);

	void ApplyFirstPersonRenderFlags();

	// ---- WeaponMasterData / WeaponSystem ----
	bool LoadWeaponMasterDataOnce();      // WeaponSystem.Load + Equip
	void SwitchWeaponByDelta(int delta);  // weaponIdList_から切替
	void SwitchWeaponCategory(EWeaponCategory category); // 数字キーでカテゴリ切替
	void TickWeapon(float dt);            // WeaponSystem.Tick
	bool EquipWeaponByID(int32_t weaponID);

	void SyncHurtboxes();

private: /// ----------メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // 入力クラス
	K4E::FpsCamera fpsCamera_;        // カメラクラス

	CollisionManager* collisionManager_ = nullptr; // 衝突管理クラス

	BulletManager* bulletManager_ = nullptr;
	K4E::Camera* shootCamera_ = nullptr;

	// ---- Weapon system ----
	std::filesystem::path weaponMasterDir_ = "Resources/JSON/weapons"; // primary/backup/... がある root
	WeaponSystem weaponSys_{};
	bool weaponLoaded_ = false;
	std::string weaponLoadError_;
	EWeaponCategory weaponCategory_ = EWeaponCategory::Primary; // 現在扱うカテゴリ（まずはPrimary）
	std::vector<int32_t> weaponIdList_;
	int32_t currentWeaponId_ = 0;
	std::array<int32_t, 6> lastWeaponIdByCategory_{}; // category index -> last equipped id

	// FSM
	PlayerAPI api_{};
	PlayerBrain brain_{};
	InputSnapshot inputSnap_{};

	// 移動状態（最小）
	float moveX_ = 0.0f;
	float moveZ_ = 0.0f;
	bool  sprint_ = false;

	float groundY_ = 0.0f;
	float verticalVel_ = 0.0f;

	float dashTimer_ = 0.0f;
	float dashDuration_ = 0.18f;
	float dashDirX_ = 0.0f;
	float dashDirZ_ = 1.0f;

	// パラメータ（あとでImGuiで調整しやすい）
	float walkSpeed_ = 3.5f;
	float runSpeed_ = 6.0f;
	float dashSpeed_ = 10.0f;
	float jumpSpeed_ = 8.0f;
	float gravity_ = 19.6f;
	float airControl_ = 0.7f;

	// 一人称視点フラグ
	bool isFirstPersonView_ = false;

	K4E::ContactRecord contactRecord_; // 接触記録
	std::string skinTexturePath_ = "steve.png"; // スキンテクスチャパス

	// 体力
	float maxHp_ = 100.0f;
	float hp_ = 100.0f;

	float hitscanRange_ = 100.0f; // デバッグ用レイ
	float shotDebugTimer_ = 0.0f;

	std::array<std::unique_ptr<PlayerHurtbox>, 6> hurtboxes_{};
	std::array<HurtboxTuning, 6> hbTuning_{};
	int hbSelected_ = 0;
	bool hbDebugDraw_ = true;

	bool isDebugCamera_ = false;
};

