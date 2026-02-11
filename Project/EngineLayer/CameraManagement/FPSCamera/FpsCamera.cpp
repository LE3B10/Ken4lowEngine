#include "FpsCamera.h"
#include <Matrix4x4.h>
#include <Object3DCommon.h>
#include "Player.h"
#include <Camera.h>
#include <Input.h>
#include "LinearInterpolation.h"
#include <Wireframe.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include <random>

namespace Ken4lowEngine
{

	/// ----------------------------------------------
	///					初期化処理
	/// ----------------------------------------------
	void FpsCamera::Initialize(Player* player)
	{
		input_ = Input::GetInstance();
		player_ = player;
		camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
		camera_->SetNearClip(0.05f);

		// リコイルの横ブレ用
		randomEngine_.seed(std::random_device{}());

		// 初期FOV（腰だめ）
		camera_->SetFovY(DegToRad(hipFovDeg_));
	}

	void FpsCamera::UpdateLook(const InputSnapshot& in, bool ignoreInput)
	{
		if (!player_ || !camera_) return;

		// ---------- ADS ブレンド更新 ----------
		const float targetAim = (isAiming_ && viewMode_ == ViewMode::FirstPerson) ? 1.0f : 0.0f;
		const float aimSpeed = (targetAim > aimAlpha_) ? adsInSpeed_ : adsOutSpeed_;
		const float tAim = 1.0f - std::exp(-aimSpeed * deltaTime_);
		aimAlpha_ = Lerp(aimAlpha_, targetAim, tAim);

		// ---------- FOV 補間 ----------
		const float fovDeg = Lerp(hipFovDeg_, adsFovDeg_, aimAlpha_);
		camera_->SetFovY(DegToRad(fovDeg));

		// ---------- 入力から yaw/pitch 更新 ----------
		if (!ignoreInput)
		{
			const float sensScale = Lerp(1.0f, adsSensitivityFactor_, aimAlpha_);

			// マウス（相対移動）
			yaw_ += -in.lookMouseX * (mouseSensitivity_ * sensScale);
			pitch_ += in.lookMouseY * (mouseSensitivity_ * sensScale);

			// パッド（-1..+1 を dt で積分）
			yaw_ += -in.lookPadX * (controllerSensitivity_ * sensScale) * deltaTime_;
			pitch_ += -in.lookPadY * (controllerSensitivity_ * sensScale) * deltaTime_;

			pitch_ = std::clamp(pitch_, minPitch_, maxPitch_);
		}

		// ---------- Idle sway 更新 ----------
		applyIdleSwayThisFrame_ = idleSwayEnabled_ && (viewMode_ == ViewMode::FirstPerson || idleSwayApplyInThird_);
		if (applyIdleSwayThisFrame_)
		{
			idleSwayTimer_ += deltaTime_;
			const float swayScale = Lerp(1.0f, adsSwayScale_, aimAlpha_);

			const float Ay = DegToRad(idleSwayAmpYawDeg_) * swayScale;
			const float Ap = DegToRad(idleSwayAmpPitchDeg_) * swayScale;

			const float targetYaw =
				sinf(idleSwayTimer_ * idleSwayFreqYaw1_) * Ay * 0.65f +
				sinf(idleSwayTimer_ * idleSwayFreqYaw2_) * Ay * 0.35f;

			const float targetPitch =
				sinf(idleSwayTimer_ * idleSwayFreqPit1_) * Ap * 0.60f +
				sinf(idleSwayTimer_ * idleSwayFreqPit2_) * Ap * 0.40f;

			const float posX = idleSwayPosX_ * swayScale;
			const float posY = idleSwayPosY_ * swayScale;
			const float posZ = idleSwayPosZ_ * swayScale;

			const float sx =
				sinf(idleSwayTimer_ * idleSwayFreqPos1_) * posX * 0.60f +
				sinf(idleSwayTimer_ * idleSwayFreqPos2_) * posX * 0.40f;

			const float sy =
				sinf(idleSwayTimer_ * idleSwayFreqPos1_) * posY * 0.65f +
				sinf(idleSwayTimer_ * (idleSwayFreqPos2_ * 0.9f)) * posY * 0.35f;

			const float sz =
				cosf(idleSwayTimer_ * (idleSwayFreqPos1_ * 0.8f)) * posZ * 0.60f +
				cosf(idleSwayTimer_ * (idleSwayFreqPos2_ * 0.7f)) * posZ * 0.40f;

			const Vector3 targetPos = { sx, sy, sz };

			const float t = 1.0f - std::exp(-idleSwaySmooth_ * deltaTime_);
			idleSwayYawRad_ = Lerp(idleSwayYawRad_, targetYaw, t);
			idleSwayPitchRad_ = Lerp(idleSwayPitchRad_, targetPitch, t);
			idleSwayPosOffset_ = Lerp(idleSwayPosOffset_, targetPos, t);
		}
		else
		{
			idleSwayYawRad_ = 0.0f;
			idleSwayPitchRad_ = 0.0f;
			idleSwayPosOffset_ = { 0.0f, 0.0f, 0.0f };
		}

		// ---------- このフレームの回転をキャッシュ ----------
		cachedEuler_ = {
			pitch_ + idleSwayPitchRad_ + recoilOffsetPitch_,
			yaw_ + idleSwayYawRad_ + recoilOffsetYaw_,
			0.0f
		};

		if (viewMode_ == ViewMode::ThirdFront)
		{
			cachedEuler_.y -= std::numbers::pi_v<float>;
			cachedEuler_.x = -cachedEuler_.x;
		}

		// ----- リコイルを元に戻す（毎フレーム）-----
		if (recoilEnabled_)
		{
			const float t = 1.0f - std::exp(-recoilReturnSpeed_ * deltaTime_);
			recoilOffsetPitch_ = Lerp(recoilOffsetPitch_, 0.0f, t);
			recoilOffsetYaw_ = Lerp(recoilOffsetYaw_, 0.0f, t);
		}
		else
		{
			recoilOffsetPitch_ = 0.0f;
			recoilOffsetYaw_ = 0.0f;
		}
	}

