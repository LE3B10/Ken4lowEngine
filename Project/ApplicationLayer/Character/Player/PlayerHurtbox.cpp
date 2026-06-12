#include "PlayerHurtbox.h"
#include "Player.h"

void PlayerHurtbox::OnCollisionEnter(Ken4lowEngine::Collider* other)
{
	if (!other) return;
	if (auto* p = GetOwner<Player>())
	{
		p->OnHitByEnemyBullet(other, part_, damageMul_);
	}
}

void PlayerHurtbox::OnCollisionEnter(const Ken4lowEngine::CollisionHit& hit)
{
	// Collision Event Systemの詳細Hit入口からも、部位別被弾処理へ集約する。
	OnCollisionEnter(hit.other);
}
