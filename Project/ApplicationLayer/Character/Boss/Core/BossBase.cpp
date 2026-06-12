#define NOMINMAX
#include "BossBase.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "Player.h"
#include "BossAttackEffects.h"
#include "GpuParticleType.h"
#include <LogString.h>
#include <ParameterManager.h>

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

	float RandRange(float minValue, float maxValue)
	{
		return minValue + (maxValue - minValue) * Rand01();
	}

	float Length(const K4E::Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	K4E::Vector3 NormalizeSafe(const K4E::Vector3& value, const K4E::Vector3& fallback = { 0.0f, 1.0f, 0.0f })
	{
		const float length = Length(value);
		if (length <= 0.0001f)
		{
			return fallback;
		}
		return value * (1.0f / length);
	}

	K4E::Vector3 ClampVectorLength(const K4E::Vector3& value, float maxLength)
	{
		const float length = Length(value);
		if (length <= maxLength || length <= 0.0001f)
		{
			return value;
		}
		return value * (maxLength / length);
	}

	void EnsureBossCommonParameters()
	{
		static bool isInitialized = false;
		if (isInitialized)
		{
			return;
		}
		isInitialized = true;

		auto* parameters = ParameterManager::GetInstance();
		parameters->CreateGroup(kBossCommonGroup);

		// JSONが存在する場合は先に読み込み、欠けている項目だけ安全な既定値で補完する。
		if (std::filesystem::exists("Resources/ParameterManager/BossCommon.json"))
		{
			parameters->LoadFile(kBossCommonGroup);
		}
		else
		{
			Log("[BossBase] BossCommon.json not found. Use built-in default boss parameters.\n");
		}

		// HPや速度などはParameterManager上で実用的な範囲だけ調整できるようにする。
		parameters->AddItem(kBossCommonGroup, "maxHP", kDefaultBossMaxHP, 1.0f, 10000.0f);
		parameters->AddItem(kBossCommonGroup, "moveSpeed", kDefaultBossMoveSpeed, 0.0f, 50.0f);
		parameters->AddItem(kBossCommonGroup, "turnSpeed", kDefaultBossTurnSpeed, 0.0f, 30.0f);
		parameters->AddItem(kBossCommonGroup, "stopDistance", kDefaultBossStopDistance, 0.0f, 100.0f);
		parameters->AddItem(kBossCommonGroup, "attackRange", kDefaultBossAttackRange, 0.0f, 100.0f);
		parameters->AddItem(kBossCommonGroup, "attackCooldownSec", kDefaultBossAttackCooldown, 0.0f, 30.0f);
		// JSONキーを英数字のまま維持し、ImGui表示だけ日本語化する。
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
		catch (const std::exception& e)
		{
			Log("[BossBase] Failed to read BossCommon." + key + ": " + e.what() + ". Use default.\n");
			return defaultValue;
		}
	}

	bool IsBossStatusDead(const BossBase& boss)
	{
		const BossStatusComponent* status = boss.GetStatusComponent();
		return status ? status->IsDead() : true;
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

		// 雑魚敵と同じように部位を飛ばして、倒れて終わるだけの死亡演出にしない。
		piece.velocity = ClampVectorLength(direction * RandRange(3.2f, 6.0f) * powerScale + K4E::Vector3{ 0.0f, RandRange(2.2f, 4.8f) * upwardScale, 0.0f }, 9.0f);
		piece.angularVelocity = ClampVectorLength(K4E::Vector3{ RandRange(-3.0f, 3.0f), RandRange(-4.0f, 4.0f), RandRange(-3.0f, 3.0f) }, 7.0f);
		return piece;
	}

	void StartBossBreakApartDeath(BossBase& boss, BossDeathRuntime& runtime)
	{
		if (runtime.initialized)
		{
			return;
		}

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

		// 魂が抜けて上へ昇るように、死亡中は上方向へ継続パーティクルを出す。
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
			if (!piece.part || !piece.part->object)
			{
				continue;
			}

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
	// 破棄済みボスへParameterManagerの反映コールバックが飛ばないよう解除する。
	ParameterManager::GetInstance()->UnregisterParameterApplier(kBossCommonGroup, this);
	g_bossDeathRuntimes.erase(this);
}


/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void BossBase::Initialize()
{
	EnsureBossCommonParameters();
	g_bossDeathRuntimes.erase(this);

	// コライダータイプとObjectChannel/Responseの設定
	ApplyCollisionPreset(*this, ECollisionPresetId::Boss);
	Collider::SetOwner<BossBase>(this);
	// プレイヤー近接攻撃が胴体を拾えるように、ボス本体を覆う大きめのOBBを登録する。
	Collider::SetOBBHalfSize({ 1.25f, 1.75f, 1.25f });

	// 衝突設定
	worldCollisionSettings_.half = { 1.25f, 1.75f, 1.25f };
	worldCollisionSettings_.centerOffset = { 0.0f, 0.0f, 0.0f };
	worldCollisionSettings_.eps = 0.002f; // ボスが壁に密着した時の再侵入を防ぐため、少しだけ隙間を空ける。

	// ボス用部位を構築
	BuildBossParts();

	// 思考コンポーネント生成
	brain_ = std::make_unique<BossBrain>();
	brain_->Initialize(this);

	// ステータス生成
	statusComponent_ = std::make_unique<BossStatusComponent>();
	statusComponent_->Initialize(GetBossParameterOrDefault("maxHP", kDefaultBossMaxHP)); // HPはJSON調整値を優先し、失敗時だけ既定値を使う

	// 状態遷移生成
	stateMachine_ = std::make_unique<BossStateMachine>();
	stateMachine_->Initialize(state_); // 仮。後で開始状態を差し替え

	// 移動コンポーネント生成
	movementComponent_ = std::make_unique<BossMovementComponent>();
	movementComponent_->Initialize(
		GetBossParameterOrDefault("moveSpeed", kDefaultBossMoveSpeed),
		GetBossParameterOrDefault("turnSpeed", kDefaultBossTurnSpeed),
		GetBossParameterOrDefault("stopDistance", kDefaultBossStopDistance)); // 移動共通値はJSONから読み込んで調整しやすくする

	animationComponent_ = std::make_unique<BossAnimationComponent>();
	animationComponent_->Initialize(this);

	attackComponent_ = std::make_unique<BossAttackComponent>();
	attackRange_ = GetBossParameterOrDefault("attackRange", kDefaultBossAttackRange); // 共通攻撃距離をJSON化してボス調整を容易にする
	attackCooldownSec_ = GetBossParameterOrDefault("attackCooldownSec", kDefaultBossAttackCooldown); // 共通クールタイムをJSON化してボス調整を容易にする
	attackCooldownTimer_ = 0.0f;

	SetupAttacks();
	SetupPhaseData();
	SetupWeakPoints();
	SetupBoss();

	attackComponent_->Initialize(this);

	ParameterManager::GetInstance()->RegisterParameterApplier(kBossCommonGroup, this, [this]() { ApplyParameters(); }); // 保存/反映後に共通ボス値を実行中のインスタンスへ再適用する。
}

/// -------------------------------------------------------------
/// 更新
/// -------------------------------------------------------------
void BossBase::Update(float deltaTime)
{
	if (statusComponent_)
	{
		statusComponent_->Update(deltaTime);
	}

	// HPが0になった後はAI/移動/攻撃を止め、死亡分裂と魂VFXだけを進める。
	if (IsBossStatusDead(*this))
	{
		if (state_ != BossState::Dead)
		{
			OnDead();
		}
		UpdateBossBreakApartDeath(*this, deltaTime);
		Collider::SetCenterPosition(GetCenterPosition());
		Collider::SetOrientation({ 0.0f, GetYaw(), 0.0f });
		return;
	}

	// 攻撃クールタイム更新
	if (attackCooldownTimer_ > 0.0f)
	{
		attackCooldownTimer_ -= deltaTime;
		if (attackCooldownTimer_ < 0.0f)
		{
			attackCooldownTimer_ = 0.0f;
		}
	}

	if (stateMachine_)
	{
		stateMachine_->Update(*this, deltaTime);
		state_ = stateMachine_->GetCurrentState();
	}

	UpdateState(deltaTime);
	UpdatePhase(deltaTime);
	UpdateMovement(deltaTime);
	UpdateAttack(deltaTime);

	if (animationComponent_)
	{
		animationComponent_->Update(*this, deltaTime);
	}

	UpdateWeakPoints(deltaTime);
	CheckDeath();

	Collider::SetCenterPosition(GetCenterPosition());
	Collider::SetOrientation({ 0.0f, GetYaw(), 0.0f });

	BaseCharacter::Update(deltaTime);
}

/// -------------------------------------------------------------
/// 描画
/// -------------------------------------------------------------
void BossBase::Draw()
{
	BaseCharacter::Draw();

	if (attackComponent_)
	{
		attackComponent_->Draw();
	}
}

void BossBase::ForceSyncWorldTransform()
{
	Collider::SetCenterPosition(GetCenterPosition());
	Collider::SetOrientation({ 0.0f, GetYaw(), 0.0f });
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
		*stageObstacleAABBs_,
		worldCollisionSettings_,
		oldPosition,
		desiredPosition,
		false,
		nullptr
	);

	const K4E::Vector3 resolvedPosition = result.fixedCenter + worldCollisionSettings_.centerOffset;

	SetPosition(resolvedPosition);

	const float dx = resolvedPosition.x - desiredPosition.x;
	const float dz = resolvedPosition.z - desiredPosition.z;
	const bool blocked = (dx * dx + dz * dz) > 0.0001f;

	return blocked;
}

