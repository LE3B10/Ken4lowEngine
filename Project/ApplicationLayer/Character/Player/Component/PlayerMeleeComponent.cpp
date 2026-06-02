#include "PlayerMeleeComponent.h"
#include "PlayerViewComponent.h"
#include "CollisionManager.h"
#include "CollisionTypeIdDef.h"
#include "Collider.h"
#include "EnemyBase.h"
#include "EnemySpawnCrystal.h"
#include "BossBase.h"

namespace
{
	float DistSq(const K4E::Vector3& a, const K4E::Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		const float dz = a.z - b.z;
		return dx * dx + dy * dy + dz * dz;
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

	K4E::Segment seg{};
	seg.origin = start;
	seg.diff = end - start; // Segment定義が origin + diff 形式ならこれでOK

	K4E::Collider* enemyHit = nullptr;
	K4E::Collider* bossHit = nullptr;
	K4E::Collider* crystalHit = nullptr;

	const bool hitEnemy = collisionManager_->SegmentCast(
		static_cast<uint32_t>(CollisionTypeIdDef::kEnemy), seg, &enemyHit);

	const bool hitBoss = collisionManager_->SegmentCast(
		static_cast<uint32_t>(CollisionTypeIdDef::kBoss), seg, &bossHit);

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

	if (bestHit->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
	{
		auto* enemyBase = bestHit->GetOwner<EnemyBase>();
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
		bestHit->GetOwner<EnemySpawnCrystal>()->ApplyDamage(damage_);
		if (onHit_) onHit_();
	}
	else if (bestHit->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
	{
		bestHit->GetOwner<BossBase>()->OnDamaged(static_cast<float>(damage_));
		if (onHit_) onHit_();
	}
}
