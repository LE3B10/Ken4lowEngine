#define NOMINMAX
#include "BossBase.h"

#include "BossAttackEffects.h"
#include "CollisionPreset.h"
#include "GpuParticleType.h"
#include "Player.h"

#include <LogString.h>
#include <ParameterManager.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Ken4lowEngine;

namespace
{
	constexpr const char* kBossCommonGroup = "BossCommon";
	constexpr float kDefaultBossMaxHP = 300.0f;
	constexpr float kDefaultBossMoveSpeed = 2.0f;
	constexpr float kDefaultBossTurnSpeed = 4.0f;
	constexpr float kDefaultBossStopDistance = 3.0f;
	constexpr float kDefaultBossAttackRange = 3.0f;
	constexpr float kDefaultBossAttackCooldown = 1.2f;
	constexpr float kBossDeathPresentationSeconds = 3.0f;
	constexpr float kBossDeathGravity = 9.8f;
	constexpr float kBossDeathFloorY = 0.05f;

	struct BossDeathPiece
	{
		BaseCharacter::BodyPart* part = nullptr;
		K4E::Vector3 velocity{};
		K4E::Vector3 angularVelocity{};
	};

	struct BossDeathRuntime
	{
		bool initialized = false;
		bool startBurstDone = false;
		float timer = 0.0f;
		float soulEmitTimer = 0.0f;
		std::vector<BossDeathPiece> pieces{};
	};

	std::unordered_map<const BossBase*, BossDeathRuntime> g_bossDeathRuntimes;

	float Rand01()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	float RandRange(float minimum, float maximum)
	{
		return minimum + (maximum - minimum) * Rand01();
	}

