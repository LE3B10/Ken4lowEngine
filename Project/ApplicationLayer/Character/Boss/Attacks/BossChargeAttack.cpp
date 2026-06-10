#define NOMINMAX
#include "BossChargeAttack.h"
#include "BossBase.h"
#include "BossAttackEffects.h"
#include "Wireframe.h"
#include "GpuParticleType.h"
#include <LogString.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

void BossChargeAttack::Initialize(BossBase* owner)
{
	owner_ = owner;
	isActive_ = false;
	isFinished_ = false;
	hasHit_ = false;
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;
	cooldownRemaining_ = 0.0f;
	hasLockedDirection_ = false;
	traveledDistance_ = 0.0f;
}

void BossChargeAttack::Start()
{
	if (!CanStart()) return;

	isActive_ = true;
	isFinished_ = false;
	hasHit_ = false;
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;
	traveledDistance_ = 0.0f;

	LockChargeDirection(); // 突進中に毎フレーム追尾しないよう、開始時のプレイヤー方向を固定する。
	ChangePhase(Phase::Windup);
	Log("[BossChargeAttack] Start\n");
}

void BossChargeAttack::Update(float deltaTime)
{
	if (!isActive_) return;

	totalTimer_ += deltaTime;
	phaseTimer_ += deltaTime;

	switch (phase_)
	{
	case Phase::Windup: UpdateWindup(deltaTime); break;
	case Phase::Charging: UpdateCharging(deltaTime); break;
	case Phase::Recovery: UpdateRecovery(deltaTime); break;
	case Phase::None:
	default: break;
	}
}

void BossChargeAttack::End()
{
	if (!isActive_) return;
	isActive_ = false;
	isFinished_ = true;
	cooldownRemaining_ = cooldownSec_;
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	Log("[BossChargeAttack] End\n");
}

bool BossChargeAttack::CanStart() const
{
	if (owner_ == nullptr) return false;
	if (isActive_ || cooldownRemaining_ > 0.0f) return false;
	if (owner_->IsDead()) return false;
	return IsTargetInValidRange();
}

void BossChargeAttack::TickCooldown(float deltaTime)
{
	if (cooldownRemaining_ <= 0.0f) return;
	cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - deltaTime);
}

void BossChargeAttack::UpdateWindup(float deltaTime)
{
	(void)deltaTime;
	if (phaseTimer_ >= startupTime_) ChangePhase(Phase::Charging);
}

void BossChargeAttack::UpdateCharging(float deltaTime)
{
	if (owner_ == nullptr) return;

	const float step = std::max(0.0f, chargeSpeed_) * deltaTime;
	Vector3 pos = owner_->GetPosition();
	pos.x += lockedForward_.x * step;
	pos.z += lockedForward_.z * step;

	const bool blocked = owner_->MoveWithWorldCollision(pos); // 突進中も障害物を貫通しないよう、移動を押し戻し判定に通す。
	traveledDistance_ += step;

	if (blocked)
	{
		ChangePhase(Phase::Recovery);
		return;
	}

	Vector3 trailPosition = owner_->GetCenterPosition();
	trailPosition.x -= lockedForward_.x * 1.2f;
	trailPosition.y += 0.25f;
	trailPosition.z -= lockedForward_.z * 1.2f;
	// 突進中の背後へ残像トレイルを出し、ボスが高速で迫ってくる視覚的な圧を作る。
	BossAttackEffects::EmitGuardianAttackPresenceEffect("GuardianChargeTrail", GpuParticleType::Trail, trailPosition, 10, 0.35f, 0.75f, 1.15f);

	Vector3 footDustPosition = owner_->GetPosition();
	footDustPosition.x -= lockedForward_.x * 0.45f;
	footDustPosition.y = 0.08f;
	footDustPosition.z -= lockedForward_.z * 0.45f;
	// 突進中の足元へ砂埃とMesh破片を出し、重い体が地面を削りながら進む印象を作る。
	BossAttackEffects::EmitGuardianAttackPresenceEffect("GuardianChargeFootDust", GpuParticleType::Dust, footDustPosition, 14, 0.85f, 0.55f, 1.45f);

	TryHitPlayer();

	if (hasHit_ || traveledDistance_ >= chargeDistance_)
	{
		ChangePhase(Phase::Recovery);
	}
}

void BossChargeAttack::UpdateRecovery(float deltaTime)
{
	(void)deltaTime;
	if (phaseTimer_ >= recoveryTime_) End();
}

void BossChargeAttack::ChangePhase(Phase newPhase)
{
	phase_ = newPhase;
	phaseTimer_ = 0.0f;
}

