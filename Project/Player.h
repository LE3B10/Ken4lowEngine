#pragma once
#include <BaseCharacter.h>
#include <Hammer.h>

#include <optional>

/// ---------- 振る舞い ---------- ///
enum class Behavior
{
	kRoot,	 // 通常状態
	kAttack, // 攻撃中
};


/// -------------------------------------------------------------
///						　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

public: /// ---------- ゲッタ ---------- ///

	// カメラの取得
	Camera* GetCamera() { return camera_; }

public: /// ---------- セッタ ---------- ///

	// カメラの設定
	void SetCamera(Camera* camera) { camera_ = camera; }

private: /// ---------- メンバ関数 ---------- ///

	// 移動処理
	void Move();

	// 浮遊ギミックの初期化
	void InitializeFloatingGimmick();

	// 浮遊ギミックの更新
	void UpdateFloatingGimmick();

	// 腕のアニメーション
	void UpdateArmAnimation(bool isMoving);

private: /// ---------- ルートビヘイビア用メンバ関数 ---------- ///

	// 通常行動の初期化処理
	void BehaviorRootInitialize();

	// 通常行動の更新処理
	void BehaviorRootUpdate();

	// 攻撃行動の初期化処理
	void BehaviorAttackInitialize();

	// 攻撃行動の更新処理
	void BehaviorAttackUpdate();

private: /// ---------- メンバ変数 ---------- ///

	// 振る舞い
	Behavior behavior_ = Behavior::kRoot; // 現在の行動

	// 次の振る舞いをリクエスト
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	// ハンマー
	std::unique_ptr<Hammer> hammer_;

private: /// ---------- 定数 ---------- ///

	float moveSpeed_ = 0.3f; // 移動速度
	float floatingParameter_ = 0.0f; // 浮遊ギミックの媒介変数
	const uint16_t kFloatingCycle = 120; // 浮遊移動のサイクル<frame>
	const float kFloatingStep = 2.0f * std::numbers::pi_v<float> / float(kFloatingCycle); // 1フレームあたりの浮遊移動量
	float armSwingParameter_ = 0.0f; // 腕のアニメーション用の媒介変数（フレームごとに変化）
	const float kMaxArmSwingAngle = 0.3f; // 腕の振りの最大角度（ラジアン）
	const float kArmSwingSpeedIdle = 0.2f; // 腕の振りの速度（待機時の揺れ速度）
	const float kArmSwingSpeedMove = 8.0f; // 移動時の腕振りの速度係数

	const float maxArmSwingAngle_ = 1.5f; // 腕の最大スイング角度（ラジアン）

	// 攻撃アニメーションの状態
	bool isAttacking_ = false; // ハンマーを描画するかどうかのフラグ
	int attackFrame_ = 0; // 攻撃の進行フレーム
	const int attackTotalFrames_ = 30; // 攻撃にかかるフレーム数
	const int attackSwingFrames_ = 20; // 振りかぶり→振り下ろしのフレーム

	bool isAttackHold_ = false; // 攻撃後に武器の角度を固定するフラグ
	int attackHoldFrame_ = 0; // 固定状態のカウント
	const int attackHoldFrames_ = 20; // 武器を振った後の硬直時間（20フレーム）
};