void BossBase::ClearRootParentKeepingWorldPosition()
{
	auto& bodyTransform = GetBody().transform;
	bodyTransform.Update();
	const K4E::Vector3 worldPosition = bodyTransform.worldTranslate_;

	// 演出終了時にボスの親Transformを解除し、ローカル座標をワールド座標へ戻す。
	bodyTransform.parent_ = nullptr;
	bodyTransform.translate_ = worldPosition;
	bodyTransform.Update();
}

/// -------------------------------------------------------------
/// シャドウ描画
/// -------------------------------------------------------------
void BossBase::DrawShadow()
{
	BaseCharacter::DrawShadow();

	if (attackComponent_)
	{
		attackComponent_->DrawShadow();
	}
}

void BossBase::DrawImGui()
{
#ifdef USE_IMGUI
	if (attackComponent_)
	{
		attackComponent_->DrawImGui();
	}
#endif
}

void BossBase::ApplyParameters()
{
	EnsureBossCommonParameters();
	const float maxHP = GetBossParameterOrDefault("maxHP", kDefaultBossMaxHP);
	if (statusComponent_)
	{
		statusComponent_->SetMaxHP(maxHP); // 最大HPだけを更新し、現在HPはコンポーネント側で範囲内に丸める。
	}

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
	ParameterManager::GetInstance()->UnregisterParameterApplier(kBossCommonGroup, this); // Finalize後の無効ポインタ呼び出しを防ぐ。
	g_bossDeathRuntimes.erase(this);

	if (attackComponent_)
	{
		attackComponent_->Finalize();
		attackComponent_.reset();
	}

	if (animationComponent_)
	{
		animationComponent_->Finalize();
		animationComponent_.reset();
	}

	if (movementComponent_)
	{
		movementComponent_->Finalize();
		movementComponent_.reset();
	}

	if (stateMachine_)
	{
		stateMachine_->Finalize();
		stateMachine_.reset();
	}

	if (statusComponent_)
	{
		statusComponent_->Finalize();
		statusComponent_.reset();
	}

	if (brain_)
	{
		brain_->Finalize();
		brain_.reset();
	}

	GetBodyParts().clear();
	GetBody().object.reset();
}