	void FpsCamera::SyncToPlayer()
	{
		if (!player_ || !camera_) return;

		// プレイヤーの頭位置
		Vector3 playerPos = player_->GetWorldTransform()->translate_;
		const Vector3 eyeBase = playerPos + Vector3{ 0.0f, eyeHeight_, 0.0f };

		// まず回転を反映（forward取得のため）
		camera_->SetRotate(cachedEuler_);

		// ※Camera::GetForward() は rotate_ だけ見るので camera_->Update() は不要
		Vector3 fwd = camera_->GetForward();

		// 視点位置
		Vector3 camPos = eyeBase;
		switch (viewMode_)
		{
		case ViewMode::FirstPerson:
			camPos = eyeBase + fwd * 0.08f;
			break;

		case ViewMode::ThirdBack:
			camPos = eyeBase - fwd * tpsDistance_;
			camPos.y += tpsUpOffset_;
			break;

		case ViewMode::ThirdFront:
			camPos = eyeBase - fwd * tpsForward_;
			camPos.y += tpsUpOffset_;
			break;
		}

		// 位置揺れ（呼吸）
		if (applyIdleSwayThisFrame_)
		{
			const Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
			Vector3 right = Vector3::Normalize(Vector3::Cross(worldUp, fwd));
			camPos += right * idleSwayPosOffset_.x;
			camPos += worldUp * idleSwayPosOffset_.y;
			camPos += fwd * idleSwayPosOffset_.z;
		}

		camera_->SetTranslate(camPos);

		camera_->Update();
	}

	void FpsCamera::Update(const InputSnapshot& in, bool ignoreInput)
	{
		UpdateLook(in, ignoreInput);
		SyncToPlayer();
		if (player_) player_->SetFirstPersonView(viewMode_ == ViewMode::FirstPerson);
	}

