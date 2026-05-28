#define NOMINMAX
#include "GuardianBoss.h"
#include "Attacks/BossPunchAttack.h"
#include "Attacks/BossHeavyPunchAttack.h"
#include "BehaviorTree/BTActionNode.h"
#include "BehaviorTree/BTConditionNode.h"
#include "BehaviorTree/BTSelectorNode.h"
#include "BehaviorTree/BTSequenceNode.h"
#include "Wireframe.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace {
float WrapAngle(float angle)
{
	while (angle > 3.14159265f) { angle -= 6.28318530f; }
	while (angle < -3.14159265f) { angle += 6.28318530f; }
	return angle;
}

float LengthXZ(const Vector3& v)
{
	return std::sqrt(v.x * v.x + v.z * v.z);
}
}

void GuardianBoss::SetupBoss()
{
	HumanoidBossBase::SetupBoss();
	SetPhase(BossPhase::Phase1);
	runtime_ = {};
	chargeTimer_ = 0.0f;
	ChangeBossState(BossState::Idle);
	BuildBehaviorTree(); // 平原ボスの意思決定はBTに集約する。
	ApplySkinToAllParts("Characters/zombie.dds");
}

void GuardianBoss::OnDamaged(float damage)
{
	if (GetState() == BossState::Dead) { return; }
	BossBase::OnDamaged(damage);
}

void GuardianBoss::OnDead() { ChangeBossState(BossState::Dead); }
void GuardianBoss::OnCollision(Collider* other) { (void)other; }

void GuardianBoss::Draw()
{
	BossBase::Draw();
	UpdateNavigationObstacleList();

	if (obstacleDebugDrawEnabled_)
	{
		for (const auto& obstacle : pathBlockingObstacleAABBs_)
		{
			Wireframe::GetInstance()->DrawAABB(obstacle, { 1.0f, 0.2f, 0.2f, 0.35f });
		}

		for (const auto& inflated : navigator_.GetInflatedObstacleAABBs())
		{
			Wireframe::GetInstance()->DrawAABB(inflated, { 1.0f, 0.7f, 0.2f, 0.35f });
		}
	}

	if (pathDebugDrawEnabled_)
	{
		const auto& path = navigator_.GetCurrentPath();
		for (size_t i = 1; i < path.size(); ++i)
		{
			Wireframe::GetInstance()->DrawLine(
				path[i - 1] + Vector3{ 0.0f, 0.15f, 0.0f },
				path[i] + Vector3{ 0.0f, 0.15f, 0.0f },
				{ 0.2f, 0.8f, 1.0f, 1.0f });
		}

		Wireframe::GetInstance()->DrawSphere(
			pathState_.currentWaypoint + Vector3{ 0.0f, 0.2f, 0.0f },
			0.4f,
			{ 1.0f, 0.3f, 0.1f, 1.0f });

		if (pathState_.lineBlocked)
		{
			Wireframe::GetInstance()->DrawLine(
				pathState_.blockedSegmentFrom + Vector3{ 0.0f, 0.25f, 0.0f },
				pathState_.blockedSegmentTo + Vector3{ 0.0f, 0.25f, 0.0f },
				{ 1.0f, 0.1f, 0.1f, 1.0f });
		}
	}
}

void GuardianBoss::UpdateState(float deltaTime)
{
	runtime_.stateTimer += deltaTime;
	runtime_.attackCooldownTimer = std::max(0.0f, runtime_.attackCooldownTimer - deltaTime);
	chargeTimer_ = std::max(0.0f, chargeTimer_ - deltaTime);
	CheckDeath();
	if (GetState() == BossState::Dead) { return; }
	UpdatePhaseTransition();
	FaceTarget(deltaTime);
	TickBehaviorTree(deltaTime);
}

void GuardianBoss::UpdateMovement(float deltaTime)
{
	if (GetState() != BossState::Move && runtime_.currentActionName != "Charge") { return; }
	UpdateStuckState(deltaTime);
	if (MoveAlongPath(deltaTime))
	{
		runtime_.walkAnimTimer += deltaTime;
	}
}

