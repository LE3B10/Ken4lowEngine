#include "EnemyShootState.h"
#include "Enemy.h"

using namespace Ken4lowEngine;

void EnemyShootState::Enter(Enemy& enemy)
{
	stayTimer_ = 0.0f;
	reevalTimer_ = 0.0f;
	enemy.ResetCoverAction();
	enemy.StopMove();
	enemy.PlayShootAnimation();
}

void EnemyShootState::Update(Enemy& enemy, float deltaTime)
{
	if (!enemy.HasTarget())
	{
		enemy.ChangeStateToIdle();
		return;
	}

	stayTimer_ += deltaTime;
	reevalTimer_ -= deltaTime;

	const Vector3 targetPos = enemy.GetTargetPosition();
	const float distToTarget = enemy.GetDistanceToTarget();
	const bool inHitReaction = enemy.IsInHitReaction();
	const bool lowHp = enemy.IsRetreating();
	const bool canSee = enemy.CanSeeTargetPublic(targetPos, distToTarget);

	if (canSee)
	{
		enemy.RememberLastSeenTarget(targetPos);
	}
	else if (enemy.HasLostTarget())
	{
		enemy.ChangeStateToSearch();
		return;
	}

	// 低HP時は早めに離脱。単発被弾だけでは即撤退しない。
	if ((inHitReaction && enemy.GetConsecutiveHitCount() >= 2 && stayTimer_ > 0.05f) ||
		(lowHp && distToTarget < enemy.GetLowHpRetreatDistance()))
	{
		enemy.ChangeStateToCombatMove();
		return;
	}

	if (reevalTimer_ <= 0.0f)
	{
		reevalTimer_ = enemy.GetShootRepositionEvalSec();
		const float allowedShootRange = lowHp ? enemy.GetLowHpShootRange() : enemy.GetFireRange();
		const float stayLimit = lowHp ? enemy.GetLowHpShootStaySec() : enemy.GetShootMaxStaySec();

		const bool canShootNow = enemy.CanShootTargetPublic(targetPos);
		if (inHitReaction || distToTarget > allowedShootRange || !canShootNow)
		{
			enemy.ChangeStateToCombatMove();
			return;
		}

		if (stayTimer_ > stayLimit)
		{
			enemy.ChangeStateToCombatMove();
			return;
		}
	}

	// 射撃中も微移動して棒立ち感を減らす
	enemy.UpdateStrafeDecision(deltaTime);
	const float retreatBias = (enemy.IsLowHp() || inHitReaction) ? -0.75f : 0.0f;
	const float reactionScale = inHitReaction ? (1.0f + enemy.GetHitReactionMoveWeight() * 0.5f) : 1.0f;
	const float moveSpeed = (enemy.IsLowHp() || inHitReaction)
		? enemy.GetShootMicroStrafeSpeed() * enemy.GetLowHpRetreatSpeedScale() * reactionScale
		: enemy.GetShootMicroStrafeSpeed();
	enemy.MoveTacticalAround(targetPos, enemy.GetCurrentStrafeSign(), retreatBias, moveSpeed);
	enemy.FaceTo(targetPos);
	enemy.PlayShootAnimation();
	if (!inHitReaction) enemy.FireAt(targetPos);
}

void EnemyShootState::Exit(Enemy& enemy)
{
	(void)enemy;
}
