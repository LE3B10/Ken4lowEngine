#pragma once
#include "Collider.h"
#include <cstdint>

class Player;

enum class PlayerHitPart : uint8_t
{
	Body,
	Head,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg,
};

class PlayerHurtbox : public Ken4lowEngine::Collider
{
public:
	void Initialize(Player* owner, PlayerHitPart part, float damageMul, uint32_t typeId)
	{
		SetOwner(owner);
		SetTypeID(typeId);
		part_ = part;
		damageMul_ = damageMul;
	}

	PlayerHitPart GetPart() const { return part_; }
	float GetDamageMul() const { return damageMul_; }

	void OnCollisionEnter(Ken4lowEngine::Collider* other) override;

private:
	PlayerHitPart part_{ PlayerHitPart::Body };
	float damageMul_ = 1.0f;
};