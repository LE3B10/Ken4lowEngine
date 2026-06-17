#pragma once

#include "Collider.h"
#include "Vector3.h"

#include <cstdint>

class Player;

class PlayerDamageCollider : public Ken4lowEngine::Collider
{
public:
	void Initialize(Player* owner, uint32_t typeId);
	void SyncFromOwner();
	void DrawImGui();

	void OnCollisionEnter(Ken4lowEngine::Collider* other) override;
	void OnCollisionEnter(const Ken4lowEngine::CollisionHit& hit) override;

private:
	Player* owner_ = nullptr;
	Ken4lowEngine::Vector3 localOffset_{ 0.0f, 0.0f, 0.0f };
	Ken4lowEngine::Vector3 halfSize_{ 0.5f, 1.8f, 0.5f };
};
