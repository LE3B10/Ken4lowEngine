#pragma once
#include <Quaternion.h>

/// ---------- 前方宣言 ---------- ///
class Player;
class Input;
class Camera;

/// -------------------------------------------------------------
///					FPS視点専用カメラクラス
/// -------------------------------------------------------------
class FpsCamera
{
public: // ---------- 関数 ---------- //

	void Initialize(Player* player);
	void Update(bool ignoreInput = false);

	// カメラ取得
	Camera* GetCamera() const { return camera_; }

	// Yaw / Pitch取得
	float GetYaw() const { return yaw_; }
	float GetPitch() const { return pitch_; }

private: // ---------- メンバ ---------- //

	Input* input_ = nullptr;
	Camera* camera_ = nullptr;
	Player* player_ = nullptr;

	// 視点角度（ラジアン）
	float yaw_ = 0.0f;
	float pitch_ = 0.0f;

	// 感度
	const float mouseSensitivity_ = 0.002f;
	const float controllerSensitivity_ = 0.05f;

	// ピッチ制限
	const float minPitch_ = -1.5f; // 下限
	const float maxPitch_ = +1.5f; // 上限

	// カメラ高さオフセット（頭位置）
	const float eyeHeight_ = 20.0f;
};
