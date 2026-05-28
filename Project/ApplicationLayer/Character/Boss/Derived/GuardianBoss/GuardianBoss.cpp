#define NOMINMAX
#include "GuardianBoss.h"
#include "Attacks/BossPunchAttack.h"
#include "Attacks/BossHeavyPunchAttack.h"
#include "BehaviorTree/BTActionNode.h"
#include "BehaviorTree/BTConditionNode.h"
#include "BehaviorTree/BTSelectorNode.h"
#include "BehaviorTree/BTSequenceNode.h"

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
	const float speed = (runtime_.currentActionName == "Charge") ? GetCurrentMoveSpeed() * 2.3f : GetCurrentMoveSpeed();
	if (!MoveAlongPath(deltaTime))
	{
		Vector3 to{ GetTargetPosition().x - GetPosition().x, 0.0f, GetTargetPosition().z - GetPosition().z };
		const float lenSq = to.x * to.x + to.z * to.z;
		if (lenSq <= 0.0001f) { return; }
		const float len = std::sqrt(lenSq);
		to.x /= len; to.z /= len;
		movementVelocity_ = Vector3{ to.x * speed, 0.0f, to.z * speed };
		runtime_.isMoving = true;
		runtime_.lastMoveDirection = Vector3{ to.x, 0.0f, to.z };
	}
	Vector3 nextPos = GetPosition();
	nextPos.x += movementVelocity_.x * deltaTime;
	nextPos.z += movementVelocity_.z * deltaTime;
	runtime_.walkAnimTimer += deltaTime;
	SetPosition(nextPos);
	const bool landedOnObstacleTop = TryLandOnObstacleTop(deltaTime);
	if (!landedOnObstacleTop) { ResolveObstaclePenetrationXZ(deltaTime); }
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
			const int sourceObstacleCount = wallObstacleAABBs_ ? static_cast<int>(wallObstacleAABBs_->size()) : 0;
			ImGui::Text("navigatorに渡している障害物数: %d", static_cast<int>(pathBlockingObstacleAABBs_.size()));
			ImGui::Text("元の障害物数: %d", sourceObstacleCount);
			ImGui::Text("経路発見: %s", pathState_.found ? "true" : "false");
			ImGui::Text("失敗理由: %s", pathState_.failureReason.c_str());
			ImGui::Text("再探索理由: %s", pathState_.lastRepathReason.c_str());
		}
	}
	ImGui::End();
#endif
}


