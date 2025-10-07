#pragma once
#include <PlayerController.h>
#include <Bullet.h>
#include "Weapon.h"
#include <NumberSpriteDrawer.h>
#include <Collider.h>
#include <AnimationModel.h>

#include "PlayerBehavior.h"

/// ---------- 前方宣言 ---------- ///
class FpsCamera;
class Crosshair;
class CollisionManager;


/// ---------- モデルの状態を表す列挙型 ---------- ///
enum class ModelState
{
	Idle,       // アイドル状態
	Walking,    // 歩行状態
	Running,    // 走行状態
	Jumping,    // ジャンプ状態
	Crouching,  // しゃがみ状態
	Shooting,   // 射撃状態
	Aiming,     // エイムダウンサイト状態
	Reloading,  // リロード状態
	Dead        // 死亡状態
};

/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public Collider
{
	// 各部位ごとの Collider を保持
	struct PartCol
	{
		std::string name;
		std::unique_ptr<Collider> col;
	};
	std::vector<PartCol> bodyCols_;

public: /// ---------- メンバ関数 ---------- ///

	~Player();

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

	// 衝突判定を行う
	void RegisterColliders(CollisionManager* collisionManager) const;

private: /// ---------- メンバ関数 ---------- ///

	// 武器の初期化処理
	void InitializeWeapons();

	// 衝突判定と応答
	void OnCollision(Collider* other) override;

	// 弾丸発射処理位置
	void FireWeapon();

public: /// ---------- ゲッタ ---------- ///

	// 死亡フラグを取得
	bool IsDead() const { return isDead_; }

	// 時間を取得
	float GetDeltaTime() const { return deltaTime; }

	// ダッシュ状態を取得
	bool IsDashing() const { return controller_->IsDashing(); }

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
	bool IsGrounded() const { return controller_->IsGrounded(); }

	// 移動ベクトル取得
	Vector3 GetMoveInput() const { return controller_->GetMoveInput(); }

	// デバッグ用のフラグを取得
	bool IsDebugCamera() const { return controller_->IsDebugCamera(); }

	// しゃがみを取得
	bool IsCrouching() const { return controller_->IsCrouching(); }

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

	// アニメーションモデルを取得
	AnimationModel* GetAnimationModel() { return animationModel_.get(); }

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
	void SetAiming(bool isAiming) { controller_->SetAimingInput(isAiming); }

	// Aimng状態を取得
	bool IsAiming() const { return controller_->IsAimingInput(); }

	// デバッグカメラフラグを設定
	void SetDebugCamera(bool isDebugCamera) { controller_->SetDebugCamera(isDebugCamera); }

	// しゃがみを設定
	void SetCrouching(bool isCrouching) { controller_->SetCrouchInput(isCrouching); }

	// クロスヘアを設定
	void SetCrosshair(Crosshair* crosshair) { crosshair_ = crosshair; }

	// モデルの状態を設定
	void SetState(ModelState state, bool force = false);

	void SetAnimationModel(std::shared_ptr<AnimationModel> model);

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力クラス
	Camera* camera_ = nullptr; // カメラクラス
	FpsCamera* fpsCamera_ = nullptr;
	Crosshair* crosshair_ = nullptr; // クロスヘアクラス

	ModelState currentState_ = ModelState::Idle; // 現在のプレイヤー状態
	std::unordered_map<ModelState, std::unique_ptr<PlayerBehavior>> behaviors_;

	std::vector<std::unique_ptr<Weapon>> weapons_; // 武器クラス
	int currentWeaponIndex_ = 0; // 現在の武器インデックス

	std::unique_ptr<PlayerController> controller_; // プレイヤーコントローラー

	std::vector<std::unique_ptr<Bullet>> bullets_; // 弾丸クラス

	std::unique_ptr<NumberSpriteDrawer> numberSpriteDrawer_; // 数字スプライト描画クラス
	std::shared_ptr<AnimationModel> animationModel_;

	float stamina_ = 1000.0f;        // 現在のスタミナ
	float maxStamina_ = 1000.0f;     // 最大スタミナ

private: /// ---------- プレイヤーの状態 ---------- ///

	float deltaTime = 1.0f / 60.0f; // フレーム間時間（例: 1/60秒）
	float hp_ = 1000.0f; // プレイヤーのHP
	float maxHP_ = 1000.0f; // プレイヤーの最大HP
	bool isDead_ = false; // 死亡フラグ

private: /// ---------- ヒットエフェクト ---------- ///

	float hitEffectTimer_ = 0.0f; // ヒットエフェクトのタイマー
	bool isHitEffectActive_ = false; // ヒットエフェクトがアクティブかどうか

private: /// ---------- デバッグカメラフラグ ---------- ///

	bool isDebugCamera_ = false; // デバッグカメラフラグ
};

