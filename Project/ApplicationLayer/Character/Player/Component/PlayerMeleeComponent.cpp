#include "PlayerMeleeComponent.h"
#include "PlayerViewComponent.h"
#include "CollisionManager.h"
#include "CollisionTypeIdDef.h"
#include "Collider.h"
#include "EnemyBase.h"
#include "EnemySpawnCrystal.h"
#include "BossBase.h"
#include <LogString.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace
{
	float DistSq(const K4E::Vector3& a, const K4E::Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		const float dz = a.z - b.z;
		return dx * dx + dy * dy + dz * dz;
	}

	float DistancePointToSegmentSq(const K4E::Vector3& point, const K4E::Segment& seg)
	{
		const float lenSq = seg.diff.x * seg.diff.x + seg.diff.y * seg.diff.y + seg.diff.z * seg.diff.z;
		if (lenSq <= 0.0001f)
		{
			return DistSq(point, seg.origin);
		}

		const K4E::Vector3 toPoint = point - seg.origin;
		float t = (toPoint.x * seg.diff.x + toPoint.y * seg.diff.y + toPoint.z * seg.diff.z) / lenSq;
		t = std::clamp(t, 0.0f, 1.0f);

		const K4E::Vector3 nearest = seg.origin + seg.diff * t;
		return DistSq(point, nearest);
	}

	void LogPlayerAttack(const std::string& message)
	{
		Log(message + "\n");
	}
}

void PlayerMeleeComponent::BindDependencies(PlayerViewComponent* view, CollisionManager* collisionManager)
{
	view_ = view;
	collisionManager_ = collisionManager;
}

void PlayerMeleeComponent::StartAttack(const K4E::Vector3& /*playerPos*/)
{
	isAttacking_ = true;
	activeHitDone_ = false;
	timer_ = 0.0f;
	hitColliderIds_.clear();
}

void PlayerMeleeComponent::Tick(float dt, const K4E::Vector3& playerPos)
{
	if (!isAttacking_)
	{
		return;
	}

	timer_ += dt;

	const float startupEnd = startupSec_;
	const float activeEnd = startupSec_ + activeSec_;
	const float finishEnd = startupSec_ + activeSec_ + recoverySec_;

	if (!activeHitDone_ && timer_ >= startupEnd && timer_ < activeEnd)
	{
		EvaluateHit(playerPos);
		activeHitDone_ = true;
	}

	if (timer_ >= finishEnd)
	{
		isAttacking_ = false;
		timer_ = 0.0f;
	}
}

