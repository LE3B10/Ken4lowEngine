#define NOMINMAX
#include "EnemyCombatMoveState.h"
#include "Enemy.h"

#include <algorithm>

using namespace Ken4lowEngine;

void EnemyCombatMoveState::Enter(Enemy& enemy)
{
	losRepositionTimer_ = 0.0f;
	retreatRepathTimer_ = 0.0f;
	coverStayTimer_ = 0.0f;
	hasRetreatTarget_ = false;
	enemy.PlayMoveAnimation(enemy.GetApproachSpeed());
}

void EnemyCombatMoveState::Update(Enemy& enemy, float deltaTime)
{
	if (!enemy.HasTarget())
	{
		enemy.ChangeStateToIdle();
		return;
	}

	const Vector3 targetPos = enemy.GetTargetPosition();
	const float distToTarget = enemy.GetDistanceToTarget();

	if (enemy.CanSeeTargetPublic(targetPos, distToTarget))
	{
		enemy.RememberLastSeenTarget(targetPos);
	}
	else if (enemy.HasLostTarget())
	{
		enemy.ChangeStateToSearch();
		return;
	}

	enemy.UpdateStrafeDecision(deltaTime);
	losRepositionTimer_ -= deltaTime;
	retreatRepathTimer_ -= deltaTime;
	coverStayTimer_ -= deltaTime;

	bool canShoot = enemy.CanShootTargetPublic(targetPos);
	bool canFireNow = canShoot && enemy.CanStartShooting();
	const auto retreatPlan = enemy.EvaluateRetreatPlan(distToTarget, canShoot);
	const auto evadePlan = enemy.EvaluateEvadePlan(canShoot);
	const bool dangerMode = retreatPlan.dangerMode;
	const bool inHitReaction = enemy.IsInHitReaction();

	float radialBias = retreatPlan.radialBias;
	float speed = retreatPlan.speed;
	bool shouldPathChase = retreatPlan.shouldPathChase;
	bool forceRetreat = retreatPlan.forceRetreat;

	if (!canShoot)
	{
		if (losRepositionTimer_ <= 0.0f)
		{
			const float betterSign = enemy.ChooseBetterStrafeSign(targetPos, enemy.GetLosProbeDistance());
			if (betterSign != enemy.GetCurrentStrafeSign())
			{
				enemy.ForceStrafeSign(betterSign);
			}
			losRepositionTimer_ = enemy.GetLosRepositionEvalSec();
		}

		if (distToTarget < enemy.GetMinCombatRange())
		{
			radialBias = std::min(radialBias, -0.85f);
		}
		else
		{
			radialBias = dangerMode ? std::min(radialBias, -0.4f) : std::max(radialBias, -0.1f);
		}
		speed = std::max(speed, enemy.GetStrafeSpeed());
		shouldPathChase = true;
	}

	if (evadePlan.mode == EnemyEvadeController::Mode::Retreat)
	{
		forceRetreat = true;
		radialBias = std::min(radialBias, evadePlan.radialBias);
		speed *= evadePlan.speedScale;
	}

	const bool shouldPrioritizeCover = forceRetreat || (dangerMode && coverStayTimer_ <= 0.0f) || (evadePlan.mode == EnemyEvadeController::Mode::ToCover);
	const bool shouldUseOpportunisticCover =
		!dangerMode &&
		!canShoot &&
		coverStayTimer_ <= 0.0f &&
		distToTarget <= enemy.GetFireRange() * (1.05f + enemy.GetCoverPreference() * 0.2f);
	const bool useCover = shouldPrioritizeCover || shouldUseOpportunisticCover;
	if (useCover)
	{
		if (!hasRetreatTarget_ || retreatRepathTimer_ <= 0.0f)
		{
			Ken4lowEngine::Vector3 cover{};
			const bool preferRetreat = dangerMode || forceRetreat;
			hasRetreatTarget_ = enemy.TryFindCoverPosition(targetPos, preferRetreat, cover);
			if (hasRetreatTarget_)
			{
				retreatTarget_ = cover;
			}

			retreatRepathTimer_ = dangerMode ? enemy.GetRetreatDecisionInterval() : enemy.GetCoverRepathInterval();
		}

		if (hasRetreatTarget_)
		{
			const auto coverAction = enemy.EvaluateCoverAction(targetPos, retreatTarget_, dangerMode, hasRetreatTarget_, deltaTime);
			enemy.MoveTowardsPath(coverAction.moveTarget, speed, deltaTime);
			coverStayTimer_ = enemy.GetCoverStayTime();
			canShoot = canShoot || enemy.ShouldShootFromCover(coverAction);
			canFireNow = canShoot && enemy.CanStartShooting();
		}
		else
		{
			enemy.MoveAwayFrom(targetPos, speed);
			enemy.ResetCoverAction();
		}
	}
	else if (shouldPathChase)
	{
		enemy.ResetCoverAction();
		enemy.MoveTowardsPath(targetPos, speed, deltaTime);
	}
	else
	{
		enemy.ResetCoverAction();
		if (enemy.IsMovementStuck())
		{
			enemy.ForceStrafeSign(-enemy.GetCurrentStrafeSign());
		}
		enemy.MoveTacticalAround(targetPos, enemy.GetCurrentStrafeSign(), radialBias, speed);
	}
	enemy.PlayMoveAnimation(speed);

	const float shootRange = dangerMode ? enemy.GetLowHpShootRange() : enemy.GetMaxCombatRange();
	if (distToTarget >= enemy.GetMinCombatRange() && distToTarget <= shootRange && canFireNow)
	{
		enemy.ChangeStateToShoot();
		return;
	}
}

void EnemyCombatMoveState::Exit(Enemy& enemy)
{
	(void)enemy;
}
