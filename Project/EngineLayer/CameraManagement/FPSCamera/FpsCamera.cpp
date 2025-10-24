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

	// ------------- 入力から yaw/pitch 更新 -------------
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

	// ------------- プレイヤーの頭位置へ置く -------------
	Vector3 playerPos = player_->GetWorldTransform()->translate_;
	const Vector3 eyeBase = playerPos + Vector3{ 0.0f, eyeHeight_, 0.0f };

	// このフレームのカメラ回転を先に適用して forward を最新化
	Vector3 camEuler = { pitch_, yaw_, 0.0f };
	if (viewMode_ == ViewMode::ThirdFront)
	{
		// 自撮りモードはプレイヤー側を向くため180度回す
		camEuler.y -= std::numbers::pi_v<float>;
		camEuler.x = -camEuler.x; // 上下も反転
	}
	camera_->SetRotate(camEuler);
	camera_->Update(); // ここで forward が最新になる

	// 最新の forward を取得
	Vector3 fwd = camera_->GetForward();

	Vector3 camPos = eyeBase;

	switch (viewMode_)
	{
	case ViewMode::FirstPerson:
		// ほんの少しだけ前へ押し出して首振り時に埋まらないように
		camPos = eyeBase + fwd * 0.08f;
		break;

	case ViewMode::ThirdBack:
		// 後ろから肩越し。少し上げると見やすい
		camPos = eyeBase - fwd * tpsDistance_;
		camPos.y += tpsUpOffset_;
		break;

	case ViewMode::ThirdFront:
		// 前方から自分を見る。上の camEuler で180°回してあるので向きはプレイヤー側
		camPos = eyeBase - fwd * tpsForward_;
		camPos.y += tpsUpOffset_;
		break;
	}

	camera_->SetTranslate(camPos);
	camera_->Update();
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

/// ----------------------------------------------
///				表示モードを切替
/// ----------------------------------------------
void FpsCamera::CycleViewMode()
{
	switch (viewMode_)
	{
	case ViewMode::FirstPerson: viewMode_ = ViewMode::ThirdBack;  break;
	case ViewMode::ThirdBack:   viewMode_ = ViewMode::ThirdFront; break;
	case ViewMode::ThirdFront:  viewMode_ = ViewMode::FirstPerson; break;
	}
}
