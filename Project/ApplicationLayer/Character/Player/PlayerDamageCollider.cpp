#include "PlayerDamageCollider.h"

#include "CollisionPreset.h"
#include "Player.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <cmath>

namespace
{
	Ken4lowEngine::Vector3 RotateYaw(const Ken4lowEngine::Vector3& v, float yaw)
	{
		const float c = std::cos(yaw);
		const float s = std::sin(yaw);
		return
		{
			v.x * c + v.z * s,
			v.y,
			-v.x * s + v.z * c,
		};
	}
}

void PlayerDamageCollider::Initialize(Player* owner, uint32_t typeId)
{
	ApplyCollisionPreset(*this, ECollisionPresetId::Player);
	SetOwner(owner);
	SetTypeID(typeId);
	owner_ = owner;
	SetOBBHalfSize(halfSize_);
	SyncFromOwner();
}

void PlayerDamageCollider::SyncFromOwner()
{
	if (!owner_)
	{
		SetEnabled(false);
		return;
	}

	auto* tr = owner_->GetWorldTransform();
	if (!tr)
	{
		SetEnabled(false);
		return;
	}

	SetEnabled(true);
	SetOBBHalfSize(halfSize_);
	ClearOBBBasis();

	// プレイヤー被弾判定は本体TransformのYawだけを使い、腕やViewModelの姿勢には依存しない。
	const float yaw = tr->rotate_.y;
	SetCenterPosition(tr->translate_ + RotateYaw(localOffset_, yaw));
	SetOrientation({ 0.0f, yaw, 0.0f });
}

void PlayerDamageCollider::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Player Damage Collider", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Local Offset", &localOffset_.x, 0.01f, -5.0f, 5.0f, "%.2f");
		ImGui::DragFloat3("Half Size", &halfSize_.x, 0.01f, 0.01f, 5.0f, "%.2f");
	}
#endif
}

void PlayerDamageCollider::OnCollisionEnter(Ken4lowEngine::Collider* other)
{
	if (!other || !owner_) return;
	owner_->OnHitByEnemyBullet(other, PlayerHitPart::Body, 1.0f);
}

void PlayerDamageCollider::OnCollisionEnter(const Ken4lowEngine::CollisionHit& hit)
{
	OnCollisionEnter(hit.other);
}
