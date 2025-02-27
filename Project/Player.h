#pragma once
#include <Object3D.h>
#include <WorldTransform.h>

#include <vector>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class Camera;
class Input;


/// ---------- 部位データ構造体 ---------- ///
struct BodyPart
{
	std::unique_ptr<Object3D> object;
	WorldTransform transform;
};


/// -------------------------------------------------------------
///					プレイヤークラス
/// -------------------------------------------------------------
class Player
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- ゲッタ ---------- ///

	// カメラの取得
	Camera* GetCamera() { return camera_; }

	// プレイヤーのワールド変換の取得
	const WorldTransform* GetWorldTransform() { return &body_.transform; }

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

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr;

	// カメラ
	Camera* camera_ = nullptr;

	// 体（親）
	BodyPart body_;

	// 他の部位（子）
	std::vector<BodyPart> parts_;

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