bool GuardianBoss::MoveAlongPath(float deltaTime)
{
	if (!pathSettings_.enabled)
	{
		movementVelocity_ = Vector3{};
		runtime_.isMoving = false;
		return false;
	}
	EnemyAStarNavigator::Settings s = navigator_.GetSettings();
	s.cellSize = pathSettings_.gridSize;
	s.agentRadius = pathSettings_.obstacleExpandRadius + pathSettings_.stuckRepathExpandBonus;
	s.searchRangeCells = static_cast<int>(pathSettings_.searchRadius);
	s.repathIntervalSec = pathSettings_.repathInterval;
	s.waypointReachDistance = pathSettings_.waypointReachDistance;
	s.disableCornerCutting = pathSettings_.cornerCuttingDisabled;
	navigator_.SetSettings(s);
	const std::vector<K4E::AABB>* obstacleAabbs = wallObstacleAABBs_;
	if (obstacleAabbs) { pathBlockingObstacleAABBs_ = *obstacleAabbs; }
	else { pathBlockingObstacleAABBs_.clear(); }
	navigator_.SetWorldAABBs(&pathBlockingObstacleAABBs_);
	pathState_.lastRepathTimer += deltaTime;
	navigator_.TickTemporaryBlocks(deltaTime);
	const Vector3 targetPos = GetTargetPosition();
	pathState_.targetMovedDistanceForRepath = LengthXZ(targetPos - pathState_.lastPathTargetPos);
	if (pathState_.targetMovedDistanceForRepath >= pathSettings_.targetRepathThreshold || isStuck_) { navigator_.Reset(); pathState_.lastRepathReason = isStuck_ ? "StuckForceRepath" : "TargetMoved"; }
	const Vector3 selfPos = GetPosition();
	if (navigator_.GetNextWaypoint(selfPos, targetPos, selfPos.y, deltaTime, pathState_.currentWaypoint))
	{
		pathState_.found = true;
		pathState_.failureReason = "None";
		pathState_.lastPathTargetPos = targetPos;
		pathState_.lastRepathReason = "Periodic";
		int blockedIdx = -1;
		if (navigator_.IsSegmentBlockedByObstacle(selfPos, pathState_.currentWaypoint, selfPos.y, &blockedIdx))
		{
			pathState_.lineBlocked = true;
			pathState_.blockedWaypointIndex = navigator_.GetCurrentPathIndex();
			pathState_.blockedObstacleName = (blockedIdx >= 0) ? ("Obstacle[" + std::to_string(blockedIdx) + "]") : "Unknown";
			pathState_.blockedSegmentFrom = selfPos;
			pathState_.blockedSegmentTo = pathState_.currentWaypoint;
			navigator_.Reset();
			navigator_.AddTemporaryBlockedArea(pathState_.currentWaypoint, pathSettings_.temporaryBlockRadius, pathSettings_.temporaryBlockDuration, "WaypointSegmentBlocked");
			pathState_.lastRepathReason = "WaypointSegmentBlocked";
			movementVelocity_ = Vector3{};
			runtime_.isMoving = false;
			return false;
		}
		pathState_.lineBlocked = false;
		pathState_.blockedWaypointIndex = navigator_.GetCurrentPathIndex();
		pathState_.blockedObstacleName = "None";
		Vector3 dir = pathState_.currentWaypoint - selfPos;
		dir.y = 0.0f;
		const float dirLenSq = dir.x * dir.x + dir.z * dir.z;
		if (dirLenSq <= 0.0001f)
		{
			movementVelocity_ = Vector3{};
			runtime_.isMoving = false;
			return false;
		}
		const float dirLen = std::sqrt(dirLenSq);
		dir.x /= dirLen;
		dir.z /= dirLen;
		const float speed = (runtime_.currentActionName == "Charge") ? GetCurrentMoveSpeed() * 2.3f : GetCurrentMoveSpeed();
		movementVelocity_ = Vector3{ dir.x * speed, 0.0f, dir.z * speed };
		runtime_.isMoving = true;
		runtime_.lastMoveDirection = dir;
		runtime_.currentActionName = "Chase";
		return true;
	}
	pathState_.found = false;
	pathState_.failureReason = "PathNotFound";
	pathState_.retryTimer += deltaTime;
	pathState_.failedWaitTimer = std::max(0.0f, pathState_.failedWaitTimer - deltaTime);
	if (pathState_.failedWaitTimer > 0.0f)
	{
		pathState_.lastRepathReason = "PathFailedWait";
		movementVelocity_ = Vector3{};
		runtime_.isMoving = false;
		return false;
	}
	if (pathState_.retryTimer >= pathSettings_.repathInterval)
	{
		navigator_.Reset();
		pathState_.retryTimer = 0.0f;
		pathState_.lastRepathReason = "RetryAfterFailure";
	}
	movementVelocity_ = Vector3{};
	runtime_.isMoving = false;
	return false;
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

bool GuardianBoss::ResolveObstaclePenetrationXZ(float)
{
	if (!wallObstacleAABBs_) { return false; }
	Vector3 pos = GetPosition();
	const float half = 1.1f;
	bool resolved = false;
	for (const auto& o : *wallObstacleAABBs_)
	{
		if (pos.x + half <= o.min.x || pos.x - half >= o.max.x || pos.z + half <= o.min.z || pos.z - half >= o.max.z) { continue; }
		const float pushLeft = o.min.x - (pos.x + half);
		const float pushRight = o.max.x - (pos.x - half);
		const float pushBack = o.min.z - (pos.z + half);
		const float pushFront = o.max.z - (pos.z - half);
		float minAbs = std::abs(pushLeft);
		Vector3 push{ pushLeft, 0.0f, 0.0f };
		if (std::abs(pushRight) < minAbs) { minAbs = std::abs(pushRight); push = { pushRight, 0.0f, 0.0f }; }
		if (std::abs(pushBack) < minAbs) { minAbs = std::abs(pushBack); push = { 0.0f, 0.0f, pushBack }; }
		if (std::abs(pushFront) < minAbs) { push = { 0.0f, 0.0f, pushFront }; }
		pos.x += push.x; pos.z += push.z;
		resolved = true;
	}
	if (resolved) { SetPosition(pos); }
	return resolved;
}
