#pragma once

#include <Actor.h>
#include <CameraComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの視点角度とADS/Sprint FOVを所有し、共通CameraComponentへTransform同期を委譲するComponent。
	class PlayerCameraComponent final : public CameraComponent
	{
	public:
		void Initialize() override
		{
			EnsureAttachedToOwnerRoot();
			SetInheritParentRotation(false);
			SetAutoRegisterMainCamera(true);
			SetLocalRotation({ pitch_, yaw_, 0.0f });
			SyncOwnerFacingToYaw();
			CameraComponent::Initialize();
			CaptureBaseFovFromCamera(); // 保存済みCamera設定を通常FOVの正本として初回だけ取り込む。
			ApplyCurrentFov();
			ActivateAsMainCameraDriver();
		}

		void Update(float deltaTime) override
		{
			EnsureGameplayCameraEnabled();
			EnsureAttachedToOwnerRoot();
			yaw_ += pendingYawDelta_;
			pitch_ = std::clamp(pitch_ + pendingPitchDelta_, -maxPitch_, maxPitch_);
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
			yaw_ = std::remainder(yaw_, std::numbers::pi_v<float> * 2.0f);
			SetLocalRotation({ pitch_, yaw_, 0.0f });
			SyncOwnerFacingToYaw();
			UpdateFov(deltaTime);
			ActivateAsMainCameraDriver();
			CameraComponent::Update(deltaTime);
		}

		void PostPhysicsUpdate(float deltaTime) override
		{
			EnsureGameplayCameraEnabled();
			EnsureAttachedToOwnerRoot();
			SyncOwnerFacingToYaw();
			RefreshWorldTransform();
			ApplyCurrentFov();
			ActivateAsMainCameraDriver();
			CameraComponent::PostPhysicsUpdate(deltaTime);
		}

		void SyncToMainCameraNow()
		{
			EnsureGameplayCameraEnabled();
			EnsureAttachedToOwnerRoot();
			SyncOwnerFacingToYaw();
			RefreshWorldTransform();
			ApplyCurrentFov();
			ActivateAsMainCameraDriver();
			CameraComponent::PostPhysicsUpdate(0.0f);
		}

		void DrawImGui() override
		{
			CameraComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤーカメラ");
			ImGui::Text("Pitch: %.3f / Yaw: %.3f", pitch_, yaw_);
			ImGui::Text("Aim: %s / Sprint: %s", aimHeld_ ? "Yes" : "No", sprintHeld_ ? "Yes" : "No");
			ImGui::Text("FOV(rad): %.3f / Base: %.3f", currentFovY_, baseFovY_);
			ImGui::SliderFloat("ADS FOV倍率", &aimFovMultiplier_, 0.4f, 1.0f, "%.2f");
			ImGui::SliderFloat("Sprint FOV倍率", &sprintFovMultiplier_, 1.0f, 1.5f, "%.2f");
			ImGui::SliderFloat("FOV追従速度", &fovTransitionSpeed_, 1.0f, 30.0f, "%.2f");
			ImGui::Text("Parent: %s", GetParent() ? GetParent()->GetName().c_str() : "None");
			ImGui::Text("World Position: %.2f, %.2f, %.2f", GetWorldPosition().x, GetWorldPosition().y, GetWorldPosition().z);
#endif
		}

		std::string GetClassTypeName() const override { return "PlayerCameraComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			CameraComponent::ToJson(outJson);
			outJson["Pitch"] = pitch_;
			outJson["Yaw"] = yaw_;
			outJson["AimFovMultiplier"] = aimFovMultiplier_;
			outJson["SprintFovMultiplier"] = sprintFovMultiplier_;
			outJson["FovTransitionSpeed"] = fovTransitionSpeed_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			CameraComponent::FromJson(inJson);
			pitch_ = inJson.value("Pitch", pitch_);
			yaw_ = inJson.value("Yaw", yaw_);
			aimFovMultiplier_ = inJson.value("AimFovMultiplier", aimFovMultiplier_);
			sprintFovMultiplier_ = inJson.value("SprintFovMultiplier", sprintFovMultiplier_);
			fovTransitionSpeed_ = inJson.value("FovTransitionSpeed", fovTransitionSpeed_);
			if (!std::isfinite(pitch_)) pitch_ = 0.0f;
			if (!std::isfinite(yaw_)) yaw_ = 0.0f;
			if (!std::isfinite(aimFovMultiplier_)) aimFovMultiplier_ = 0.72f;
			if (!std::isfinite(sprintFovMultiplier_)) sprintFovMultiplier_ = 1.10f;
			if (!std::isfinite(fovTransitionSpeed_)) fovTransitionSpeed_ = 12.0f;
			pitch_ = std::clamp(pitch_, -maxPitch_, maxPitch_);
			yaw_ = std::remainder(yaw_, std::numbers::pi_v<float> * 2.0f);
			aimFovMultiplier_ = std::clamp(aimFovMultiplier_, 0.4f, 1.0f);
			sprintFovMultiplier_ = std::clamp(sprintFovMultiplier_, 1.0f, 1.5f);
			fovTransitionSpeed_ = std::max(0.1f, fovTransitionSpeed_);
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
			EnsureAttachedToOwnerRoot();
			SetLocalRotation({ pitch_, yaw_, 0.0f });
			SyncOwnerFacingToYaw();
		}

		void RequestLook(float yawDelta, float pitchDelta)
		{
			if (std::isfinite(yawDelta)) pendingYawDelta_ += yawDelta;
			if (std::isfinite(pitchDelta)) pendingPitchDelta_ += pitchDelta;
		}

		void SetAimHeld(bool held) { aimHeld_ = held; }
		void SetSprintHeld(bool held) { sprintHeld_ = held; }

		void ResetLook(float pitch = 0.0f, float yaw = 0.0f)
		{
			pitch_ = std::clamp(pitch, -maxPitch_, maxPitch_);
			yaw_ = std::isfinite(yaw) ? std::remainder(yaw, std::numbers::pi_v<float> * 2.0f) : 0.0f;
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
			aimHeld_ = false;
			sprintHeld_ = false;
			currentFovY_ = baseFovY_;
			EnsureGameplayCameraEnabled();
			EnsureAttachedToOwnerRoot();
			SetLocalRotation({ pitch_, yaw_, 0.0f });
			SyncOwnerFacingToYaw();
			SyncToMainCameraNow();
		}

		float GetPitch() const { return pitch_; }
		float GetYaw() const { return yaw_; }
		bool IsAiming() const { return aimHeld_; }
		float GetCurrentFovY() const { return currentFovY_; }

	private:
		void EnsureGameplayCameraEnabled() { SetAutoRegisterMainCamera(true); }

		void EnsureAttachedToOwnerRoot()
		{
			Actor* owner = GetOwner();
			SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
			if (!root || root == this || GetParent() == root) return;
			AttachTo(root);
		}

		void SyncOwnerFacingToYaw()
		{
			Actor* owner = GetOwner();
			SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
			if (!root || root == this) return;
			Vector3 rootRotation = root->GetLocalRotation();
			rootRotation.y = yaw_;
			root->SetLocalRotation(rootRotation);
			root->RefreshWorldTransform();
		}

		void CaptureBaseFovFromCamera()
		{
			if (fovInitialized_) return;
			if (Camera* camera = GetCamera())
			{
				baseFovY_ = camera->GetFovY();
				if (!std::isfinite(baseFovY_) || baseFovY_ <= 0.0f) baseFovY_ = std::numbers::pi_v<float> / 3.0f;
				currentFovY_ = baseFovY_;
				fovInitialized_ = true;
			}
		}

		void UpdateFov(float deltaTime)
		{
			CaptureBaseFovFromCamera();
			const float targetFov = aimHeld_ ? baseFovY_ * aimFovMultiplier_ : (sprintHeld_ ? baseFovY_ * sprintFovMultiplier_ : baseFovY_);
			const float blend = std::clamp(std::max(0.0f, deltaTime) * fovTransitionSpeed_, 0.0f, 1.0f);
			currentFovY_ += (targetFov - currentFovY_) * blend;
			ApplyCurrentFov();
		}

		void ApplyCurrentFov()
		{
			if (Camera* camera = GetCamera()) camera->SetFovY(currentFovY_);
		}

	private:
		float pitch_ = 0.0f;
		float yaw_ = 0.0f;
		float pendingPitchDelta_ = 0.0f;
		float pendingYawDelta_ = 0.0f;
		float maxPitch_ = std::numbers::pi_v<float> * 0.49f;
		float baseFovY_ = std::numbers::pi_v<float> / 3.0f;
		float currentFovY_ = std::numbers::pi_v<float> / 3.0f;
		float aimFovMultiplier_ = 0.72f;
		float sprintFovMultiplier_ = 1.10f;
		float fovTransitionSpeed_ = 12.0f;
		bool aimHeld_ = false;
		bool sprintHeld_ = false;
		bool fovInitialized_ = false;
	};
} // namespace Ken4lowEngine