void BossBase::OnDamaged(float damage)
{
	if (!statusComponent_ || IsBossStatusDead(*this))
	{
		return;
	}

	const float hpBefore = statusComponent_->GetHP();
	statusComponent_->ApplyDamage(damage);

	std::ostringstream oss;
	oss << "[GuardianBoss] TakeDamage: damage=" << damage
		<< ", HP=" << statusComponent_->GetHP() << "/" << statusComponent_->GetMaxHP()
		<< ", before=" << hpBefore;
	Log(oss.str() + "\n");
}

void BossBase::OnBulletDamaged(float damage)
{
	OnDamaged(damage);
}

bool BossBase::ApplyDamageToTargetPlayer(float damage, const K4E::Vector3* attackPosition)
{
	if (!targetPlayer_ || damage <= 0.0f || IsBossStatusDead(*this))
	{
		return false;
	}

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

	if (stateMachine_)
	{
		stateMachine_->ChangeState(*this, BossState::Dead);
	}

	if (attackComponent_)
	{
		attackComponent_->ForceEndCurrentAttack();
	}

	BossDeathRuntime& runtime = GetBossDeathRuntime(*this);
	StartBossBreakApartDeath(*this, runtime);
}

bool BossBase::IsAlive() const
{
	return statusComponent_ ? statusComponent_->IsAlive() : false;
}

