#include "GameplayPhysicsEventHandler.h"

#include "PhysicsTestBullet.h"
#include "Engine/Physics/Collision/Core/Collider.h"
#include "Engine/Physics/Event/PhysicsEvent.h"

#include <sstream>

void GameplayPhysicsEventHandler::Configure(PhysicsTestBullet* bullet, K4E::Collider* targetCollider)
{
	// 既存Bullet/Enemyへ触れず、Physics Trigger確認用の2つのColliderだけを監視する。
	bullet_ = bullet;
	targetCollider_ = targetCollider;
}

void GameplayPhysicsEventHandler::OnPhysicsEvent(const K4E::PhysicsEvent& event)
{
	// TriggerEnterをゲーム側の処理へ変換する。Stayでは連続ヒットさせない。
	if (event.type != K4E::PhysicsEventType::TriggerEnter || !IsBulletTargetPair(event))
	{
		return;
	}

	++triggerEnterCount_;
	hasTriggerHit_ = true;

	std::ostringstream stream;
	stream << "TriggerEnter Bullet=" << bullet_->GetCollider()
		<< " Target=" << targetCollider_
		<< " Count=" << triggerEnterCount_;
	latestTriggerEvent_ = stream.str();

	if (bullet_)
	{
		bullet_->Kill();
	}
}

void GameplayPhysicsEventHandler::Reset()
{
	// Debug表示用の反応状態だけをクリアし、PhysicsWorldの履歴はWorld側の登録解除に任せる。
	triggerEnterCount_ = 0;
	hasTriggerHit_ = false;
	latestTriggerEvent_ = "None";
}

bool GameplayPhysicsEventHandler::IsBulletTargetPair(const K4E::PhysicsEvent& event) const
{
	if (!bullet_ || !targetCollider_)
	{
		return false;
	}

	K4E::Collider* bulletCollider = bullet_->GetCollider();
	return (event.colliderA == bulletCollider && event.colliderB == targetCollider_) ||
		(event.colliderA == targetCollider_ && event.colliderB == bulletCollider);
}
