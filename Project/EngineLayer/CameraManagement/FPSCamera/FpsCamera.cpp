#include "FpsCamera.h"
#include <Matrix4x4.h>
#include <Object3DCommon.h>
#include "Player.h"
#include <Camera.h>
#include <Input.h>
#include "LinearInterpolation.h"
#include <Wireframe.h>

/// ----------------------------------------------
///					初期化処理
/// ----------------------------------------------
void FpsCamera::Initialize(Player* player)
{
	input_ = Input::GetInstance();
	player_ = player;
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
	camera_->SetNearClip(0.05f);
}

/// ----------------------------------------------
///					更新処理
/// ----------------------------------------------
void FpsCamera::Update(bool ignoreInput)
{
	if (!player_ || !camera_) return;
	if (player_->IsDebugCamera()) return; // デバッグカメラ中は無効

	// ------------- (a) 入力から yaw/pitch 更新 -------------
	if (!ignoreInput)
	{
		const int dx = input_->GetMouseMoveX();
		const int dy = input_->GetMouseMoveY();

		// ADS中は少し感度を下げる（必要なければ isAiming_ を false のままで）
		const float sensMouse = isAiming_ ? mouseSensitivity_ * 0.5f : mouseSensitivity_;

		yaw_ += -dx * sensMouse;                 // 横：左ドラッグで+Yaw
		pitch_ += dy * sensMouse;                 // 縦：上ドラッグで-視線にしたいなら符号を反転
		pitch_ = std::clamp(pitch_, minPitch_, maxPitch_);  // ピッチ制限
	}

	// ------------- (b) プレイヤーの頭位置へ置く -------------
	// 基準位置：プレイヤーのルート位置（腰）＋ 目の高さ
	// ※ Player 側に「現在位置」を返すメソッドが無ければ Body の Object3D::GetTranslate() を使ってください。
	Vector3 playerPos = player_->GetWorldPosition();         // 推奨：小さなゲッタを用意
	// Vector3 playerPos = player_->GetBodyObject()->GetTranslate(); // 代替案

	Vector3 camPos = playerPos + Vector3{ 0.0f, eyeHeight_, 0.0f };

	// カメラ前方に少し押し出す（5～10cm）
	Vector3 forward = camera_->GetForward();     // カメラクラスに前方取得あり
	camPos = camPos + forward * 0.08f;

	// ------------- (c) カメラへ反映（回転は yaw/pitch をそのまま） -------------
	Vector3 camEuler = { pitch_, yaw_, 0.0f };
	camera_->SetTranslate(camPos);
	camera_->SetRotate(camEuler);
	camera_->Update();   // 既存のCameraで行列更新
}

/// ----------------------------------------------
///	デバッグ用カメラの位置をワイヤーフレームで描画
/// ----------------------------------------------
void FpsCamera::DrawDebugCamera()
{
	AABB aabb;
	aabb.min = { -0.125f, eyeHeight_, -0.125f };
	aabb.max = { 0.125f, eyeHeight_,  0.125f };

	Wireframe::GetInstance()->DrawAABB(aabb, { 0.0f, 0.0f, 1.0f, 1.0f });
}

/// ----------------------------------------------
///					反動を追加
/// ----------------------------------------------
void FpsCamera::AddRecoil(float verticalAmount, float horizontalAmount)
{
	std::uniform_real_distribution<float> horizontalDist(-horizontalAmount, horizontalAmount);
	recoilOffsetPitch_ += -verticalAmount;
	recoilOffsetYaw_ += horizontalDist(randomEngine_);  // ランダムな横ブレ
}
