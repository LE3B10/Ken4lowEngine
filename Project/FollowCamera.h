#pragma once
#include <WorldTransform.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Camera;
class Input;
class Player;


/// -------------------------------------------------------------
///						追従カメラクラス
/// -------------------------------------------------------------
class FollowCamera
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// リセット処理
	void Reset();

public: /// ---------- ゲッタ ---------- ///

	// カメラの取得
	Camera* GetCamera() { return camera_; }

public: /// ---------- セッタ ---------- ///

	// 追従対象の設定
	void SetTarget(const WorldTransform* target) { target_ = target; Reset(); }

	// プレイヤー情報を取得
	void SetPlayer(Player* player) { player_ = player; }

private: /// ---------- メンバ関数 ---------- ///

	Vector3 UpdateOffset();

private: /// ---------- メンバ変数 ---------- ///

	// カメラ
	Camera* camera_ = nullptr;

	// 入力
	Input* input_ = nullptr;

	// プレイヤー
	Player* player_ = nullptr;

	// 追従対象
	const WorldTransform* target_ = nullptr;

	// カメラの座標
	Vector3 cameraPos_ = { 0.0f, 0.0f, 0.0f };

	// 追従オフセット（カメラとターゲットの相対位置）
	Vector3 offset_ = { 0.0f, 10.0f, -35.0f }; // 後ろ上から見下ろす位置

	// 追従対象の残像座標
	Vector3 interTarget_ = {};

	// カメラの回転角度 (Yaw: 左右回転, Pitch: 上下回転)
	float cameraYaw_ = 0.0f;
	float cameraPitch_ = 0.0f;

	// 回転速度
	const float rotationSpeed_ = 0.09f;

	// X軸のデッドゾーン（上下回転の制限）
	const float minPitch_ = -0.2f; // 下向き制限
	const float maxPitch_ = 0.65f;   // 上向き制限
};