	/// ----------------------------------------------
	///					更新処理
	/// ----------------------------------------------
	void FpsCamera::Update(bool ignoreInput)
	{
		if (!input_) return;

		InputSnapshot in{};
		if (!ignoreInput)
		{
			in.lookMouseX = static_cast<float>(input_->GetMouseMoveX());
			in.lookMouseY = static_cast<float>(input_->GetMouseMoveY());

			if (input_->IsConnect() && !input_->RStickInDeadZone())
			{
				const auto rs = input_->GetRightStick();
				in.lookPadX = rs.x;
				in.lookPadY = rs.y;
			}
		}

		Update(in, ignoreInput);
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

	void FpsCamera::DrawImGui()
	{
		if (!camera_) return;

#ifdef USE_IMGUI
		ImGui::Begin("FPS Camera");

		// 現在モード表示
		const char* modeStr = "";
		switch (viewMode_)
		{
		case ViewMode::FirstPerson: modeStr = "FirstPerson"; break;
		case ViewMode::ThirdBack:   modeStr = "ThirdBack";   break;
		case ViewMode::ThirdFront:  modeStr = "ThirdFront";  break;
		}
		ImGui::Text("ViewMode: %s", modeStr);

		// TPS用
		ImGui::DragFloat("TPS Distance", &tpsDistance_, 0.05f, 0.1f, 20.0f);
		ImGui::DragFloat("TPS Up Offset", &tpsUpOffset_, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("TPS Forward (front)", &tpsForward_, 0.05f, 0.1f, 20.0f);

		// Idle sway
		ImGui::Separator();
		ImGui::Checkbox("Idle Sway Enabled", &idleSwayEnabled_);
		ImGui::Checkbox("Apply in TPS", &idleSwayApplyInThird_);
		ImGui::DragFloat("Sway Amp Yaw (deg)", &idleSwayAmpYawDeg_, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("Sway Amp Pitch (deg)", &idleSwayAmpPitchDeg_, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("Sway Freq Yaw1", &idleSwayFreqYaw1_, 0.01f, 0.1f, 10.0f);
		ImGui::DragFloat("Sway Freq Yaw2", &idleSwayFreqYaw2_, 0.01f, 0.1f, 10.0f);
		ImGui::DragFloat("Sway Freq Pit1", &idleSwayFreqPit1_, 0.01f, 0.1f, 10.0f);
		ImGui::DragFloat("Sway Freq Pit2", &idleSwayFreqPit2_, 0.01f, 0.1f, 10.0f);

		// 位置揺れ（メンバへ反映）
		float posX = idleSwayPosX_;
		float posY = idleSwayPosY_;
		float posZ = idleSwayPosZ_;
		ImGui::DragFloat("Sway Pos X", &posX, 0.001f, 0.0f, 0.05f, "%.3f");
		ImGui::DragFloat("Sway Pos Y", &posY, 0.001f, 0.0f, 0.05f, "%.3f");
		ImGui::DragFloat("Sway Pos Z", &posZ, 0.001f, 0.0f, 0.05f, "%.3f");
		idleSwayPosX_ = posX;
		idleSwayPosY_ = posY;
		idleSwayPosZ_ = posZ;

		ImGui::DragFloat("Sway Freq Pos1", &idleSwayFreqPos1_, 0.01f, 0.1f, 10.0f);
		ImGui::DragFloat("Sway Freq Pos2", &idleSwayFreqPos2_, 0.01f, 0.1f, 10.0f);
		ImGui::DragFloat("Sway Smooth", &idleSwaySmooth_, 0.1f, 0.0f, 60.0f);

		// ADS
		ImGui::Separator();
		ImGui::Text("ADS");
		ImGui::DragFloat("Hip FOV (deg)", &hipFovDeg_, 0.1f, 40.0f, 110.0f, "%.1f");
		ImGui::DragFloat("ADS FOV (deg)", &adsFovDeg_, 0.1f, 20.0f, hipFovDeg_, "%.1f");
		ImGui::DragFloat("ADS Sens Factor", &adsSensitivityFactor_, 0.01f, 0.05f, 1.0f, "%.2f");
		ImGui::DragFloat("ADS In Speed", &adsInSpeed_, 0.1f, 0.1f, 60.0f, "%.1f");
		ImGui::DragFloat("ADS Out Speed", &adsOutSpeed_, 0.1f, 0.1f, 60.0f, "%.1f");
		ImGui::DragFloat("ADS Sway Scale", &adsSwayScale_, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::Text("Aim Alpha: %.2f", aimAlpha_);

		static float nearTmp = 0.05f;
		static float farTmp = 1000.0f;
		if (ImGui::DragFloat("Near (for FPS)", &nearTmp, 0.001f, 0.01f, 0.2f, "%.3f")) {
			camera_->SetNearClip(nearTmp);
		}
		if (ImGui::DragFloat("Far (for FPS)", &farTmp, 1.0f, 50.0f, 10000.0f)) {
			camera_->SetFarClip(farTmp);
		}

		if (ImGui::Button("Reset View"))
		{
			yaw_ = 0.0f;
			pitch_ = 0.0f;
			eyeHeight_ = 1.5f;
			hipFovDeg_ = 60.0f;
			adsFovDeg_ = 45.0f;
			adsSensitivityFactor_ = 0.25f;
			adsInSpeed_ = 18.0f;
			adsOutSpeed_ = 14.0f;
			adsSwayScale_ = 0.25f;
			camera_->SetFovY(DegToRad(hipFovDeg_));
			camera_->SetNearClip(0.05f);
			camera_->SetFarClip(1000.0f);
			aimAlpha_ = 0.0f;
		}

		ImGui::End();
#endif // USE_IMGUI
	}

	/// ----------------------------------------------
	///					反動を追加
	/// ----------------------------------------------
	void FpsCamera::AddRecoil(float verticalAmount, float horizontalAmount)
	{
		if (!recoilEnabled_) return;

		std::uniform_real_distribution<float> horizontalDist(-horizontalAmount, horizontalAmount);

		// 縦：上を向かせたいので pitch をマイナス方向へ
		recoilOffsetPitch_ += -verticalAmount;

		// 横：ランダムブレ
		recoilOffsetYaw_ += horizontalDist(randomEngine_);

		// 上限（度→ラジアン）
		const float maxP = DegToRad(recoilMaxPitchDeg_);
		const float maxY = DegToRad(recoilMaxYawDeg_);
		recoilOffsetPitch_ = std::clamp(recoilOffsetPitch_, -maxP, +maxP);
		recoilOffsetYaw_ = std::clamp(recoilOffsetYaw_, -maxY, +maxY);
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

} // namespace Ken4lowEngine
