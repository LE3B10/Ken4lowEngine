#include "MidRangeBombProjectile.h"
#include "Collider.h"
#include "Wireframe.h"
#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	float LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}
}

void MidRangeBombProjectile::Launch(const Vector3& startPosition, const Vector3& targetPosition, const BombProjectileSettings& settings, Collider* target)
{
	// 追加: 投擲開始時に弾の初期状態をリセットする。
	position_ = startPosition;
	settings_ = settings;
	// ビルド優先: owner参照は一旦持たない。
	target_ = target;
	lifeTimer_ = 0.0f;
	exploded_ = false;
	alive_ = true;
	directHitEvent_ = false;
	explosionEvent_ = false;
	debugLastReason_ = "投擲中";

	Vector3 toTarget = targetPosition - startPosition;
	toTarget.y = 0.0f;
	const float len = LengthXZ(toTarget);
	Vector3 dir = len > 0.0001f ? Vector3{ toTarget.x / len, 0.0f, toTarget.z / len } : Vector3{ 0.0f, 0.0f, 1.0f };
	velocity_ = dir * settings_.initialSpeed;
	velocity_.y = settings_.upwardVelocity;
}

void MidRangeBombProjectile::Update(float deltaTime, const std::vector<AABB>* floorAabbs, const std::vector<AABB>* obstacleAabbs)
{
	if (!alive_ || exploded_)
	{
		return;
	}

	lifeTimer_ += deltaTime;
	if (lifeTimer_ >= settings_.lifeTime)
	{
		Explode("寿命");
		return;
	}

	// 追加: 放物線運動で爆弾の位置を更新する。
	velocity_.y -= settings_.gravity * deltaTime;
	position_ += velocity_ * deltaTime;

	if (target_)
	{
		const Vector3 toTarget = target_->GetCenterPosition() - position_;
		const float sqDist = toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z;
		if (sqDist <= settings_.hitRadius * settings_.hitRadius)
		{
			directHitEvent_ = true;
			Explode("プレイヤー直撃");
			return;
		}
	}

	if (CheckAabbHit(floorAabbs) || CheckAabbHit(obstacleAabbs))
	{
		Explode("床/障害物着弾");
		return;
	}
}

void MidRangeBombProjectile::DrawDebug() const
{
	if (!alive_)
	{
		return;
	}
	// 追加: 爆弾本体と爆発半径のデバッグ球を描画する。
	Wireframe::GetInstance()->DrawSphere(position_, settings_.hitRadius, { 0.9f, 0.7f, 0.1f, 1.0f });
	if (exploded_)
	{
		Wireframe::GetInstance()->DrawSphere(position_, settings_.explosionRadius, { 1.0f, 0.3f, 0.2f, 0.35f });
	}
}

bool MidRangeBombProjectile::IsAlive() const { return alive_; }
bool MidRangeBombProjectile::IsExploded() const { return exploded_; }
const char* MidRangeBombProjectile::GetDebugLastReason() const { return debugLastReason_.c_str(); }
const Vector3& MidRangeBombProjectile::GetPosition() const { return position_; }
float MidRangeBombProjectile::GetExplosionRadius() const { return settings_.explosionRadius; }

bool MidRangeBombProjectile::ConsumeDirectHitEvent() { const bool v = directHitEvent_; directHitEvent_ = false; return v; }
bool MidRangeBombProjectile::ConsumeExplosionEvent() { const bool v = explosionEvent_; explosionEvent_ = false; return v; }

void MidRangeBombProjectile::Explode(const char* reason)
{
	exploded_ = true;
	alive_ = true;
	explosionEvent_ = true;
	debugLastReason_ = reason;
}

bool MidRangeBombProjectile::CheckAabbHit(const std::vector<AABB>* aabbs) const
{
	if (!aabbs)
	{
		return false;
	}
	for (const auto& aabb : *aabbs)
	{
		const bool inside =
			position_.x >= aabb.min.x && position_.x <= aabb.max.x &&
			position_.y >= aabb.min.y && position_.y <= aabb.max.y &&
			position_.z >= aabb.min.z && position_.z <= aabb.max.z;
		if (inside)
		{
			return true;
		}
	}
	return false;
}