	float Length(const K4E::Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	K4E::Vector3 NormalizeSafe(const K4E::Vector3& value, const K4E::Vector3& fallback = { 0.0f, 1.0f, 0.0f })
	{
		const float length = Length(value);
		return length > 0.0001f ? value * (1.0f / length) : fallback;
	}

	K4E::Vector3 ClampVectorLength(const K4E::Vector3& value, float maxLength)
	{
		const float length = Length(value);
		return (length <= maxLength || length <= 0.0001f) ? value : value * (maxLength / length);
	}

	void EnsureBossCommonParameters()
	{
		static bool initialized = false;
		if (initialized) return;
		initialized = true;

		auto* parameters = ParameterManager::GetInstance();
		parameters->CreateGroup(kBossCommonGroup);
		if (std::filesystem::exists("Resources/ParameterManager/BossCommon.json")) parameters->LoadFile(kBossCommonGroup);
		else Log("[BossBase] BossCommon.json not found. Use built-in default boss parameters.\n");

		parameters->AddItem(kBossCommonGroup, "maxHP", kDefaultBossMaxHP, 1.0f, 10000.0f);
		parameters->AddItem(kBossCommonGroup, "moveSpeed", kDefaultBossMoveSpeed, 0.0f, 50.0f);
		parameters->AddItem(kBossCommonGroup, "turnSpeed", kDefaultBossTurnSpeed, 0.0f, 30.0f);
		parameters->AddItem(kBossCommonGroup, "stopDistance", kDefaultBossStopDistance, 0.0f, 100.0f);
		parameters->AddItem(kBossCommonGroup, "attackRange", kDefaultBossAttackRange, 0.0f, 100.0f);
		parameters->AddItem(kBossCommonGroup, "attackCooldownSec", kDefaultBossAttackCooldown, 0.0f, 30.0f);
		parameters->SetDisplayName(kBossCommonGroup, "maxHP", "ボス最大HP");
		parameters->SetDisplayName(kBossCommonGroup, "moveSpeed", "ボス移動速度");
		parameters->SetDisplayName(kBossCommonGroup, "turnSpeed", "ボス旋回速度");
		parameters->SetDisplayName(kBossCommonGroup, "stopDistance", "ボス停止距離");
		parameters->SetDisplayName(kBossCommonGroup, "attackRange", "ボス攻撃距離");
		parameters->SetDisplayName(kBossCommonGroup, "attackCooldownSec", "ボス攻撃クールタイム");
	}

	template<typename T>
	T GetBossParameterOrDefault(const std::string& key, const T& defaultValue)
	{
		try
		{
			return ParameterManager::GetInstance()->GetValue<T>(kBossCommonGroup, key);
		}
		catch (const std::exception& exception)
		{
			Log("[BossBase] Failed to read BossCommon." + key + ": " + exception.what() + ". Use default.\n");
			return defaultValue;
		}
	}

	bool IsBossHealthDead(const BossBase& boss)
	{
		const auto* health = boss.GetHealthComponent();
		return !health || health->IsDead();
	}

	BossDeathRuntime& GetBossDeathRuntime(const BossBase& boss)
	{
		return g_bossDeathRuntimes[&boss];
	}

	K4E::Vector3 MakeBossDeathCenter(BossBase& boss)
	{
		auto& body = boss.GetBody();
		body.transform.Update();
		return body.transform.worldTranslate_;
	}

	BossDeathPiece MakeBossDeathPiece(BaseCharacter::BodyPart& part, const K4E::Vector3& center, float powerScale, float upwardScale)
	{
		BossDeathPiece piece{};
		piece.part = &part;
		const K4E::Vector3 outward = NormalizeSafe(part.transform.translate_ - center, { 0.0f, 0.0f, 1.0f });
		K4E::Vector3 randomHorizontal{ RandRange(-1.0f, 1.0f), 0.0f, RandRange(-1.0f, 1.0f) };
		randomHorizontal = NormalizeSafe(randomHorizontal, outward);
		const K4E::Vector3 direction = NormalizeSafe(outward * 0.8f + randomHorizontal * 0.2f, outward);
		piece.velocity = ClampVectorLength(direction * RandRange(3.2f, 6.0f) * powerScale + K4E::Vector3{ 0.0f, RandRange(2.2f, 4.8f) * upwardScale, 0.0f }, 9.0f);
		piece.angularVelocity = ClampVectorLength({ RandRange(-3.0f, 3.0f), RandRange(-4.0f, 4.0f), RandRange(-3.0f, 3.0f) }, 7.0f);
		return piece;
	}

	void StartBossBreakApartDeath(BossBase& boss, BossDeathRuntime& runtime)
	{
		if (runtime.initialized) return;
		runtime.initialized = true;
		runtime.timer = 0.0f;
		runtime.soulEmitTimer = 0.0f;
		runtime.pieces.clear();

		auto& body = boss.GetBody();
		auto& parts = boss.GetBodyParts();
		body.transform.Update();
		const K4E::Vector3 center = body.transform.worldTranslate_;

		body.transform.parent_ = nullptr;
		body.transform.translate_ = center;
		body.transform.Update();
		if (body.object)
		{
			body.object->SetTranslate(body.transform.translate_);
			body.object->SetRotate(body.transform.rotate_);
			body.object->SetColor({ 1.0f, 0.45f, 0.35f, 1.0f });
			body.object->Update();
		}
		runtime.pieces.push_back(MakeBossDeathPiece(body, center, 0.45f, 0.55f));

		for (auto& part : parts)
		{
			part.transform.worldRotate_ = body.transform.worldRotate_;
			part.transform.Update();
			part.transform.parent_ = nullptr;
			part.transform.translate_ = part.transform.worldTranslate_;
			part.transform.rotate_ = part.transform.worldRotate_;
			part.transform.Update();
			if (part.object)
			{
				part.object->SetTranslate(part.transform.translate_);
				part.object->SetRotate(part.transform.rotate_);
				part.object->SetColor({ 1.0f, 0.45f, 0.35f, 1.0f });
				part.object->Update();
			}
			runtime.pieces.push_back(MakeBossDeathPiece(part, center, 1.15f, 1.15f));
		}

		K4E::Vector3 burstPosition = center;
		burstPosition.y += 1.0f;
		BossAttackEffects::EmitGuardianHitEffect("GuardianBossDeathStartBurst", K4E::GpuParticleType::Shockwave, burstPosition, 96, 3.5f, 1.3f, 2.4f);
		BossAttackEffects::EmitGuardianMeshDebrisEffect("GuardianBossDeathBreakDebris", K4E::GpuParticleType::Debris, center, 180, 4.0f, 1.8f, 2.8f);
	}

	void UpdateBossBreakApartDeath(BossBase& boss, float deltaTime)
	{
		BossDeathRuntime& runtime = GetBossDeathRuntime(boss);
		StartBossBreakApartDeath(boss, runtime);
		deltaTime = std::clamp(deltaTime, 0.0f, 0.05f);
		runtime.timer += deltaTime;
		runtime.soulEmitTimer += deltaTime;

		const K4E::Vector3 center = MakeBossDeathCenter(boss);
		K4E::Vector3 soulPosition = center;
		soulPosition.y += 1.8f + runtime.timer * 0.85f;
		if (runtime.soulEmitTimer >= 0.06f && runtime.timer < kBossDeathPresentationSeconds)
		{
			runtime.soulEmitTimer = 0.0f;
			BossAttackEffects::EmitGuardianAttackPresenceEffect("GuardianBossSoulRise", K4E::GpuParticleType::Trail, soulPosition, 5, 0.45f, 0.75f, 0.55f);
			BossAttackEffects::EmitGuardianAttackPresenceEffect("GuardianBossDeathSmoke", K4E::GpuParticleType::Dust, center, 4, 1.8f, 0.65f, 0.8f);
		}
		if (!runtime.startBurstDone && runtime.timer >= 0.35f)
		{
			runtime.startBurstDone = true;
			K4E::Vector3 secondBurst = center;
			secondBurst.y += 1.2f;
			BossAttackEffects::EmitGuardianHitEffect("GuardianBossDeathSecondBurst", K4E::GpuParticleType::Shockwave, secondBurst, 72, 4.2f, 1.0f, 2.2f);
		}

		for (BossDeathPiece& piece : runtime.pieces)
		{
			if (!piece.part || !piece.part->object) continue;
			piece.velocity.y -= kBossDeathGravity * deltaTime;
			piece.velocity = ClampVectorLength(piece.velocity * std::max(0.0f, 1.0f - 0.55f * deltaTime), 9.0f);
			piece.angularVelocity = ClampVectorLength(piece.angularVelocity * std::max(0.0f, 1.0f - 0.35f * deltaTime), 7.0f);
			piece.part->transform.translate_ = piece.part->transform.translate_ + piece.velocity * deltaTime;
			piece.part->transform.rotate_ = piece.part->transform.rotate_ + piece.angularVelocity * deltaTime;
			if (piece.part->transform.translate_.y < kBossDeathFloorY)
			{
				piece.part->transform.translate_.y = kBossDeathFloorY;
				if (piece.velocity.y < 0.0f)
				{
					piece.velocity.y = -piece.velocity.y * 0.30f;
					piece.velocity.x *= 0.65f;
					piece.velocity.z *= 0.65f;
				}
			}
			piece.part->transform.parent_ = nullptr;
			piece.part->transform.Update();
			piece.part->object->SetTranslate(piece.part->transform.translate_);
			piece.part->object->SetRotate(piece.part->transform.rotate_);
			piece.part->object->Update();
		}
	}
}

BossBase::~BossBase()
{
	ParameterManager::GetInstance()->UnregisterParameterApplier(kBossCommonGroup, this);
	g_bossDeathRuntimes.erase(this);
}

void BossBase::Initialize()
{
	EnsureBossCommonParameters();
	g_bossDeathRuntimes.erase(this);

	worldCollisionSettings_.half = { 1.25f, 1.75f, 1.25f };
	worldCollisionSettings_.centerOffset = {};
	worldCollisionSettings_.eps = 0.002f;

	BuildBossParts(); // 共通Character/Humanoid Componentを先に生成する。
	ApplyCollisionPreset(*this, ECollisionPresetId::Boss);
	if (auto* collider = GetColliderComponent()) collider->SetHalfSize(worldCollisionSettings_.half);
	if (Collider* primitive = GetCollisionPrimitive()) primitive->SetOwner(this);

	brain_ = std::make_unique<BossBrain>();
	brain_->Initialize(this);

	statusComponent_ = std::make_unique<BossStatusComponent>();
	statusComponent_->Initialize(GetHealthComponent(), GetBossParameterOrDefault("maxHP", kDefaultBossMaxHP)); // HP実体はCharacterHealthComponentだけが所有する。

	stateMachine_ = std::make_unique<BossStateMachine>();
	stateMachine_->Initialize(state_);

	movementComponent_ = std::make_unique<BossMovementComponent>();
	movementComponent_->Initialize(
		GetBossParameterOrDefault("moveSpeed", kDefaultBossMoveSpeed),
		GetBossParameterOrDefault("turnSpeed", kDefaultBossTurnSpeed),
		GetBossParameterOrDefault("stopDistance", kDefaultBossStopDistance));

	animationComponent_ = std::make_unique<BossAnimationComponent>();
	animationComponent_->Initialize(this);

	attackComponent_ = std::make_unique<BossAttackComponent>();
	attackRange_ = GetBossParameterOrDefault("attackRange", kDefaultBossAttackRange);
	attackCooldownSec_ = GetBossParameterOrDefault("attackCooldownSec", kDefaultBossAttackCooldown);
	attackCooldownTimer_ = 0.0f;

	SetupAttacks();
	SetupPhaseData();
	SetupWeakPoints();
	SetupBoss();
	attackComponent_->Initialize(this);

	ParameterManager::GetInstance()->RegisterParameterApplier(kBossCommonGroup, this, [this]() { ApplyParameters(); });
}

void BossBase::Update(float deltaTime)
{
	if (statusComponent_) statusComponent_->Update(deltaTime);

	if (IsBossHealthDead(*this))
	{
		if (state_ != BossState::Dead) OnDead();
		UpdateBossBreakApartDeath(*this, deltaTime);
		return;
	}

	attackCooldownTimer_ = std::max(0.0f, attackCooldownTimer_ - deltaTime);
	if (stateMachine_)
	{
		stateMachine_->Update(*this, deltaTime);
		state_ = stateMachine_->GetCurrentState();
	}

	UpdateState(deltaTime);
	UpdatePhase(deltaTime);
	UpdateMovement(deltaTime);
	UpdateAttack(deltaTime);
	if (animationComponent_) animationComponent_->Update(*this, deltaTime);
	UpdateWeakPoints(deltaTime);
	CheckDeath();

	BaseCharacter::Update(deltaTime); // Transform・表示・Colliderは共通Actor/Componentの更新経路へ統一する。
}

void BossBase::Draw()
{
	BaseCharacter::Draw();
	if (attackComponent_) attackComponent_->Draw();
}

void BossBase::ForceSyncWorldTransform()
{
	BaseCharacter::Update(0.0f);
}

bool BossBase::MoveWithWorldCollision(const K4E::Vector3& desiredPosition)
{
	const K4E::Vector3 oldPosition = GetPosition();
	if (!stageObstacleAABBs_ || stageObstacleAABBs_->empty())
	{
		SetPosition(desiredPosition);
		return false;
	}

	const K4E::WorldCollisionResult result = K4E::WorldCollisionResolver::Resolve(
		*stageObstacleAABBs_, worldCollisionSettings_, oldPosition, desiredPosition, false, nullptr);
	const K4E::Vector3 resolvedPosition = result.fixedCenter + worldCollisionSettings_.centerOffset;
	SetPosition(resolvedPosition);
	const float dx = resolvedPosition.x - desiredPosition.x;
	const float dz = resolvedPosition.z - desiredPosition.z;
	return (dx * dx + dz * dz) > 0.0001f;
}

void BossBase::ClearRootParentKeepingWorldPosition()
{
	auto& bodyTransform = GetBody().transform;
	bodyTransform.Update();
	const K4E::Vector3 worldPosition = bodyTransform.worldTranslate_;
	bodyTransform.parent_ = nullptr;
	bodyTransform.translate_ = worldPosition;
	bodyTransform.Update();
	SetPosition(worldPosition);
}

void BossBase::DrawShadow()
{
	BaseCharacter::DrawShadow();
	if (attackComponent_) attackComponent_->DrawShadow();
}

void BossBase::DrawImGui()
{
#ifdef USE_IMGUI
	if (attackComponent_) attackComponent_->DrawImGui();
#endif
}

void BossBase::ApplyParameters()
{
	EnsureBossCommonParameters();
	if (statusComponent_) statusComponent_->SetMaxHP(GetBossParameterOrDefault("maxHP", kDefaultBossMaxHP));
	if (movementComponent_)
	{
		movementComponent_->SetMoveSpeed(GetBossParameterOrDefault("moveSpeed", kDefaultBossMoveSpeed));
		movementComponent_->SetTurnSpeed(GetBossParameterOrDefault("turnSpeed", kDefaultBossTurnSpeed));
		movementComponent_->SetStopDistance(GetBossParameterOrDefault("stopDistance", kDefaultBossStopDistance));
	}
	attackRange_ = GetBossParameterOrDefault("attackRange", kDefaultBossAttackRange);
	attackCooldownSec_ = GetBossParameterOrDefault("attackCooldownSec", kDefaultBossAttackCooldown);
}

void BossBase::Finalize()
{
	ParameterManager::GetInstance()->UnregisterParameterApplier(kBossCommonGroup, this);
	g_bossDeathRuntimes.erase(this);

	if (attackComponent_) { attackComponent_->Finalize(); attackComponent_.reset(); }
	if (animationComponent_) { animationComponent_->Finalize(); animationComponent_.reset(); }
	if (movementComponent_) { movementComponent_->Finalize(); movementComponent_.reset(); }
	if (stateMachine_) { stateMachine_->Finalize(); stateMachine_.reset(); }
	if (statusComponent_) { statusComponent_->Finalize(); statusComponent_.reset(); }
	if (brain_) { brain_->Finalize(); brain_.reset(); }

	BaseCharacter::Finalize(); // Visual/Collider/Healthを個別破棄せずActor/Componentの終了処理へ統一する。
}

void BossBase::OnDamaged(float damage)
{
	if (!statusComponent_ || IsBossHealthDead(*this) || damage <= 0.0f) return;
	const float hpBefore = statusComponent_->GetHP();
	statusComponent_->ApplyDamage(damage);
	std::ostringstream stream;
	stream << "[GuardianBoss] TakeDamage: damage=" << damage
		<< ", HP=" << statusComponent_->GetHP() << "/" << statusComponent_->GetMaxHP()
		<< ", before=" << hpBefore;
	Log(stream.str() + "\n");
}

void BossBase::OnBulletDamaged(float damage)
{
	OnDamaged(damage);
}

bool BossBase::ApplyDamageToTargetPlayer(float damage, const K4E::Vector3* attackPosition)
{
	if (!targetPlayer_ || damage <= 0.0f || IsBossHealthDead(*this)) return false;
	targetPlayer_->ApplyDamage(damage, attackPosition);
	OnTargetPlayerDamaged(damage);
	return true;
}

void BossBase::OnTargetPlayerDamaged(float damage)
{
	(void)damage;
}

void BossBase::OnDead()
{
	state_ = BossState::Dead;
	if (stateMachine_) stateMachine_->ChangeState(*this, BossState::Dead);
	if (attackComponent_) attackComponent_->ForceEndCurrentAttack();
	if (auto* collider = GetColliderComponent()) collider->SetActive(false);
	if (Collider* primitive = GetCollisionPrimitive()) primitive->SetEnabled(false);
	BossDeathRuntime& runtime = GetBossDeathRuntime(*this);
	StartBossBreakApartDeath(*this, runtime);
}

bool BossBase::IsAlive() const
{
	return !IsBossHealthDead(*this);
}

bool BossBase::IsDead() const
{
	if (!IsBossHealthDead(*this)) return false;
	const auto it = g_bossDeathRuntimes.find(this);
	if (it == g_bossDeathRuntimes.end()) return true;
	return it->second.timer >= kBossDeathPresentationSeconds;
}

float BossBase::GetHP() const
{
	const auto* health = GetHealthComponent();
	return health ? health->GetCurrentHealth() : 0.0f;
}

float BossBase::GetMaxHP() const
{
	const auto* health = GetHealthComponent();
	return health ? health->GetMaxHealth() : 0.0f;
}

float BossBase::GetHPRate() const
{
	const auto* health = GetHealthComponent();
	return health ? health->GetHealthRatio() : 0.0f;
}

K4E::Vector3 BossBase::GetDirectionToTargetXZOrForward(const K4E::Vector3& origin) const
{
	K4E::Vector3 toTarget{ targetPosition_.x - origin.x, 0.0f, targetPosition_.z - origin.z };
	const float lengthSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lengthSq > 0.0001f)
	{
		const float inverseLength = 1.0f / std::sqrt(lengthSq);
		return { toTarget.x * inverseLength, 0.0f, toTarget.z * inverseLength };
	}
	return { std::sin(GetYaw()), 0.0f, std::cos(GetYaw()) };
}

