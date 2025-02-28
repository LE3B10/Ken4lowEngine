#pragma once
#include <BaseCharacter.h>
#include <numbers>


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
	
private: /// ---------- 定数 ---------- ///
	
	// 移動速度
	float moveSpeed_ = 0.3f;

	// 浮遊ギミックの媒介変数
	float floatingParameter_ = 0.0f;

	// 浮遊移動のサイクル<frame>
	const uint16_t kFloatingCycle = 120;

	// 1フレームあたりの浮遊移動量
	const float kFloatingStep = 2.0f * std::numbers::pi_v<float> / float(kFloatingCycle);

	// 腕のアニメーション用の媒介変数（フレームごとに変化）
	float armSwingParameter_ = 0.0f;

	// 腕の振りの最大角度（ラジアン）
	const float kMaxArmSwingAngle = 0.3f;

	// 腕の振りの速度（待機時の揺れ速度）
	const float kArmSwingSpeedIdle = 0.2f;

	// 移動時の腕振りの速度係数
	const float kArmSwingSpeedMove = 8.0f;
};