void GuardianBoss::UpdateAttack(float deltaTime) { BossBase::UpdateAttack(deltaTime); (void)deltaTime; }
void GuardianBoss::CheckDeath() { if (IsDead() && GetState() != BossState::Dead) { OnDead(); } }
void GuardianBoss::SetupAttacks() { RegisterAttack(std::make_unique<BossPunchAttack>()); RegisterAttack(std::make_unique<BossHeavyPunchAttack>()); }
void GuardianBoss::SetupPhaseData() {}
void GuardianBoss::SetupWeakPoints() {}

void GuardianBoss::FaceTarget(float deltaTime)
{
	Vector3 to{ GetTargetPosition().x - GetPosition().x, 0.0f, GetTargetPosition().z - GetPosition().z };
	const float lenSq = to.x * to.x + to.z * to.z;
	if (lenSq <= 0.0001f) { return; }
	const float desiredYaw = std::atan2(-to.x, to.z);
	float currentYaw = GetYaw();
	float diff = WrapAngle(desiredYaw - currentYaw);
	const float maxStep = rotateSpeed_ * deltaTime;
	diff = std::clamp(diff, -maxStep, maxStep);
	SetYaw(currentYaw + diff);
}

void GuardianBoss::ChangeBossState(BossState newState)
{
	if (GetStateMachine()) { GetStateMachine()->ChangeState(*this, newState); }
	else { SetState(newState); }
}

void GuardianBoss::EnterIdle() { ChangeBossState(BossState::Idle); runtime_.stateTimer = 0.0f; runtime_.currentActionName = "Idle"; }
void GuardianBoss::EnterMove() { ChangeBossState(BossState::Move); runtime_.stateTimer = 0.0f; runtime_.currentActionName = "Move"; }
void GuardianBoss::EnterAttack(const char* actionName) { ChangeBossState(BossState::Attack); runtime_.stateTimer = 0.0f; runtime_.currentActionName = actionName; }

void GuardianBoss::UpdatePhaseTransition()
{
	if (!runtime_.isPhase2 && GetHPRate() <= phaseSettings_.phase2HpRate)
	{
		runtime_.isPhase2 = true;
		SetPhase(BossPhase::Phase2);
	}
}

bool GuardianBoss::StartAttackByDistance()
{
	if (!GetAttackComponent() || GetAttackComponent()->IsAttacking()) { return false; }
	const float d = GetDistanceToTargetXZ();
	if (d <= attackSettings_.meleeRange)
	{
		if (GetAttackComponent()->StartAttackByName("HeavyPunch") || GetAttackComponent()->StartAttackByName("Punch"))
		{ EnterAttack("StompSmash"); return true; }
	}
	if (d <= attackSettings_.chargeRange && chargeTimer_ <= 0.0f)
	{
		EnterAttack("Charge");
		runtime_.attackCooldownTimer = attackSettings_.chargeCooldown;
		chargeTimer_ = runtime_.isPhase2 ? 1.2f : 0.8f;
		return true;
	}
	if (d <= attackSettings_.shockwaveRange)
	{
		if (GetAttackComponent()->StartAttackByName("Punch")) { EnterAttack("Shockwave"); return true; }
	}
	return false;
}

BTNodeResult GuardianBoss::TickBehaviorTree(float deltaTime)
{
	if (!behaviorRoot_) { return BTNodeResult::Failure; }
	return behaviorRoot_->Tick(deltaTime);
}