void BossBase::FaceDirectionXZImmediate(const K4E::Vector3& direction)
{
	const float lengthSq = direction.x * direction.x + direction.z * direction.z;
	if (lengthSq > 0.0001f) SetYaw(std::atan2(-direction.x, direction.z));
}

float BossBase::GetDistanceToTargetXZ() const
{
	const float dx = targetPosition_.x - GetPosition().x;
	const float dz = targetPosition_.z - GetPosition().z;
	return std::sqrt(dx * dx + dz * dz);
}

bool BossBase::IsTargetInAttackRange() const
{
	return GetDistanceToTargetXZ() <= attackRange_;
}

void BossBase::RegisterAttack(std::unique_ptr<IBossAttack> attack)
{
	if (attack && attackComponent_) attackComponent_->RegisterAttack(std::move(attack));
}

void BossBase::UpdateState(float deltaTime)
{
	(void)deltaTime;
	if (!stateMachine_) return;
	if (IsBossHealthDead(*this))
	{
		stateMachine_->ChangeState(*this, BossState::Dead);
		state_ = stateMachine_->GetCurrentState();
		return;
	}

	switch (stateMachine_->GetCurrentState())
	{
	case BossState::Intro:
		stateMachine_->ChangeState(*this, BossState::Idle);
		break;
	case BossState::Idle:
		stateMachine_->ChangeState(*this, IsTargetInAttackRange() && !IsAttackCoolingDown() ? BossState::Attack : BossState::Move);
		break;
	case BossState::Move:
		if (IsTargetInAttackRange() && !IsAttackCoolingDown()) stateMachine_->ChangeState(*this, BossState::Attack);
		break;
	case BossState::Attack:
		if (attackComponent_ && !attackComponent_->IsAttacking())
		{
			attackCooldownTimer_ = attackCooldownSec_;
			stateMachine_->ChangeState(*this, IsTargetInAttackRange() ? BossState::Idle : BossState::Move);
		}
		break;
	default:
		break;
	}
	state_ = stateMachine_->GetCurrentState();
}

