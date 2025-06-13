#include "FpsCamera.h"
#include <Matrix4x4.h>
#include <Object3DCommon.h>
#include "Player.h"
#include <Camera.h>
#include <Input.h>
#include "LinearInterpolation.h"
#include <Wireframe.h>

void FpsCamera::Initialize(Player* player)
{
	input_ = Input::GetInstance();
	player_ = player;
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
}

void FpsCamera::Update(bool ignoreInput)
{
	if (!player_ || !camera_) return;
	if (player_->IsDebugCamera()) return; // デバッグカメラ中は更新しない

	if (!ignoreInput)
	{
		int mouseDX = input_->GetMouseMoveX(); // マウスのX軸移動量
		int mouseDY = input_->GetMouseMoveY(); // マウスのY軸移動量

		// --- ADS状態によって感度を変更 ---
		float effectiveMouseSensitivity = isAiming_ ? mouseSensitivity_ * 0.5f : mouseSensitivity_;
		float effectiveControllerSensitivity = isAiming_ ? controllerSensitivity_ * 0.5f : controllerSensitivity_;

		// --- マウスとコントローラーの入力を組み合わせて視点を更新 ---
		float sensitivityModifier = player_->IsAiming() ? adsSensitivityFactor_ : 1.0f;

		float deltaYaw = -mouseDX * mouseSensitivity_ * sensitivityModifier; // マウスのX軸移動量に基づくヨー角の変化
		float deltaPitch = mouseDY * mouseSensitivity_ * sensitivityModifier; // マウスのY軸移動量に基づくピッチ角の変化

		Vector2 rStick = input_->GetRightStick();
		if (!input_->RStickInDeadZone()) {
			deltaYaw += -rStick.x * controllerSensitivity_ * sensitivityModifier;
			deltaPitch += -rStick.y * controllerSensitivity_ * sensitivityModifier;
		}

		yaw_ += deltaYaw;
		pitch_ = std::clamp(pitch_ + deltaPitch, minPitch_, maxPitch_);
	}

	// --- カメラ高さをしゃがみに応じて補間 ---
	const float targetEyeHeight = player_->IsCrouching() ? crouchEyeHeight_ : standEyeHeight_; // しゃがみ時は1.2、立ち上がり時は1.74
	eyeHeight_ = Lerp(eyeHeight_, targetEyeHeight, 0.1f); // 0.1fは補間のスピード

	// --- カメラ位置設定（プレイヤーの頭位置） ---
	Vector3 playerPos = player_->GetWorldTransform()->translate_;
	Vector3 camPos = playerPos + Vector3{ 0, eyeHeight_, 0 };

	// --- ボビング処理 ---
	Vector3 moveInput = player_->GetMoveInput(); // Player経由で移動ベクトル取得
	bool isMoving = Vector3::Length(moveInput) > 0.01f;
	bool isGrounded = player_->IsGrounded(); // Playerに関数があれば（無ければpublic bool参照）
	bool isDashing = player_->IsDashing(); // ダッシュ状態を取得

	float bobbingOffsetY = 0.0f; // ボビングのYオフセット

	// 状態に応じて揺れを変更
	float speed = isDashing ? 14.0f : 10.0f;        // 揺れる速さ
	float amplitude = isDashing ? 0.14f : 0.1f;    // 揺れの高さ

	// 徐々に追従（Lerp）
	currentBobbingSpeed_ = Lerp(currentBobbingSpeed_, speed, 0.1f);
	currentBobbingAmplitude_ = Lerp(currentBobbingAmplitude_, amplitude, 0.1f);

	// ボビング時間加算
	if (isMoving && isGrounded && !isAiming_)
	{
		bobbingTimer_ += speed * deltaTime_;
		bobbingOffsetY = std::sinf(bobbingTimer_) * currentBobbingAmplitude_;
	}
	else
	{
		bobbingTimer_ = 0.0f;
	}

	camPos.y += bobbingOffsetY;

	// ---------------------- 着地バウンド演出 ----------------------
	if (!wasGrounded_ && isGrounded) {
		// 着地した瞬間
		landingBounceTimer_ = 0.0f;
	}
	wasGrounded_ = isGrounded;

	if (landingBounceTimer_ < landingBounceDuration_) {
		landingBounceTimer_ += deltaTime_;
		float t = landingBounceTimer_ / landingBounceDuration_;
		float easing = std::sin(t * std::numbers::pi_v<float>); // sin波で上下揺れ
		camPos.y -= easing * landingBounceAmplitude_; // 縦に沈ませる
	}

	// --- オイラー角ベースのビュー行列作成 ---
	Vector3 camEuler = { pitch_, yaw_, 0.0f }; // X, Y 回転
	Matrix4x4 rotMat = Matrix4x4::MakeRotateMatrix(camEuler);
	Matrix4x4 transMat = Matrix4x4::MakeTranslateMatrix(camPos);
	Matrix4x4 viewMat = Matrix4x4::Inverse(rotMat * transMat);

	camera_->SetViewMatrix(viewMat);
	camera_->SetTranslate(camPos);
	camera_->SetRotate(camEuler);
}

void FpsCamera::DrawDebugCamera()
{
	AABB aabb;
	aabb.min = { -0.125f, eyeHeight_, -0.125f };
	aabb.max = { 0.125f, eyeHeight_,  0.125f };

	Wireframe::GetInstance()->DrawAABB(aabb, { 0.0f, 0.0f, 1.0f, 1.0f });
}
