#pragma once
#include <cstdint>

#include "CollisionTypes.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	class Collider;
}

namespace K4E = ::Ken4lowEngine;

/// Raycast / SegmentCast / Sweepなどの単発問い合わせ結果を保持する。
struct CollisionHitResult
{
	bool hit = false;
	K4E::Vector3 point{};  // 現段階で正確な交点が取れない問い合わせは近似点を入れる。
	K4E::Vector3 normal{}; // 法線はPrimitive判定が返せる段階で埋める。
	float distance = 0.0f;
	K4E::Collider* collider = nullptr;
	uint32_t typeId = 0u;
	EObjectChannel objectChannel = EObjectChannel::Default;
	ECollisionResponse response = ECollisionResponse::Ignore;
};