void PlayerMeleeComponent::EvaluateHit(const K4E::Vector3& playerPos)
{
	if (!view_ || !collisionManager_)
	{
		return;
	}

	auto* cam = view_->GetCamera();
	if (!cam)
	{
		return;
	}

	K4E::Vector3 forward = cam->GetForward();
	forward.y = 0.0f;

	const float lenSq = forward.x * forward.x + forward.z * forward.z;
	if (lenSq <= 0.0001f)
	{
		forward = { 0.0f, 0.0f, 1.0f };
	}
	else
	{
		forward = K4E::Vector3::Normalize(forward);
	}

	// 胸あたりから少し前へ
	const K4E::Vector3 start = playerPos + K4E::Vector3{ 0.0f, 1.2f, 0.0f };
	const K4E::Vector3 end = start + forward * range_;
	const K4E::Vector3 attackCenter = start + (end - start) * 0.5f;

	K4E::Segment seg{};
	seg.origin = start;
	seg.diff = end - start; // Segment定義が origin + diff 形式ならこれでOK

	K4E::Collider* enemyHit = nullptr;
	K4E::Collider* bossHit = nullptr;
	K4E::Collider* crystalHit = nullptr;

	const bool hitEnemy = collisionManager_->SegmentCast(
		static_cast<uint32_t>(CollisionTypeIdDef::kEnemy), seg, &enemyHit);

	bool hitBoss = collisionManager_->SegmentCast(
		static_cast<uint32_t>(CollisionTypeIdDef::kBoss), seg, &bossHit);

	int bossCandidateCount = 0;
	const auto& bossColliders = collisionManager_->GetCollidersByType(static_cast<uint32_t>(CollisionTypeIdDef::kBoss));
	for (K4E::Collider* bossCollider : bossColliders)
	{
		if (!bossCollider) continue;

		const K4E::Vector3 halfSize = bossCollider->GetOBBHalfSize();
		const float bossRadius = std::max({ halfSize.x, halfSize.y, halfSize.z });
		const float hitRadius = radius_ + bossRadius;
		if (DistancePointToSegmentSq(bossCollider->GetCenterPosition(), seg) > hitRadius * hitRadius)
		{
			continue;
		}

		++bossCandidateCount;
		if (!bossHit || DistSq(bossCollider->GetCenterPosition(), start) < DistSq(bossHit->GetCenterPosition(), start))
		{
			bossHit = bossCollider;
		}
	}
	hitBoss = hitBoss || bossHit != nullptr;
	{
		std::ostringstream oss;
		oss << "[PlayerAttack] Boss candidate count = " << bossCandidateCount
			<< ", registered = " << bossColliders.size()
			<< ", segmentHit = " << (hitBoss ? "true" : "false");
		LogPlayerAttack(oss.str());
	}

	const bool hitCrystal = collisionManager_->SegmentCast(
		static_cast<uint32_t>(CollisionTypeIdDef::kCrystal), seg, &crystalHit);

	K4E::Collider* bestHit = nullptr;
	float bestDist = 0.0f;
	auto selectNearest = [&](bool hit, K4E::Collider* candidate)
		{
			if (!hit || !candidate) return;
			const float dist = DistSq(candidate->GetCenterPosition(), start);
			if (!bestHit || dist < bestDist)
			{
				bestHit = candidate;
				bestDist = dist;
			}
		};

	selectNearest(hitEnemy, enemyHit);
	selectNearest(hitBoss, bossHit);
	selectNearest(hitCrystal, crystalHit);

	if (!bestHit)
	{
		return;
	}

	if (hitColliderIds_.find(bestHit->GetUniqueID()) != hitColliderIds_.end())
	{
		LogPlayerAttack("[PlayerAttack] Same collider already hit in this attack. Skip.");
		return;
	}

	if (bestHit->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
	{
		auto* enemyBase = bestHit->GetOwner<EnemyBase>();
		if (!enemyBase) return;
		hitColliderIds_.insert(bestHit->GetUniqueID());
		EnemyBase::SetDeathDebugComparePositions(playerPos, attackCenter);
		const bool wasDead = enemyBase->IsDead();

		// 近接ヒット位置
		// 敵中心に少し上オフセットして、さらにプレイヤー前方方向に少し寄せる
		K4E::Vector3 hitFxPos = enemyBase->GetCenterPosition();
		hitFxPos.y += 1.0f;
		hitFxPos.x += forward.x * 0.35f;
		hitFxPos.z += forward.z * 0.35f;

		enemyBase->SpawnHitEffectAt(hitFxPos);

		enemyBase->TakeDamage(damage_, forward, 1.2f + static_cast<float>(damage_) * 0.02f);

		if (!enemyBase->IsDead())
		{
			K4E::Vector3 knock = enemyBase->GetVelocity();
			knock.x = forward.x * 4.0f;
			knock.z = forward.z * 4.0f;
			knock.y = 1.5f;
			enemyBase->SetVelocity(knock);
		}


		if (!wasDead && onHit_)
		{
			onHit_();
		}
	}
	else if (bestHit->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kCrystal))
	{
		// 敵だけでなく、近接攻撃の最寄り対象がクリスタルならHPを減らす。
		auto* crystal = bestHit->GetOwner<EnemySpawnCrystal>();
		if (!crystal) return;
		hitColliderIds_.insert(bestHit->GetUniqueID());
		crystal->ApplyDamage(damage_);
		if (onHit_) onHit_();
	}
	else if (bestHit->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
	{
		// ボス出現後もプレイヤー攻撃対象に含めるため、BossColliderへのヒット判定を追加する。
		auto* boss = bestHit->GetOwner<BossBase>();
		if (!boss || boss->IsDead()) return;
		hitColliderIds_.insert(bestHit->GetUniqueID());
		const BossHitResult bossPartHit = boss->CheckDebugHitSphere(attackCenter, radius_);
		const float bossDamage = bossPartHit.isHit ? static_cast<float>(damage_) * bossPartHit.damageMultiplier : static_cast<float>(damage_);
		{
			std::ostringstream oss;
			oss << "[PlayerAttack] Boss hit damage=" << bossDamage
				<< ", hpBefore=" << boss->GetHP() << "/" << boss->GetMaxHP();
			LogPlayerAttack(oss.str());
		}
		boss->OnDamaged(bossDamage);
		if (onHit_) onHit_();
	}
}