bool BossBase::IsDead() const
{
	if (!IsBossStatusDead(*this))
	{
		return false;
	}

	const auto it = g_bossDeathRuntimes.find(this);
	if (it == g_bossDeathRuntimes.end())
	{
		return true;
	}

	// GamePlayWorldはIsDead()でクリアアイテムを出すので、死亡演出が数秒進むまで外部には死亡完了を返さない。
	return it->second.timer >= kBossDeathPresentationSeconds;
}

float BossBase::GetHP() const
{
	return statusComponent_ ? statusComponent_->GetHP() : 0.0f;
}

float BossBase::GetMaxHP() const
{
	return statusComponent_ ? statusComponent_->GetMaxHP() : 0.0f;
}

float BossBase::GetHPRate() const
{
	return statusComponent_ ? statusComponent_->GetHPRate() : 0.0f;
}

K4E::Vector3 BossBase::GetDirectionToTargetXZOrForward(const K4E::Vector3& origin) const
{
	K4E::Vector3 toTarget{
		targetPosition_.x - origin.x,
		0.0f,
		targetPosition_.z - origin.z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq > 0.0001f)
	{
		const float invLen = 1.0f / std::sqrt(lenSq);
		return { toTarget.x * invLen, 0.0f, toTarget.z * invLen };
	}

	return { std::sin(GetYaw()), 0.0f, std::cos(GetYaw()) };
}

void BossBase::FaceDirectionXZImmediate(const K4E::Vector3& direction)
{
	const float lenSq = direction.x * direction.x + direction.z * direction.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	SetYaw(std::atan2(-direction.x, direction.z));
}

float BossBase::GetDistanceToTargetXZ() const
{
	const K4E::Vector3 from = GetPosition();
	const K4E::Vector3 to = targetPosition_;

	const float dx = to.x - from.x;
	const float dz = to.z - from.z;

	return std::sqrt(dx * dx + dz * dz);
}

bool BossBase::IsTargetInAttackRange() const
{
	return GetDistanceToTargetXZ() <= attackRange_;
}

void BossBase::RegisterAttack(std::unique_ptr<IBossAttack> attack)
{
	if (!attack)
	{
		return;
	}

	if (attackComponent_)
	{
		attackComponent_->RegisterAttack(std::move(attack));
	}
}