void GuardianBoss::BuildBehaviorTree()
{
	auto root = std::make_unique<BTSelectorNode>();
	auto attackSeq = std::make_unique<BTSequenceNode>();
	attackSeq->AddChild(std::make_unique<BTConditionNode>([this]() { return runtime_.attackCooldownTimer <= 0.0f; }));
	attackSeq->AddChild(std::make_unique<BTConditionNode>([this]() { return GetState() != BossState::Attack; }));
	attackSeq->AddChild(std::make_unique<BTActionNode>([this](float) { return StartAttackByDistance() ? BTNodeResult::Success : BTNodeResult::Failure; }));
	root->AddChild(std::move(attackSeq));
	root->AddChild(std::make_unique<BTActionNode>([this](float) {
		const float d = GetDistanceToTargetXZ();
		if (runtime_.currentActionName == "Charge")
		{
			if (chargeTimer_ <= 0.0f) { runtime_.attackCooldownTimer = attackSettings_.attackCooldown * (runtime_.isPhase2 ? phaseSettings_.phase2CooldownMultiplier : 1.0f); EnterIdle(); }
			return BTNodeResult::Running;
		}
		if (GetState() == BossState::Attack)
		{
			if (GetAttackComponent() && !GetAttackComponent()->IsAttacking()) { runtime_.attackCooldownTimer = attackSettings_.attackCooldown * (runtime_.isPhase2 ? phaseSettings_.phase2CooldownMultiplier : 1.0f); EnterIdle(); }
			return BTNodeResult::Running;
		}
		if (d > attackSettings_.moveStartDistance) { EnterMove(); }
		else if (d <= attackSettings_.moveStopDistance) { EnterIdle(); }
		return BTNodeResult::Success;
	}));
	behaviorRoot_ = std::move(root);
}

float GuardianBoss::GetCurrentMoveSpeed() const
{
	const float base = 3.2f;
	return runtime_.isPhase2 ? base * phaseSettings_.phase2MoveSpeedMultiplier : base;
}

