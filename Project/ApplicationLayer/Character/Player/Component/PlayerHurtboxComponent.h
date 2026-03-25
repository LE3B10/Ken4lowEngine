#pragma once

#include "PlayerHurtbox.h"

#include <array>
#include <memory>

namespace K4E = ::Ken4lowEngine;
class Player;
class CollisionManager;

struct HurtboxTuning
{
	K4E::Vector3 localOffset{ 0,0,0 };
	K4E::Vector3 halfSize{ 0.2f,0.2f,0.2f };
	K4E::Vector3 rotOffset{ 0,0,0 };
	float damageMul = 1.0f;
	bool enabled = true;
};

class PlayerHurtboxComponent
{
public:
	static constexpr int kCount = 6;

	void Initialize(Player& owner, CollisionManager* collisionManager);
	void Sync(Player& owner);
	void DrawImGui();

	std::array<std::unique_ptr<PlayerHurtbox>, kCount>& GetBoxes() { return hurtboxes_; }
	const std::array<std::unique_ptr<PlayerHurtbox>, kCount>& GetBoxes() const { return hurtboxes_; }

private:
	std::array<std::unique_ptr<PlayerHurtbox>, kCount> hurtboxes_{};
	std::array<HurtboxTuning, kCount> tuning_{};
	int selected_ = 0;
	bool debugDraw_ = true;
};
