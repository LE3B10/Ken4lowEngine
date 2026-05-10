#include "Item.h"
#include "Player.h"
#include <CollisionTypeIdDef.h>

#include <cmath>
#include <string>

namespace K4E = ::Ken4lowEngine;

namespace
{
	const char* GetModelPath(ItemType type)
	{
		switch (type)
		{
		case ItemType::HealSmall:
		case ItemType::AmmoSmall:
		case ItemType::NextStageKey:
		default:
			return "cube.gltf";
		}
	}
}

void Item::Initialize(ItemType type, const K4E::Vector3& pos, int healAmount, int ammoAmount, float pickupRadius)
{
	K4E::Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kItem));
	K4E::Collider::SetOBBHalfSize(scale_);

	type_ = type;
	position_ = pos;
	basePosition_ = pos;
	active_ = (type_ != ItemType::None);
	pickupRadius_ = pickupRadius;
	healAmount_ = healAmount;
	ammoAmount_ = ammoAmount;
	lifetime_ = 0.0f;
	floatTimer_ = 0.0f;
	rotation_ = { 0.0f, 0.0f, 0.0f };

	object3d_ = std::make_unique<K4E::Object3D>();
	object3d_->Initialize(GetModelPath(type_));
	object3d_->SetTranslate(position_);
	object3d_->SetScale(scale_);

	switch (type_)
	{
	case ItemType::HealSmall:
		object3d_->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
		break;
	case ItemType::AmmoSmall:
		object3d_->SetColor({ 0.0f, 0.0f, 1.0f, 1.0f });
		break;
	case ItemType::NextStageKey:
		object3d_->SetColor({ 0.0f, 1.0f, 1.0f, 1.0f });
		break;
	case ItemType::None:
	default:
		object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 0.5f });
		break;
	}
}

void Item::Update(float deltaTime)
{
	if (!active_) return;

	lifetime_ += deltaTime;
	floatTimer_ += floatSpeed_ * deltaTime;
	const float floatOffset = std::sinf(floatTimer_) * floatAmplitude_;
	position_.y = basePosition_.y + floatOffset + 1.0f;
	rotation_.y += rotationSpeed_;

	if (object3d_)
	{
		object3d_->SetTranslate(position_);
		object3d_->SetRotate(rotation_);
		object3d_->Update();
	}

	K4E::Collider::SetCenterPosition(position_);
}

void Item::Draw()
{
	if (active_ && object3d_)
	{
		object3d_->Draw();
	}
}

bool Item::CheckCollisionWithPlayer(const K4E::Vector3& playerPos) const
{
	if (!active_) return false;
	const K4E::Vector3 diff = position_ - playerPos;
	return K4E::Vector3::Length(diff) <= pickupRadius_;
}

bool Item::OnPickup(Player& player)
{
	if (!active_) return false;

	switch (type_)
	{
	case ItemType::HealSmall:
		player.Heal(static_cast<float>(healAmount_));
		break;
	case ItemType::AmmoSmall:
		player.AddCurrentWeaponAmmo(ammoAmount_);
		break;
	case ItemType::NextStageKey:
	case ItemType::None:
	default:
		break;
	}

	active_ = false;
	return true;
}

void Item::ApplyTo(Player* player)
{
	if (!player) return;
	(void)OnPickup(*player);
}

void Item::OnCollision(K4E::Collider* other)
{
	if (!other) return;

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		ApplyTo(static_cast<Player*>(other));
	}
}