void BossChargeAttack::LockChargeDirection()
{
	if (owner_ == nullptr)
	{
		hasLockedDirection_ = false;
		return;
	}

	startPosition_ = owner_->GetPosition();
	lockedForward_ = owner_->GetDirectionToTargetXZOrForward(startPosition_);
	owner_->FaceDirectionXZImmediate(lockedForward_); // 突進は開始時の前方へだけ進み、移動中の急な追尾回転を避ける。
	hasLockedDirection_ = true;
}

void BossChargeAttack::TryHitPlayer()
{
	if (owner_ == nullptr || hasHit_) return;

	const Vector3 bossCenter = owner_->GetCenterPosition();
	const Vector3 target = owner_->GetTargetPosition();
	const float dx = target.x - bossCenter.x;
	const float dy = target.y - bossCenter.y;
	const float dz = target.z - bossCenter.z;
	const float sumRadius = hitRadius_ + targetRadius_;
	if ((dx * dx + dy * dy + dz * dz) <= (sumRadius * sumRadius))
	{
		hasHit_ = true;
		Vector3 hitPosition = bossCenter;
		hitPosition.y += 0.25f;
		if (owner_->ApplyDamageToTargetPlayer(damage_, &hitPosition))
		{
			BossAttackEffects::EmitGuardianHitEffect("GuardianChargeImpact", GpuParticleType::Debris, hitPosition, particleSpawnCount_, particleSpawnRadius_, particleLifetimeScale_, particleInitialSpeedScale_);
			Log("[BossChargeAttack] Player damage applied.\n");
		}
	}
}

bool BossChargeAttack::IsTargetInValidRange() const
{
	if (owner_ == nullptr) return false;
	const float distance = owner_->GetDistanceToTargetXZ();
	return distance >= minRange_ && distance <= maxRange_;
}

const char* BossChargeAttack::GetPhaseName() const
{
	switch (phase_)
	{
	case Phase::Windup: return "Windup";
	case Phase::Charging: return "Charging";
	case Phase::Recovery: return "Recovery";
	case Phase::None:
	default: return "None";
	}
}

void BossChargeAttack::SetValidRange(float minRange, float maxRange)
{
	minRange_ = std::max(0.0f, minRange);
	maxRange_ = std::max(minRange_, maxRange);
}

void BossChargeAttack::SetChargeParameters(float speed, float distance, float damage, float startupSec, float recoverySec, float cooldownSec)
{
	// 突進の速度・距離・威力・予備動作・後隙・クールタイムをParameterManagerから調整可能にする。
	chargeSpeed_ = std::max(0.0f, speed);
	chargeDistance_ = std::max(0.0f, distance);
	damage_ = std::max(0.0f, damage);
	startupTime_ = std::max(0.0f, startupSec);
	recoveryTime_ = std::max(0.0f, recoverySec);
	cooldownSec_ = std::max(0.0f, cooldownSec);
}

void BossChargeAttack::SetImpactParticleParameters(uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	particleSpawnCount_ = spawnCount;
	particleSpawnRadius_ = std::max(0.0f, spawnRadius);
	particleLifetimeScale_ = std::max(0.01f, lifetimeScale);
	particleInitialSpeedScale_ = std::max(0.0f, initialSpeedScale);
}

void BossChargeAttack::Draw()
{
#ifdef _DEBUG
	if (!owner_ || !isActive_) return;
	if (auto* wireframe = Wireframe::GetInstance())
	{
		Vector3 from = startPosition_;
		from.y += 0.2f;
		Vector3 to = from;
		to.x += lockedForward_.x * chargeDistance_;
		to.z += lockedForward_.z * chargeDistance_;
		wireframe->DrawLine(from, to, Vector4{ 1.0f, 0.15f, 0.05f, 1.0f });
	}
#endif
}

void BossChargeAttack::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Separator();
	ImGui::Text("[BossChargeAttack]");
	ImGui::Text("Active            : %s", isActive_ ? "true" : "false");
	ImGui::Text("Finished          : %s", isFinished_ ? "true" : "false");
	ImGui::Text("HasHit            : %s", hasHit_ ? "true" : "false");
	ImGui::Text("Phase             : %s", GetPhaseName());
	ImGui::Text("PhaseTimer        : %.2f", phaseTimer_);
	ImGui::Text("TotalTimer        : %.2f", totalTimer_);
	ImGui::Text("CooldownRemaining : %.2f", cooldownRemaining_);
	ImGui::Text("Range             : %.2f - %.2f", minRange_, maxRange_);
	ImGui::Text("Speed/Distance    : %.2f / %.2f", chargeSpeed_, chargeDistance_);
	ImGui::Text("TraveledDistance  : %.2f", traveledDistance_);
	ImGui::Text("Damage            : %.2f", damage_);
	ImGui::Text("Startup/Recovery  : %.2f / %.2f", startupTime_, recoveryTime_);
#endif
}