void BossBase::UpdatePhase(float deltaTime)
{
	(void)deltaTime;
}

void BossBase::UpdateMovement(float deltaTime)
{
	if (movementComponent_) movementComponent_->Update(*this, deltaTime);
}

void BossBase::UpdateAttack(float deltaTime)
{
	if (attackComponent_) attackComponent_->Update(deltaTime);
}

void BossBase::UpdateWeakPoints(float deltaTime)
{
	(void)deltaTime;
}

void BossBase::CheckDeath()
{
	if (IsBossHealthDead(*this) && state_ != BossState::Dead) OnDead();
}

K4E::Vector3 BossBase::GetPartWorldPosition(size_t partIndex)
{
	auto& parts = GetBodyParts();
	if (partIndex >= parts.size()) return GetCenterPosition();
	auto& body = GetBody();
	body.transform.Update();
	auto& part = parts[partIndex];
	part.transform.worldRotate_ = body.transform.worldRotate_;
	part.transform.Update();
	return part.transform.worldTranslate_;
}

bool BossBase::IsSphereHit(const K4E::Vector3& attackCenter, float attackRadius, const K4E::Vector3& targetCenter, float targetRadius) const
{
	const float dx = attackCenter.x - targetCenter.x;
	const float dy = attackCenter.y - targetCenter.y;
	const float dz = attackCenter.z - targetCenter.z;
	const float sumRadius = attackRadius + targetRadius;
	return dx * dx + dy * dy + dz * dz <= sumRadius * sumRadius;
}

