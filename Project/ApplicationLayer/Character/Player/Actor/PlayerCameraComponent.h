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
	/// Playerの視点角度を所有し、共通CameraComponentへTransform同期を委譲するComponent。
	class PlayerCameraComponent final : public CameraComponent
	{
	public:
		/// Player視点では親回転を継承せず、自身のPitch/Yawを使用する。
		void Initialize() override
		{
			EnsureAttachedToOwnerRoot(); // JSON復元やPIE複製後もPlayer Rootへの追従関係を必ず復元する。
			SetInheritParentRotation(false);
			SetAutoRegisterMainCamera(true); // PlayerCameraはゲーム用Cameraなので、PIE生成時の一時的な自動登録解除より優先する。
			CameraComponent::Initialize();
			ActivateAsMainCameraDriver();
		}

		/// 入力Componentから配送された視点要求を角度へ反映してから共通Camera更新を行う。
		void Update(float deltaTime) override
		{
			EnsureGameplayCameraEnabled(); // ActorWorldのJSON Spawn経路で一度OFFにされてもPlay中は必ず復旧する。
			EnsureAttachedToOwnerRoot();
			yaw_ += pendingYawDelta_;
			pitch_ = std::clamp(pitch_ + pendingPitchDelta_, -maxPitch_, maxPitch_);
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;

			// 長時間操作でも角度が巨大化しないようYawを[-pi, pi]へ正規化する。
			yaw_ = std::remainder(yaw_, std::numbers::pi_v<float> * 2.0f);
			SetLocalRotation({ pitch_, yaw_, 0.0f });
			ActivateAsMainCameraDriver();
			CameraComponent::Update(deltaTime);
		}

		/// PhysicsでPlayer Rootが補正された後、最終位置をMain Cameraへ必ず反映する。
		void PostPhysicsUpdate(float deltaTime) override
		{
			EnsureGameplayCameraEnabled();
			EnsureAttachedToOwnerRoot();
			RefreshWorldTransform(); // Collider補正後のRoot位置を子CameraのWorld位置へ確実に伝播させる。
			ActivateAsMainCameraDriver();
			CameraComponent::PostPhysicsUpdate(deltaTime);
		}

		/// Actor側の最終PostPhysics地点から、Player Cameraをそのフレームの描画Cameraへ確定する。
		void SyncToMainCameraNow()
		{
			EnsureGameplayCameraEnabled();
			EnsureAttachedToOwnerRoot();
			RefreshWorldTransform();
			ActivateAsMainCameraDriver();
			CameraComponent::PostPhysicsUpdate(0.0f);
		}

		/// Player視点角度をDebug表示する。
		void DrawImGui() override
		{
			CameraComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤーカメラ");
			ImGui::Text("Pitch: %.3f / Yaw: %.3f", pitch_, yaw_);
			ImGui::Text("Parent: %s", GetParent() ? GetParent()->GetName().c_str() : "None");
			ImGui::Text("World Position: %.2f, %.2f, %.2f", GetWorldPosition().x, GetWorldPosition().y, GetWorldPosition().z);
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
			yaw_ = std::remainder(yaw_, std::numbers::pi_v<float> * 2.0f);
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
			EnsureAttachedToOwnerRoot();
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
			yaw_ = std::isfinite(yaw) ? std::remainder(yaw, std::numbers::pi_v<float> * 2.0f) : 0.0f;
			pendingYawDelta_ = 0.0f;
			pendingPitchDelta_ = 0.0f;
			EnsureGameplayCameraEnabled();
			EnsureAttachedToOwnerRoot();
			SetLocalRotation({ pitch_, yaw_, 0.0f });
			SyncToMainCameraNow();
		}

		float GetPitch() const { return pitch_; }
		float GetYaw() const { return yaw_; }

	private:
		/// PlayerCameraは通常の配置用Cameraと違い、Play中のゲーム視点として常にMain Cameraへ接続する。
		void EnsureGameplayCameraEnabled()
		{
			SetAutoRegisterMainCamera(true);
		}

		/// Player CameraがPIE複製やJSON復元後も必ず所有PlayerのRootを親に持つよう補修する。
		void EnsureAttachedToOwnerRoot()
		{
			Actor* owner = GetOwner();
			SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
			if (!root || root == this || GetParent() == root) return;
			AttachTo(root);
		}

		float pitch_ = 0.0f;
		float yaw_ = 0.0f;
		float pendingPitchDelta_ = 0.0f;
		float pendingYawDelta_ = 0.0f;
		float maxPitch_ = std::numbers::pi_v<float> * 0.49f;
	};
} // namespace Ken4lowEngine
