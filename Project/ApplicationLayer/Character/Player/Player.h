#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include "ContactRecord.h"
#include "FpsCamera.h"

#include "PlayerStateMachines.h"   // PlayerBrain / PlayerAPI / LocoId等
#include "PlayerInputSnapshot.h"   // InputSnapshot型

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }
class CollisionManager;
class BulletManager;
class Enemy;

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

	void SetBulletManager(BulletManager* mgr) { bulletManager_ = mgr; }
	void SetShootCamera(K4E::Camera* cam) { shootCamera_ = cam; }

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

	// ★1発撃つ（入力判定は外でやる）
	void FireOnce();

private: /// ----------メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // 入力クラス
	K4E::FpsCamera fpsCamera_;        // カメラクラス

	CollisionManager* collisionManager_ = nullptr; // 衝突管理クラス

	BulletManager* bulletManager_ = nullptr;
	K4E::Camera* shootCamera_ = nullptr;

	float bulletSpeed_ = 80.0f;   // units/sec（好きに調整）
	int   bulletDamage_ = 1;
	float muzzleForwardOffset_ = 0.25f; // カメラ前に少し出す（自分に当たらないため）

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

	float hitscanRange_ = 100.0f;
	float shotDebugTimer_ = 0.0f;
};

