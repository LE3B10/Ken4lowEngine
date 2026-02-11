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