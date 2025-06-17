#pragma once
#include <PlayerController.h>
#include <Bullet.h>
#include "Weapon.h" // 追加
#include <NumberSpriteDrawer.h>
#include <Collider.h>
#include <AnimationModel.h>

/// ---------- 前方宣言 ---------- ///
class FpsCamera;
class Crosshair;


//// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

	// ダメージを受ける
	void TakeDamage(float damage);

private: /// ---------- メンバ関数 ---------- ///

	// 武器の初期化処理
	void InitializeWeapons();

	// 移動処理
	void Move();

	// 衝突判定と応答
	void OnCollision(Collider* other) override;

	// 弾丸発射処理位置
	void FireWeapon();

	// スタミナシステムの更新
	void UpdateStamina();

public: /// ---------- ゲッタ ---------- ///

	// 死亡フラグを取得
	bool IsDead() const { return isDead_; }

	// 時間を取得
	float GetDeltaTime() const { return deltaTime; }

	// ダッシュ状態を取得
	bool IsDashing() const { return isDashing_; }

	// 全ての弾丸を取得
	std::vector<const Bullet*> GetAllBullets() const;

	// プレイヤーの向きを取得
	float GetYaw() const { return animationModel_->GetRotate().y; }

	// 弾丸を取得
	const std::vector<std::unique_ptr<Bullet>>& GetBullets() const { return weapons_[currentWeaponIndex_]->GetBullets(); }

	// ワールド変換の取得
	const WorldTransform* GetWorldTransform() { return &animationModel_->GetWorldTransform(); }

	// 弾薬の取得
	int GetAmmoInClip() const { return weapons_[currentWeaponIndex_]->GetAmmoInClip(); }

	// 武器の取得
	Weapon* GetCurrentWeapon() const { return weapons_[currentWeaponIndex_].get(); }

	// HPの取得
	float GetHP() const { return hp_; }

	// 最大HPの取得
	float GetMaxHP() const { return maxHP_; }

	// 地面にいるかどうかを取得
	bool IsGrounded() const { return isGrounded_; }

	// 移動ベクトル取得
	Vector3 GetMoveInput() const { return controller_->GetMoveInput(); }

	// デバッグ用のフラグを取得
	bool IsDebugCamera() const { return controller_->IsDebugCamera(); }

	// しゃがみを取得
	bool IsCrouching() const { return isCrouching_; }

	// コントローラーを取得
	PlayerController* GetController() const { return controller_.get(); }

	// FPSカメラを取得
	FpsCamera* GetFpsCamera() const { return fpsCamera_; }

	// スタミナを取得
	float GetStamina() const { return stamina_; }

	// 最大スタミナを取得
	float GetMaxStamina() const { return maxStamina_; }

	// クロスヘアを取得
	Crosshair* GetCrosshair() const { return crosshair_; }

public: /// ---------- セッタ ---------- ///

	// 追従カメラを設定
	void SetCamera(Camera* camera) { camera_ = camera; }

	// FPSカメラをセット
	void SetFpsCamera(FpsCamera* fpsCamera) { fpsCamera_ = fpsCamera; }

	// HPを追加
	void AddHP(int amount);

	// 弾薬を追加
	void AddAmmo(int amount) { GetCurrentWeapon()->AddReserveAmmo(amount); }

	// Aiming状態を設定
	void SetAiming(bool isAiming) { isAiming_ = isAiming; }

	// Aimng状態を取得
	bool IsAiming() const { return isAiming_; }

	// デバッグカメラフラグを設定
	void SetDebugCamera(bool isDebugCamera) { controller_->SetDebugCamera(isDebugCamera); }

	// しゃがみを設定
	void SetCrouching(bool isCrouching) { isCrouching_ = isCrouching; }

	// クロスヘアを設定
	void SetCrosshair(Crosshair* crosshair) { crosshair_ = crosshair; }

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力クラス
	Camera* camera_ = nullptr; // カメラクラス
	FpsCamera* fpsCamera_ = nullptr;
	Crosshair* crosshair_ = nullptr; // クロスヘアクラス

	std::vector<std::unique_ptr<Weapon>> weapons_; // 武器クラス
	int currentWeaponIndex_ = 0; // 現在の武器インデックス

	std::unique_ptr<PlayerController> controller_; // プレイヤーコントローラー
	std::vector<std::unique_ptr<Bullet>> bullets_; // 弾丸クラス

	std::unique_ptr<NumberSpriteDrawer> numberSpriteDrawer_; // 数字スプライト描画クラス
	std::unique_ptr<AnimationModel> animationModel_;

private: /// ---------- ジャンプ機能 ---------- ///

	Vector3 velocity_ = {};        // 移動速度（Y成分がジャンプに使われる）
	bool isGrounded_ = true;       // 地面にいるかどうか
	const float gravity_ = -0.98f; // 重力加速度
	const float jumpPower_ = 0.28f; // ジャンプ力

private: /// ---------- ダッシュ機能 ---------- ///

	bool isDashing_ = false;
	float baseSpeed_ = 0.1f;
	float dashMultiplier_ = 2.0f;

private: /// ---------- スタミナシステム ---------- ///

	float stamina_ = 100.0f;        // 現在のスタミナ
	float maxStamina_ = 100.0f;     // 最大スタミナ
	float staminaRegenRate_ = 15.0f; // 1秒あたりの回復量
	float staminaDashCost_ = 20.0f;  // ダッシュ1回分の消費量
	float staminaJumpCost_ = 15.0f;  // ジャンプ1回分の消費量
	bool isStaminaRecoverBlocked_ = false; // 一時的に回復を止める（行動中など）
	float staminaRecoverDelay_ = 1.0f; // 行動後、回復開始までの猶予秒数
	float staminaRecoverTimer_ = 0.0f; // スタミナ回復タイマー

private: /// ---------- プレイヤーの状態 ---------- ///

	float deltaTime = 1.0f / 60.0f; // フレーム間時間（例: 1/60秒）
	float hp_ = 1000.0f; // プレイヤーのHP
	float maxHP_ = 1000.0f; // プレイヤーの最大HP
	bool isDead_ = false; // 死亡フラグ

private: /// ---------- エイミング機能 ---------- ///

	bool isAiming_ = false; // エイミング状態
	float adsSpeedFactor_ = 0.25f;  // ADS時の移動速度倍率（例：50%）

private: /// ---------- しゃがみ機能 ---------- ///

	// しゃがむ機能
	bool isCrouching_ = false; // しゃがみ状態
	float crouchingSpeed_ = 0.25f; // しゃがみ時の移動速度

private: /// ---------- ヒットエフェクト ---------- ///

	float hitEffectTimer_ = 0.0f; // ヒットエフェクトのタイマー
	bool isHitEffectActive_ = false; // ヒットエフェクトがアクティブかどうか

private: /// ---------- デバッグカメラフラグ ---------- ///

	bool isDebugCamera_ = false; // デバッグカメラフラグ
};

