#pragma once

#include "BaseCharacter.h"
#include "BossTypes.h"
#include "AABB.h"
#include "WorldCollisionResolver.h"
#include <Vector3.h>

#include "BossBrain.h"
#include "BossStateMachine.h"
#include "BossStatusComponent.h"
#include "BossMovementComponent.h"
#include "BossAnimationComponent.h"
#include "BossAttackComponent.h"
#include "IBossAttack.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// デバッグ用の簡易ヒット部位。正式な部位所有はHumanoidVisualComponent側に置く。
enum class BossHitPart
{
	None,
	Head,
	Body,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg
};

/// Bossの簡易部位ヒット結果。
struct BossHitResult
{
	bool isHit = false;
	BossHitPart part = BossHitPart::None;
	K4E::Vector3 hitPosition{};
	float damageMultiplier = 1.0f;
};

class Player;
class BossPhaseComponent;
class BossWeakPointComponent;
class BossEffectComponent;
class BossSoundComponent;

/// Boss固有AI/攻撃を保持しつつ、HP・Movement・Collider・Animation・人型表示は共通Actor/Componentへ接続する基底。
class BossBase : public BaseCharacter
{
public:
	BossBase() = default;
	~BossBase() override;

	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;
	void DrawShadow() override;
	void DrawImGui() override;
	virtual void Finalize();

	/// 人型Bossは標準HumanoidVisualComponent構成をそのまま使用する。
	virtual void BuildBossParts() { BaseCharacter::Initialize(); }
	virtual void SetupAttacks() = 0;
	virtual void SetupBoss()
	{
		if (auto* visual = GetHumanoidVisualComponent()) visual->SetAllPartsVisible(true); // 共通人型表示をBossの初期表示状態へ揃える。
	}

	virtual void ApplyParameters();
	void ForceSyncWorldTransform();
	bool MoveWithWorldCollision(const K4E::Vector3& desiredPosition);
	void ClearRootParentKeepingWorldPosition();

	/// 既存の登場演出が参照するRoot Transform情報を共通HumanoidVisualのBodyから返す。
	bool HasRootParent() const { return GetBody().transform.parent_ != nullptr; }
	K4E::Vector3 GetRootLocalPosition() const { return GetBody().transform.translate_; }
	K4E::Vector3 GetRootWorldPosition() const { return GetBody().transform.worldTranslate_; }

	/// 個別BossがCollider種別ごとの処理を実装する。
	virtual void OnCollision(K4E::Collider* other) override = 0;

	virtual void OnDamaged(float damage);
	virtual void OnBulletDamaged(float damage);
	virtual bool ApplyDamageToTargetPlayer(float damage, const K4E::Vector3* attackPosition = nullptr);
	virtual void OnTargetPlayerDamaged(float damage);
	virtual void OnDead();

	bool IsAlive() const;
	bool IsDead() const;
	float GetHP() const;
	float GetMaxHP() const;
	float GetHPRate() const;

	BossState GetState() const { return state_; }
	void SetState(BossState state) { state_ = state; }
	BossPhase GetPhase() const { return phase_; }
	void SetPhase(BossPhase phase) { phase_ = phase; }

	void SetPosition(const K4E::Vector3& position) { SetCenterPosition(position); }
	K4E::Vector3 GetPosition() const { return GetCenterPosition(); }

	void SetYaw(float yaw)
	{
		auto rotation = GetBody().transform.rotate_;
		rotation.y = yaw;
		SetOrientation(rotation);
	}
	float GetYaw() const { return GetBody().transform.rotate_.y; }

	void SetTargetPosition(const K4E::Vector3& position) { targetPosition_ = position; }
	const K4E::Vector3& GetTargetPosition() const { return targetPosition_; }
	K4E::Vector3 GetDirectionToTargetXZOrForward(const K4E::Vector3& origin) const;
	void FaceDirectionXZImmediate(const K4E::Vector3& direction);
	float GetDistanceToTargetXZ() const;
	bool IsTargetInAttackRange() const;

	void SetTargetPlayer(Player* player) { targetPlayer_ = player; }
	Player* GetTargetPlayer() const { return targetPlayer_; }

	void SetStageObstacleAABBs(const std::vector<K4E::AABB>* aabbs) { stageObstacleAABBs_ = aabbs; }
	const std::vector<K4E::AABB>* GetStageObstacleAABBs() const { return stageObstacleAABBs_; }
	const K4E::WorldCollisionSettings& GetWorldCollisionSettings() const { return worldCollisionSettings_; }
	void SetWorldCollisionSettings(const K4E::WorldCollisionSettings& settings) { worldCollisionSettings_ = settings; }

	void SetAttackRange(float attackRange) { attackRange_ = attackRange; }
	float GetAttackRange() const { return attackRange_; }
	void SetAttackCooldown(float cooldownSec) { attackCooldownSec_ = cooldownSec; }
	float GetAttackCooldown() const { return attackCooldownSec_; }
	float GetAttackCooldownTimer() const { return attackCooldownTimer_; }
	bool IsAttackCoolingDown() const { return attackCooldownTimer_ > 0.0f; }
	void ResetAttackCooldown() { attackCooldownTimer_ = attackCooldownSec_; }

