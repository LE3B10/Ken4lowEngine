#pragma once
#include <Quaternion.h>
#include <random>

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

	// 初期化処理
	void Initialize(Player* player);

	// 更新処理
	void Update(bool ignoreInput = false);

	// デバッグ用カメラの位置をワイヤーフレームで描画
	void DrawDebugCamera();

	void AddRecoil(float verticalAmount = 0.0f, float horizontalAmount = 0.0f);

public: // ---------- ゲッタ ---------- //

	// カメラ取得
	Camera* GetCamera() const { return camera_; }

	// Yaw / Pitch取得
	float GetYaw() const { return yaw_; }
	float GetPitch() const { return pitch_; }

public: // ---------- セッタ ---------- //

	// Aiming状態を設定
	void SetAiming(bool isAiming) { isAiming_ = isAiming; }

	// ADS状態の感度補正係数を設定
	void SetAdsSensitivityFactor(float factor) { adsSensitivityFactor_ = factor; }

	// 外部から時間を貰う
	void SetDeltaTime(float deltaTime) { deltaTime_ = deltaTime; }

private: // ---------- メンバ ---------- //

	Input* input_ = nullptr;
	Camera* camera_ = nullptr;
	Player* player_ = nullptr;

	// 視点角度（ラジアン）
	float yaw_ = 0.0f;
	float pitch_ = 0.0f;

	// 感度
	const float mouseSensitivity_ = 0.002f; // マウス感度（例: 0.002f）
	const float controllerSensitivity_ = 0.05f; // コントローラー感度（例: 0.05f）

	// ピッチ制限
	const float minPitch_ = -1.5f; // 下限
	const float maxPitch_ = +1.5f; // 上限

	// カメラ高さオフセット（頭位置）
	float eyeHeight_ = 1.74f;

	// Aiming状態フラグ
	bool isAiming_ = false;
	// ADS状態の感度補正係数（例: 0.5で半分の感度）
	float adsSensitivityFactor_ = 0.25f;

	// ボビング処理用
	float currentBobbingSpeed_;
	float currentBobbingAmplitude_;
	float bobbingTimer_ = 0.0f;
	float bobbingAmplitude_ = 0.25f;   // 振れ幅
	float bobbingSpeed_ = 10.0f;       // サイクル速度
	float deltaTime_ = 1.0f / 60.0f;    // 仮：外部から渡すべき

	// しゃがむ状態のフラグ
	bool isCrouching_ = false; // しゃがむ状態のフラグ
	const float standEyeHeight_ = 1.685f; // 立ち上がり時の目の高さ
	const float crouchEyeHeight_ = 1.2f; // しゃがみ時の目の高さ

	// 着地検出用（前フレームとの比較）
	bool wasGrounded_ = true;

	// 着地バウンド処理用
	float landingBounceTimer_ = 0.0f;
	const float landingBounceDuration_ = 0.25f; // バウンドの持続時間
	const float landingBounceAmplitude_ = 0.25f; // バウンドの深さ

	float recoilOffsetPitch_ = 0.0f;
	float recoilOffsetYaw_ = 0.0f;
	std::default_random_engine randomEngine_;

	bool debugThirdPerson_ = false;   // true のとき TPS 表示
	// TPS オフセット（好みに応じて調整）
	float debugCamDistance_ = 3.0f; // 背後距離
	float debugShoulderHeight_ = 1.6f; // 肩の高さ
	float debugSideOffset_ = 0.0f;// 右肩越し (+左なら負に)
};