void GuardianBoss::DrawImGui()
{
#ifdef USE_IMGUI
	UpdateNavigationObstacleList();

	if (ImGui::Begin("PlainsGuardianBoss"))
	{
		BossBase::DrawImGui();
		if (ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("State: %d", static_cast<int>(GetState()));
			ImGui::Text("Phase2: %s", runtime_.isPhase2 ? "true" : "false");
			ImGui::Text("Action: %s", runtime_.currentActionName.c_str());
			ImGui::Text("HP: %.1f/%.1f", GetHP(), GetMaxHP());
			ImGui::Text("Cooldown: %.2f", runtime_.attackCooldownTimer);
		}
		if (ImGui::CollapsingHeader("移動・フェーズ", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("旋回速度", &rotateSpeed_, 0.1f, 20.0f);
			ImGui::SliderFloat("フェーズ2移動倍率", &phaseSettings_.phase2MoveSpeedMultiplier, 0.5f, 2.5f);
			ImGui::SliderFloat("フェーズ2移行HP比率", &phaseSettings_.phase2HpRate, 0.1f, 0.9f);
			ImGui::SliderFloat("フェーズ2クールダウン倍率", &phaseSettings_.phase2CooldownMultiplier, 0.1f, 1.0f);
		}
		if (ImGui::CollapsingHeader("攻撃設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("近接攻撃距離", &attackSettings_.meleeRange, 0.5f, 20.0f);
			ImGui::SliderFloat("突進距離", &attackSettings_.chargeRange, 0.5f, 30.0f);
			ImGui::SliderFloat("衝撃波距離", &attackSettings_.shockwaveRange, 0.5f, 40.0f);
			ImGui::SliderFloat("追跡開始距離", &attackSettings_.moveStartDistance, 0.5f, 20.0f);
			ImGui::SliderFloat("追跡停止距離", &attackSettings_.moveStopDistance, 0.5f, 20.0f);
			ImGui::SliderFloat("通常攻撃CT", &attackSettings_.attackCooldown, 0.0f, 8.0f);
			ImGui::SliderFloat("突進CT", &attackSettings_.chargeCooldown, 0.0f, 8.0f);
			ImGui::SliderFloat("衝撃波CT", &attackSettings_.shockwaveCooldown, 0.0f, 8.0f);
		}
		if (ImGui::CollapsingHeader("経路探索", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("経路探索を使う", &pathSettings_.enabled);
			ImGui::SliderFloat("再探索間隔", &pathSettings_.repathInterval, 0.05f, 2.0f);
			ImGui::SliderFloat("到達判定距離", &pathSettings_.waypointReachDistance, 0.5f, 2.5f);
			ImGui::SliderFloat("グリッドサイズ", &pathSettings_.gridSize, 0.5f, 2.5f);
			ImGui::SliderFloat("探索半径", &pathSettings_.searchRadius, 6.0f, 120.0f);
			ImGui::SliderFloat("障害物拡張半径", &pathSettings_.obstacleExpandRadius, 0.1f, 3.0f);
			ImGui::SliderFloat("一時ブロック時間", &pathSettings_.temporaryBlockDuration, 0.2f, 6.0f);
			ImGui::SliderFloat("一時ブロック半径", &pathSettings_.temporaryBlockRadius, 0.2f, 3.0f);
			ImGui::Checkbox("角抜け無効", &pathSettings_.cornerCuttingDisabled);
			ImGui::SliderFloat("再探索ターゲット閾値", &pathSettings_.targetRepathThreshold, 0.1f, 10.0f);
			ImGui::SliderFloat("スタック再探索拡張", &pathSettings_.stuckRepathExpandBonus, 0.0f, 4.0f);
			ImGui::SliderFloat("スタック再探索拡張最大", &pathSettings_.maxStuckRepathExpandBonus, 0.0f, 8.0f);
			ImGui::Checkbox("経路デバッグ描画", &pathDebugDrawEnabled_);
			ImGui::Checkbox("障害物デバッグ描画", &obstacleDebugDrawEnabled_);
			ImGui::Text("元の障害物AABB数: %d", pathState_.sourceObstacleAABBCount);
			ImGui::Text("A*へ渡す障害物AABB数: %d", pathState_.blockingObstacleAABBCount);
			ImGui::Text("経路あり: %s", pathState_.found ? "はい" : "いいえ");
			ImGui::Text("失敗理由: %s", pathState_.failureReason.c_str());
			ImGui::Text("最後のリパス理由: %s", pathState_.lastRepathReason.c_str());
			ImGui::Text("LineBlocked: %s", pathState_.lineBlocked ? "はい" : "いいえ");
			ImGui::Text("BlockedObstacle: %s", pathState_.blockedObstacleName.c_str());
		}
	}
	ImGui::End();
#endif
}


void GuardianBoss::UpdateNavigationObstacleList()
{
	pathBlockingObstacleAABBs_.clear();

	const auto* source = wallObstacleAABBs_;
	pathState_.sourceObstacleAABBCount = source ? static_cast<int>(source->size()) : 0;

	if (!source)
	{
		pathState_.blockingObstacleAABBCount = 0;
		return;
	}

	const Vector3 bossPos = GetPosition();

	for (const auto& aabb : *source)
	{
		const float sizeX = aabb.max.x - aabb.min.x;
		const float sizeY = aabb.max.y - aabb.min.y;
		const float sizeZ = aabb.max.z - aabb.min.z;

		// 床のように薄いAABBは経路障害物から除外し、壁や柱だけをA*へ渡す。
		if (sizeY <= 0.05f)
		{
			continue;
		}

		// ボスの足元より大きく下にあるAABBは、経路探索の障害物から除外する。
		if (aabb.max.y < bossPos.y - 1.5f)
		{
			continue;
		}

		// 極端に小さいAABBはノイズとして除外し、経路探索の誤反応を減らす。
		if (sizeX <= 0.05f || sizeZ <= 0.05f)
		{
			continue;
		}

		pathBlockingObstacleAABBs_.push_back(aabb);
	}

	pathState_.blockingObstacleAABBCount = static_cast<int>(pathBlockingObstacleAABBs_.size());
}

void GuardianBoss::StopMove()
{
	// BossBaseには速度APIがないため、停止状態は移動フラグだけで管理する。
	movementVelocity_ = Vector3{};
	runtime_.isMoving = false;
	pathState_.currentWaypoint = GetPosition();
}

bool GuardianBoss::MoveAlongPath(float deltaTime)
{
	if (!pathSettings_.enabled)
	{
		StopMove();
		return false;
	}

	const Vector3 currentPos = GetPosition();
	const Vector3 targetPos = GetTargetPosition();

	UpdateNavigationObstacleList();

	EnemyAStarNavigator::Settings settings = navigator_.GetSettings();
	settings.cellSize = pathSettings_.gridSize;
	settings.agentRadius = pathSettings_.obstacleExpandRadius + pathSettings_.stuckRepathExpandBonus;
	settings.searchRangeCells = static_cast<int>(pathSettings_.searchRadius);
	settings.repathIntervalSec = pathSettings_.repathInterval;
	settings.waypointReachDistance = pathSettings_.waypointReachDistance;
	settings.disableCornerCutting = pathSettings_.cornerCuttingDisabled;
	navigator_.SetSettings(settings);

	// MeleeEnemyと同じく、分類済みの回避対象AABBだけをnavigatorへ渡す。
	navigator_.SetWorldAABBs(&pathBlockingObstacleAABBs_);
	navigator_.TickTemporaryBlocks(deltaTime);
	pathState_.lastRepathTimer += deltaTime;

	pathState_.targetMovedDistanceForRepath = LengthXZ(targetPos - pathState_.lastPathTargetPos);
	if (pathState_.targetMovedDistanceForRepath >= pathSettings_.targetRepathThreshold || isStuck_)
	{
		navigator_.Reset();
		pathState_.lastRepathReason = isStuck_ ? "StuckForceRepath" : "TargetMoved";
	}

	Vector3 waypoint = targetPos;
	if (!navigator_.GetNextWaypoint(currentPos, targetPos, currentPos.y, deltaTime, waypoint))
	{
		pathState_.found = false;
		pathState_.failureReason = "PathNotFound";
		pathState_.retryTimer += deltaTime;
		pathState_.failedWaitTimer = std::max(0.0f, pathState_.failedWaitTimer - deltaTime);

		if (pathState_.failedWaitTimer > 0.0f)
		{
			pathState_.lastRepathReason = "PathFailedWait";
			StopMove();
			return false;
		}

		if (pathState_.retryTimer >= pathSettings_.repathInterval)
		{
			navigator_.Reset();
			pathState_.retryTimer = 0.0f;
			pathState_.lastRepathReason = "RetryAfterFailure";
		}

		StopMove();
		return false;
	}

	pathState_.found = true;
	pathState_.failureReason = "None";
	pathState_.currentWaypoint = waypoint;
	pathState_.lastPathTargetPos = targetPos;
	pathState_.retryTimer = 0.0f;
	pathState_.lineBlocked = false;
	pathState_.blockedWaypointIndex = -1;
	pathState_.blockedObstacleName = "None";

	int blockedIdx = -1;
	if (navigator_.IsSegmentBlockedByObstacle(currentPos, waypoint, currentPos.y, &blockedIdx))
	{
		pathState_.lineBlocked = true;
		pathState_.blockedWaypointIndex = navigator_.GetCurrentPathIndex();
		pathState_.blockedObstacleName = (blockedIdx >= 0) ? ("Obstacle[" + std::to_string(blockedIdx) + "]") : "Unknown";
		pathState_.blockedSegmentFrom = currentPos;
		pathState_.blockedSegmentTo = waypoint;

		navigator_.Reset();
		navigator_.AddTemporaryBlockedArea(
			waypoint,
			pathSettings_.temporaryBlockRadius,
			pathSettings_.temporaryBlockDuration,
			"BossWaypointSegmentBlocked");

		pathState_.lastRepathReason = "WaypointSegmentBlocked";
		StopMove();
		return false;
	}

	Vector3 dir = waypoint - currentPos;
	dir.y = 0.0f;
	const float dirLen = LengthXZ(dir);
	if (dirLen <= 0.0001f)
	{
		StopMove();
		return false;
	}
	dir.x /= dirLen;
	dir.z /= dirLen;

	const float moveSpeed = (runtime_.currentActionName == "Charge") ? GetCurrentMoveSpeed() * 2.3f : GetCurrentMoveSpeed();
	Vector3 nextPos = currentPos;
	nextPos.x += dir.x * moveSpeed * deltaTime;
	nextPos.z += dir.z * moveSpeed * deltaTime;
	SetPosition(nextPos);

	if (!TryLandOnObstacleTop(deltaTime))
	{
		ResolveObstaclePenetrationXZ(deltaTime);
	}

	runtime_.isMoving = true;
	runtime_.lastMoveDirection = dir;
	movementVelocity_ = Vector3{ dir.x * moveSpeed, 0.0f, dir.z * moveSpeed };
	if (runtime_.currentActionName != "Charge")
	{
		runtime_.currentActionName = "Chase";
	}

	return true;
}

void GuardianBoss::UpdateStuckState(float deltaTime)
{
	stuckTimer_ += deltaTime;
	if (stuckTimer_ < 0.8f) { return; }
	const float moved = LengthXZ(GetPosition() - pathState_.lastStuckCheckPosition);
	isStuck_ = (GetState() == BossState::Move) && moved <= 0.2f && GetDistanceToTargetXZ() > attackSettings_.moveStopDistance;
	pathState_.lastMovedDistance = moved;
	if (isStuck_)
	{
		navigator_.AddTemporaryBlockedArea(GetPosition(), pathSettings_.temporaryBlockRadius, pathSettings_.temporaryBlockDuration, "StuckPosition");
		navigator_.Reset();
		pathSettings_.stuckRepathExpandBonus = std::min(pathSettings_.stuckRepathExpandBonus + 0.1f, pathSettings_.maxStuckRepathExpandBonus);
	}
	else
	{
		pathSettings_.stuckRepathExpandBonus = std::max(0.0f, pathSettings_.stuckRepathExpandBonus - 0.05f);
	}
	pathState_.lastStuckCheckPosition = GetPosition();
	stuckTimer_ = 0.0f;
}

bool GuardianBoss::TryLandOnObstacleTop(float)
{
	return false;
}

bool GuardianBoss::ResolveObstaclePenetrationXZ(float deltaTime)
{
	(void)deltaTime;

	if (pathBlockingObstacleAABBs_.empty())
	{
		return false;
	}

	Vector3 pos = GetPosition();
	const Vector3 half{ 1.8f, 3.0f, 1.8f };

	bool resolved = false;

	for (const auto& o : pathBlockingObstacleAABBs_)
	{
		if (pos.y + half.y < o.min.y || pos.y - half.y > o.max.y)
		{
			continue;
		}

		const float overlapX = std::min(pos.x + half.x, o.max.x) - std::max(pos.x - half.x, o.min.x);
		const float overlapZ = std::min(pos.z + half.z, o.max.z) - std::max(pos.z - half.z, o.min.z);

		if (overlapX <= 0.0f || overlapZ <= 0.0f)
		{
			continue;
		}

		float pushX = 0.0f;
		float pushZ = 0.0f;

		if (overlapX < overlapZ)
		{
			pushX = pos.x < (o.min.x + o.max.x) * 0.5f ? -overlapX : overlapX;
		}
		else
		{
			pushZ = pos.z < (o.min.z + o.max.z) * 0.5f ? -overlapZ : overlapZ;
		}

		const float maxPush = 0.6f;
		pushX = std::clamp(pushX, -maxPush, maxPush);
		pushZ = std::clamp(pushZ, -maxPush, maxPush);

		pos.x += pushX;
		pos.z += pushZ;

		resolved = true;
	}

	if (resolved)
	{
		// 直接移動で障害物へめり込んだ場合に押し戻し、次フレームで再経路探索させる。
		SetPosition(pos);
		navigator_.Reset();
		pathState_.lastRepathReason = "BossObstaclePushResolve";
	}

	return resolved;
}