BossHitResult BossBase::CheckDebugHitSphere(const K4E::Vector3& attackCenter, float attackRadius)
{
	BossHitResult result{};
	if (IsBossHealthDead(*this)) return result;
	const auto& indices = GetPartIndices();
	struct HitCheckInfo
	{
		BossHitPart part;
		Vector3 position;
		float radius;
		float damageMultiplier;
	};
	const HitCheckInfo checks[] = {
		{ BossHitPart::Head, GetPartWorldPosition(indices.head), 0.45f, 2.0f },
		{ BossHitPart::Body, GetCenterPosition(), 0.85f, 1.0f },
		{ BossHitPart::LeftArm, GetPartWorldPosition(indices.leftArm), 0.45f, 0.8f },
		{ BossHitPart::RightArm, GetPartWorldPosition(indices.rightArm), 0.45f, 0.8f },
		{ BossHitPart::LeftLeg, GetPartWorldPosition(indices.leftLeg), 0.50f, 0.9f },
		{ BossHitPart::RightLeg, GetPartWorldPosition(indices.rightLeg), 0.50f, 0.9f },
	};
	for (const HitCheckInfo& check : checks)
	{
		if (!IsSphereHit(attackCenter, attackRadius, check.position, check.radius)) continue;
		result.isHit = true;
		result.part = check.part;
		result.hitPosition = check.position;
		result.damageMultiplier = check.damageMultiplier;
		break;
	}
	return result;
}

void BossBase::ApplyDebugHitResult(const BossHitResult& hitResult, float baseDamage)
{
	if (hitResult.isHit) OnDamaged(baseDamage * hitResult.damageMultiplier);
}