	void RegisterAttack(std::unique_ptr<IBossAttack> attack);
	BossAttackComponent* GetAttackComponent() { return attackComponent_.get(); }
	const BossAttackComponent* GetAttackComponent() const { return attackComponent_.get(); }
	BossAnimationComponent* GetAnimationComponent() { return animationComponent_.get(); }
	const BossAnimationComponent* GetAnimationComponent() const { return animationComponent_.get(); }
	BossMovementComponent* GetMovementComponent() { return movementComponent_.get(); }
	const BossMovementComponent* GetMovementComponent() const { return movementComponent_.get(); }
	K4E::CharacterMovementComponent* GetCharacterMovementComponent()
	{
		return movementComponent_ ? movementComponent_->GetCharacterMovementComponent() : nullptr;
	}
	const K4E::CharacterMovementComponent* GetCharacterMovementComponent() const
	{
		return movementComponent_ ? movementComponent_->GetCharacterMovementComponent() : nullptr;
	}
	BossStateMachine* GetStateMachine() { return stateMachine_.get(); }
	const BossStateMachine* GetStateMachine() const { return stateMachine_.get(); }
	BossStatusComponent* GetStatusComponent() { return statusComponent_.get(); }
	const BossStatusComponent* GetStatusComponent() const { return statusComponent_.get(); }
	BossBrain* GetBrain() { return brain_.get(); }
	const BossBrain* GetBrain() const { return brain_.get(); }

	BossPhaseComponent* GetPhaseComponent() { return phaseComponent_.get(); }
	const BossPhaseComponent* GetPhaseComponent() const { return phaseComponent_.get(); }
	BossWeakPointComponent* GetWeakPointComponent() { return weakPointComponent_.get(); }
	const BossWeakPointComponent* GetWeakPointComponent() const { return weakPointComponent_.get(); }
	BossEffectComponent* GetEffectComponent() { return effectComponent_.get(); }
	const BossEffectComponent* GetEffectComponent() const { return effectComponent_.get(); }
	BossSoundComponent* GetSoundComponent() { return soundComponent_.get(); }
	const BossSoundComponent* GetSoundComponent() const { return soundComponent_.get(); }

	void SetDamageCallback(std::function<void(float)> callback) { damageCallback_ = std::move(callback); }

	/// 攻撃アニメーションが使う腕のローカル回転を共通Visualの部位へ適用する。
	void SetLeftArmLocalRotate(const K4E::Vector3& rotate)
	{
		auto& parts = GetBodyParts();
		const auto index = GetPartIndices().leftArm;
		if (index < parts.size()) parts[index].transform.rotate_ = rotate;
	}
	void SetRightArmLocalRotate(const K4E::Vector3& rotate)
	{
		auto& parts = GetBodyParts();
		const auto index = GetPartIndices().rightArm;
		if (index < parts.size()) parts[index].transform.rotate_ = rotate;
	}

	K4E::Vector3 GetLeftArmRootWorldPosition()
	{
		return GetPartWorldPosition(GetPartIndices().leftArm);
	}
	K4E::Vector3 GetRightArmRootWorldPosition()
	{
		return GetPartWorldPosition(GetPartIndices().rightArm);
	}

	BossHitResult CheckDebugHitSphere(const K4E::Vector3& attackCenter, float attackRadius);
	void ApplyDebugHitResult(const BossHitResult& hitResult, float baseDamage);

protected:
	virtual void UpdateState(float deltaTime);
	virtual void UpdatePhase(float deltaTime);
	virtual void UpdateMovement(float deltaTime);
	virtual void UpdateAttack(float deltaTime);
	virtual void UpdateWeakPoints(float deltaTime);
	virtual void CheckDeath();
	K4E::Vector3 GetPartWorldPosition(size_t partIndex);
	bool IsSphereHit(const K4E::Vector3& attackCenter, float attackRadius, const K4E::Vector3& targetCenter, float targetRadius) const;

protected:
	std::unique_ptr<BossBrain> brain_;
	std::unique_ptr<BossStatusComponent> statusComponent_; // HP値は持たず、共通CharacterHealthComponentへのBoss API窓口だけを提供する。
	std::unique_ptr<BossStateMachine> stateMachine_;
	std::unique_ptr<BossMovementComponent> movementComponent_;
	std::unique_ptr<BossAnimationComponent> animationComponent_;
	std::unique_ptr<BossAttackComponent> attackComponent_;
	std::unique_ptr<BossPhaseComponent> phaseComponent_;
	std::unique_ptr<BossWeakPointComponent> weakPointComponent_;
	std::unique_ptr<BossEffectComponent> effectComponent_;
	std::unique_ptr<BossSoundComponent> soundComponent_;

	BossState state_ = BossState::Intro;
	BossPhase phase_ = BossPhase::Phase1;
	K4E::Vector3 targetPosition_{};
	Player* targetPlayer_ = nullptr;
	const std::vector<K4E::AABB>* stageObstacleAABBs_ = nullptr;
	K4E::WorldCollisionSettings worldCollisionSettings_{};
	float attackRange_ = 3.0f;
	float attackCooldownSec_ = 1.2f;
	float attackCooldownTimer_ = 0.0f;
	std::function<void(float)> damageCallback_{};
};