void BossBase::UpdateState(float deltaTime)
{
	(void)deltaTime;

	if (!stateMachine_)
	{
		return;
	}

	if (IsBossStatusDead(*this))
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
		if (IsTargetInAttackRange() && !IsAttackCoolingDown())
		{
			stateMachine_->ChangeState(*this, BossState::Attack);
		}
		else
		{
			stateMachine_->ChangeState(*this, BossState::Move);
		}
		break;

	case BossState::Move:
		if (IsTargetInAttackRange() && !IsAttackCoolingDown())
		{
			stateMachine_->ChangeState(*this, BossState::Attack);
		}
		break;

	case BossState::Attack:
		if (attackComponent_ && !attackComponent_->IsAttacking())
		{
			attackCooldownTimer_ = attackCooldownSec_;

			if (IsTargetInAttackRange())
			{
				stateMachine_->ChangeState(*this, BossState::Idle);
			}
			else
			{
				stateMachine_->ChangeState(*this, BossState::Move);
			}
		}
		break;

	case BossState::Stagger:
	case BossState::Down:
	case BossState::PhaseTransition:
	case BossState::Dead:
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
	if (movementComponent_)
	{
		movementComponent_->Update(*this, deltaTime);
	}
}

void BossBase::UpdateAttack(float deltaTime)
{
	if (!attackComponent_)
	{
		return;
	}

	attackComponent_->Update(deltaTime);
}

void BossBase::UpdateWeakPoints(float deltaTime)
{
	(void)deltaTime;
}

void BossBase::CheckDeath()
{
	if (IsBossStatusDead(*this) && state_ != BossState::Dead)
	{
		OnDead();
	}
}

K4E::Vector3 BossBase::GetPartWorldPosition(size_t partIndex)
{
	auto& parts = GetBodyParts();
	if (partIndex >= parts.size())
	{
		return GetCenterPosition();
	}

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

	const float distanceSq = dx * dx + dy * dy + dz * dz;
	const float sumRadius = attackRadius + targetRadius;

	return distanceSq <= (sumRadius * sumRadius);
}

BossHitResult BossBase::CheckDebugHitSphere(const K4E::Vector3& attackCenter, float attackRadius)
{
	BossHitResult result{};
	if (IsBossStatusDead(*this)) return result;

	const auto& indices = GetPartIndices();
	const float headRadius = 0.45f;
	const float bodyRadius = 0.85f;
	const float armRadius = 0.45f;
	const float legRadius = 0.50f;

	struct HitCheckInfo
	{
		BossHitPart part;
		Vector3 position;
		float radius;
		float damageMultiplier;
	};

	const HitCheckInfo hitChecks[] = {
		{ BossHitPart::Head, GetPartWorldPosition(indices.head), headRadius, 2.0f },
		{ BossHitPart::Body, GetCenterPosition(), bodyRadius, 1.0f },
		{ BossHitPart::LeftArm, GetPartWorldPosition(indices.leftArm), armRadius, 0.8f },
		{ BossHitPart::RightArm, GetPartWorldPosition(indices.rightArm), armRadius, 0.8f },
		{ BossHitPart::LeftLeg, GetPartWorldPosition(indices.leftLeg), legRadius, 0.9f },
		{ BossHitPart::RightLeg, GetPartWorldPosition(indices.rightLeg), legRadius, 0.9f },
	};

	for (const HitCheckInfo& check : hitChecks)
	{
		if (!IsSphereHit(attackCenter, attackRadius, check.position, check.radius))
		{
			continue;
		}

		result.isHit = true;
		result.part = check.part;
		result.hitPosition = check.position;
		result.damageMultiplier = check.damageMultiplier;
		return result;
	}

	return result;
}

void BossBase::ApplyDebugHitResult(const BossHitResult& hitResult, float baseDamage)
{
	if (!hitResult.isHit) return;

	const float finalDamage = baseDamage * hitResult.damageMultiplier;
	OnDamaged(finalDamage);
}
