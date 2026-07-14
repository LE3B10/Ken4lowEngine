#pragma once

#include <CameraComponent.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの視点角度を所有し、共通CameraComponentへTransform同期を委譲するComponent。
	class PlayerCameraComponent final : public CameraComponent
	{
	public:
		/// Player視点では親回転を継承せず、自身のPitch/Yawを使用する。
		void Initialize() override
		{
			SetInheritParentRotation(false);
			CameraComponent::Initialize();
			ActivateAsMainCameraDriver(); // Player生成時点からゲーム用Main CameraのDriverとして確定する。
		}

		/// 入力Componentから配送された視点要求を角度へ反映してから共通Camera更新を行う。
		void Update(float deltaTime) override
		{
			(void)deltaTime;
			yaw_ += pendingYawDelta_;
			pitch_ = std::clamp(pitch_ + pendingPitchDelta_, -maxPitch_, maxPitch_);
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
			SetLocalRotation({ pitch_, yaw_, 0.0f }); // 視点角度だけを確定し、Camera本体への同期は基底Componentへ任せる。
			ActivateAsMainCameraDriver(); // 他CameraComponentが存在してもPlayer Cameraを現在のMain Camera Driverへ戻す。
			CameraComponent::Update(deltaTime);
		}

		/// PhysicsでPlayer Rootが補正された後、最終位置をMain Cameraへ必ず反映する。
		void PostPhysicsUpdate(float deltaTime) override
		{
			ActivateAsMainCameraDriver();
			CameraComponent::PostPhysicsUpdate(deltaTime);
		}

		/// Player視点角度をDebug表示する。
		void DrawImGui() override
		{
			CameraComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤーカメラ");
			ImGui::Text("Pitch: %.3f / Yaw: %.3f", pitch_, yaw_);
#endif
		}

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "PlayerCameraComponent"; }

		/// Player固有の視点角度をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override
		{
			CameraComponent::ToJson(outJson);
			outJson["Pitch"] = pitch_;
			outJson["Yaw"] = yaw_;
		}

		/// Actor JSONからPlayer固有の視点角度を復元する。
		void FromJson(const nlohmann::json& inJson) override
		{
			CameraComponent::FromJson(inJson);
			pitch_ = inJson.value("Pitch", pitch_);
			yaw_ = inJson.value("Yaw", yaw_);
			if (!std::isfinite(pitch_)) pitch_ = 0.0f;
			if (!std::isfinite(yaw_)) yaw_ = 0.0f;
			pitch_ = std::clamp(pitch_, -maxPitch_, maxPitch_);
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
		}

		/// 入力Componentから1フレーム分の視点回転要求を受け取る。
		void RequestLook(float yawDelta, float pitchDelta)
		{
			if (std::isfinite(yawDelta)) pendingYawDelta_ += yawDelta;
			if (std::isfinite(pitchDelta)) pendingPitchDelta_ += pitchDelta;
		}

		/// Debug検証用に視点角度と未処理要求を初期化する。
		void ResetLook(float pitch = 0.0f, float yaw = 0.0f)
		{
			pitch_ = std::clamp(pitch, -maxPitch_, maxPitch_);
			yaw_ = yaw;
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
			SetLocalRotation({ pitch_, yaw_, 0.0f });
			ActivateAsMainCameraDriver(); // Reset直後もPlayer位置・向きをMain Cameraへ同期できる状態に戻す。
		}

		float GetPitch() const { return pitch_; }
		float GetYaw() const { return yaw_; }

	private:
		float pitch_ = 0.0f;
		float yaw_ = 0.0f;
		float pendingPitchDelta_ = 0.0f;
		float pendingYawDelta_ = 0.0f;
		float maxPitch_ = std::numbers::pi_v<float> * 0.49f;
	};
} // namespace Ken4lowEngine
