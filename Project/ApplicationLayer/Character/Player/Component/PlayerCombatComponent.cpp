#define NOMINMAX
#include "PlayerCombatComponent.h"

#include "BulletManager.h"
#include "Camera.h"
#include "CollisionManager.h"
#include "PlayerViewComponent.h"
#include "PlayerWeaponComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void PlayerCombatComponent::BindDependencies(PlayerWeaponComponent* weapon, PlayerViewComponent* view)
{
	weapon_ = weapon;
	view_ = view;
}

void PlayerCombatComponent::SetAudioCallbacks(std::function<void()>* onFire, std::function<void()>* onReload)
{
	onFireSE_ = onFire;
	onReloadSE_ = onReload;
}

void PlayerCombatComponent::Tick(float deltaTime)
{
	if (shotDebugTimer_ > 0.0f)
	{
		shotDebugTimer_ -= deltaTime;
		if (shotDebugTimer_ < 0.0f)
		{
			shotDebugTimer_ = 0.0f;
		}
	}
}

void PlayerCombatComponent::FireOnce(
	const InputSnapshot& input,
	BulletManager* bulletManager,
	CollisionManager* collisionManager,
	const std::function<void(const K4E::Segment&)>& setDebugSegment)
{
	if (!weapon_ || !view_)
	{
		return;
	}

	// CombatFSM から呼ばれる発射処理
	auto* shootCam = view_->GetShootCamera();
	if (!bulletManager || !shootCam)
	{
		return;
	}

	const bool fired = weapon_->TryFire(input, shootCam, bulletManager, collisionManager);
	if (!fired)
	{
		return;
	}

	// 発射した瞬間のSE
	if (onFireSE_ && *onFireSE_)
	{
		(*onFireSE_)();
	}

	// カメラリコイル
	const bool ads = input.aimHeld;
	const float vDeg = ads ? recoilPitchDegAds_ : recoilPitchDegHip_;
	const float hDeg = ads ? recoilYawDegAds_ : recoilYawDegHip_;
	view_->AddRecoil(vDeg, hDeg);

	// デバッグ用ショットレイ
	if (setDebugSegment)
	{
		if (auto* cam = view_->GetCamera())
		{
			K4E::Segment seg{};
			seg.origin = cam->GetTranslate();
			seg.diff = cam->GetForward() * hitscanRange_;
			setDebugSegment(seg);
			shotDebugTimer_ = 0.05f;
		}
	}
}

void PlayerCombatComponent::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Recoil Tuning", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Pre-shot camera kick (deg)");
		ImGui::DragFloat("Hip Pitch", &recoilPitchDegHip_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Hip Yaw", &recoilYawDegHip_, 0.10f, 0.0f, 20.0f, "%.2f");
		ImGui::Separator();
		ImGui::DragFloat("ADS Pitch", &recoilPitchDegAds_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("ADS Yaw", &recoilYawDegAds_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::Separator();
		ImGui::DragFloat("Hitscan Range", &hitscanRange_, 1.0f, 1.0f, 5000.0f, "%.1f");
		ImGui::Text("ShotDebugTimer: %.3f", shotDebugTimer_);
	}
#endif
}
