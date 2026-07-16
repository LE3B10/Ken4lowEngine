#pragma once

#include "EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"
#include "ApplicationLayer/Character/Enemy/Projectile/MidRangeBombProjectile.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"

#include <ActorComponent.h>
#include <AABB.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	class MidRangeEnemyAttackComponent;

	/// 中距離敵の徘徊、接近、間合い維持、後退をA*と共通Movement要求へ分離するComponent。
	class MidRangeEnemyAIComponent final : public ActorComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void DrawImGui() override;
		std::string GetClassTypeName() const override { return "MidRangeEnemyAIComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetTargetActor(CharacterActor* targetActor) { targetActor_ = targetActor; }
		void SetNavigationObstacles(const std::vector<AABB>* obstacles);
		void ApplyMoveSpeedMultiplier(float multiplier);
		void StopBehavior();
		void ResetBehavior();
		const std::string& GetStateName() const { return stateName_; }

	private:
		CharacterActor* targetActor_ = nullptr;
		const std::vector<AABB>* navigationObstacles_ = nullptr;
		EnemyAStarNavigator navigator_{};
		Vector3 spawnOrigin_{};
		Vector3 wanderTarget_{};
		float detectRange_ = 24.0f;
		float attackMinRange_ = 5.0f;
		float attackMaxRange_ = 14.0f;
		float idealRange_ = 9.0f;
		float tooCloseRange_ = 4.0f;
		float moveSpeed_ = 2.6f;
		float retreatSpeed_ = 2.8f;
		float rotateSpeed_ = 8.0f;
		float wanderRadius_ = 8.0f;
		float wanderInterval_ = 3.0f;
		float wanderSpeed_ = 1.8f;
		float wanderTimer_ = 0.0f;
		float wanderAngle_ = 0.0f;
		float distanceToTarget_ = 0.0f;
		bool behaviorEnabled_ = true;
		bool spawnOriginCaptured_ = false;
		bool pathFound_ = false;
		std::string stateName_ = "Idle";
	};

	/// 中距離敵のBomb投擲、Projectile寿命、HP低下時の自爆をActor本体から分離するComponent。
	class MidRangeEnemyAttackComponent final : public ActorComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Draw() override;
		void DrawImGui() override;
		void Finalize() override;
		std::string GetClassTypeName() const override { return "MidRangeEnemyAttackComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetTargetActor(CharacterActor* targetActor) { targetActor_ = targetActor; }
		void ApplyDifficultyMultipliers(float cooldownMultiplier, float damageMultiplier);
		void StopAttacking();
		void ResetAttackState();
		bool IsCasting() const { return casting_; }
		bool IsSuicideActive() const { return suicideActive_; }
		bool IsSuicideInvulnerable() const { return suicideActive_ && invincibleWhileSuicide_; }
		int GetAliveBombCount() const { return static_cast<int>(bombs_.size()); }

	private:
		void UpdateBombs(float deltaTime);
		void BeginCast();
		void LaunchBomb();
		void BeginSuicideMode();
		void UpdateSuicideMode(float deltaTime);
		void ExplodeSuicide();
		float GetTargetDistanceXZ() const;

	private:
		CharacterActor* targetActor_ = nullptr;
		std::vector<std::unique_ptr<::MidRangeBombProjectile>> bombs_{};
		::BombProjectileSettings bombSettings_{};
		float attackMinRange_ = 5.0f;
		float attackMaxRange_ = 14.0f;
		float cooldown_ = 2.0f;
		float cooldownTimer_ = 0.0f;
		float castTime_ = 0.45f;
		float castTimer_ = 0.0f;
		float throwHeightOffset_ = 1.6f;
		float suicideTriggerHpRate_ = 0.30f;
		float suicideTimeLimit_ = 5.0f;
		float suicideChaseSpeed_ = 5.0f;
		float suicideRotateSpeed_ = 12.0f;
		float suicideExplodeDistance_ = 1.8f;
		float suicideExplosionRadius_ = 4.0f;
		float suicideTimer_ = 0.0f;
		float blinkTimer_ = 0.0f;
		int suicideDamage_ = 60;
		bool attackEnabled_ = true;
		bool casting_ = false;
		bool thrownThisCast_ = false;
		bool suicideEnabled_ = true;
		bool suicideActive_ = false;
		bool invincibleWhileSuicide_ = true;
		std::string stateName_ = "Idle";
	};

	namespace MidRangeEnemyComponentDetail
	{
		inline Vector3 NormalizeXZ(const Vector3& value)
		{
			const float length = Vector3::LengthXZ(value);
			if (length <= 0.0001f) return {};
			return { value.x / length, 0.0f, value.z / length };
		}
	}

	inline void MidRangeEnemyAIComponent::Initialize()
	{
		EnemyAStarNavigator::Settings settings{};
		settings.cellSize = 1.5f;
		settings.agentRadius = 0.9f;
		settings.searchRangeCells = 28;
		settings.waypointReachDistance = 0.85f;
		settings.repathIntervalSec = 0.25f;
		settings.disableCornerCutting = true;
		navigator_.SetSettings(settings);
		navigator_.SetWorldAABBs(navigationObstacles_);
		ResetBehavior();
	}

	inline void MidRangeEnemyAIComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterMovementComponent* movement = owner ? owner->GetMovementComponent() : nullptr;
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		auto* attack = owner ? owner->GetCharacterComponent<MidRangeEnemyAttackComponent>() : nullptr;
		if (!owner || !movement || !root || !behaviorEnabled_ || owner->IsDead())
		{
			if (movement) movement->Stop();
			stateName_ = owner && owner->IsDead() ? "Dead" : "Disabled";
			return;
		}

		if (!spawnOriginCaptured_)
		{
			spawnOrigin_ = root->GetWorldPosition();
			wanderTarget_ = spawnOrigin_;
			spawnOriginCaptured_ = true; // 実際のSpawn位置を徘徊中心として一度だけ保存する。
		}

		if (attack && attack->IsSuicideActive())
		{
			stateName_ = "Suicide";
			return; // 自爆追跡中はAttack Componentだけが移動要求を出す。
		}
		if (attack && attack->IsCasting())
		{
			movement->Stop();
			if (targetActor_) movement->FaceDirectionXZ(targetActor_->GetTargetPosition() - root->GetWorldPosition(), rotateSpeed_, deltaTime);
			stateName_ = "Casting";
			return;
		}

		const Vector3 current = root->GetWorldPosition();
		if (targetActor_ && !targetActor_->IsDead())
		{
			const Vector3 toTarget = targetActor_->GetTargetPosition() - current;
			distanceToTarget_ = Vector3::LengthXZ(toTarget);
			if (distanceToTarget_ <= detectRange_)
			{
				if (distanceToTarget_ < attackMinRange_)
				{
					const Vector3 away = MidRangeEnemyComponentDetail::NormalizeXZ(toTarget) * -1.0f;
					movement->FaceDirectionXZ(toTarget, rotateSpeed_, deltaTime);
					movement->SetVelocity(away * retreatSpeed_);
					pathFound_ = false;
					stateName_ = distanceToTarget_ <= tooCloseRange_ ? "EmergencyRetreat" : "Retreat";
					return;
				}
				if (distanceToTarget_ > attackMaxRange_)
				{
					Vector3 waypoint = targetActor_->GetTargetPosition();
					pathFound_ = navigator_.GetNextWaypoint(current, waypoint, current.y, deltaTime, waypoint);
					const Vector3 direction = MidRangeEnemyComponentDetail::NormalizeXZ(waypoint - current);
					movement->FaceDirectionXZ(direction, rotateSpeed_, deltaTime);
					movement->SetVelocity(direction * moveSpeed_);
					stateName_ = pathFound_ ? "ApproachPath" : "ApproachDirect";
					return;
				}

				movement->Stop();
				movement->FaceDirectionXZ(toTarget, rotateSpeed_, deltaTime);
				pathFound_ = false;
				stateName_ = std::abs(distanceToTarget_ - idealRange_) <= 1.5f ? "IdealRange" : "AttackRange";
				return;
			}
		}
		else
		{
			distanceToTarget_ = 0.0f;
		}

		pathFound_ = false;
		wanderTimer_ = std::max(0.0f, wanderTimer_ - std::max(0.0f, deltaTime));
		if (wanderTimer_ <= 0.0f || Vector3::LengthXZ(wanderTarget_ - current) <= 1.0f)
		{
			wanderAngle_ = std::fmod(wanderAngle_ + 2.39996323f, std::numbers::pi_v<float> * 2.0f);
			wanderTarget_ = spawnOrigin_ + Vector3{ std::cos(wanderAngle_) * wanderRadius_, 0.0f, std::sin(wanderAngle_) * wanderRadius_ };
			wanderTimer_ = wanderInterval_;
		}
		const Vector3 direction = MidRangeEnemyComponentDetail::NormalizeXZ(wanderTarget_ - current);
		movement->FaceDirectionXZ(direction, rotateSpeed_ * 0.65f, deltaTime);
		movement->SetVelocity(direction * wanderSpeed_);
		stateName_ = "Wander";
	}

	inline void MidRangeEnemyAIComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("中距離敵AI");
		ImGui::Text("状態: %s", stateName_.c_str());
		ImGui::Text("距離: %.2f / 攻撃 %.2f - %.2f", distanceToTarget_, attackMinRange_, attackMaxRange_);
		ImGui::Text("移動: %.2f / 後退: %.2f / A*: %s", moveSpeed_, retreatSpeed_, pathFound_ ? "有効" : "なし");
#endif
	}

	inline void MidRangeEnemyAIComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["DetectRange"] = detectRange_;
		outJson["AttackMinRange"] = attackMinRange_;
		outJson["AttackMaxRange"] = attackMaxRange_;
		outJson["IdealRange"] = idealRange_;
		outJson["TooCloseRange"] = tooCloseRange_;
		outJson["MoveSpeed"] = moveSpeed_;
		outJson["RetreatSpeed"] = retreatSpeed_;
		outJson["RotateSpeed"] = rotateSpeed_;
		outJson["WanderRadius"] = wanderRadius_;
		outJson["WanderInterval"] = wanderInterval_;
		outJson["WanderSpeed"] = wanderSpeed_;
	}

	inline void MidRangeEnemyAIComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		detectRange_ = std::max(1.0f, inJson.value("DetectRange", detectRange_));
		attackMinRange_ = std::max(0.0f, inJson.value("AttackMinRange", attackMinRange_));
		attackMaxRange_ = std::max(attackMinRange_ + 0.1f, inJson.value("AttackMaxRange", attackMaxRange_));
		idealRange_ = std::clamp(inJson.value("IdealRange", idealRange_), attackMinRange_, attackMaxRange_);
		tooCloseRange_ = std::clamp(inJson.value("TooCloseRange", tooCloseRange_), 0.0f, attackMinRange_);
		moveSpeed_ = std::max(0.0f, inJson.value("MoveSpeed", moveSpeed_));
		retreatSpeed_ = std::max(0.0f, inJson.value("RetreatSpeed", retreatSpeed_));
		rotateSpeed_ = std::max(0.0f, inJson.value("RotateSpeed", rotateSpeed_));
		wanderRadius_ = std::max(0.0f, inJson.value("WanderRadius", wanderRadius_));
		wanderInterval_ = std::max(0.1f, inJson.value("WanderInterval", wanderInterval_));
		wanderSpeed_ = std::max(0.0f, inJson.value("WanderSpeed", wanderSpeed_));
	}

	inline void MidRangeEnemyAIComponent::SetNavigationObstacles(const std::vector<AABB>* obstacles)
	{
		navigationObstacles_ = obstacles;
		navigator_.SetWorldAABBs(obstacles);
		navigator_.Reset();
	}

	inline void MidRangeEnemyAIComponent::ApplyMoveSpeedMultiplier(float multiplier)
	{
		const float safeMultiplier = std::max(0.1f, multiplier);
		moveSpeed_ *= safeMultiplier;
		retreatSpeed_ *= safeMultiplier;
		wanderSpeed_ *= safeMultiplier; // Director倍率は3種類の移動出力へ同じ比率で一度だけ適用する。
	}

	inline void MidRangeEnemyAIComponent::StopBehavior()
	{
		behaviorEnabled_ = false;
		if (auto* owner = dynamic_cast<CharacterActor*>(GetOwner()))
		{
			if (CharacterMovementComponent* movement = owner->GetMovementComponent()) movement->Stop();
		}
		stateName_ = "Stopped";
	}

	inline void MidRangeEnemyAIComponent::ResetBehavior()
	{
		behaviorEnabled_ = true;
		spawnOriginCaptured_ = false;
		pathFound_ = false;
		wanderTimer_ = 0.0f;
		wanderAngle_ = 0.0f;
		distanceToTarget_ = 0.0f;
		stateName_ = "Idle";
		navigator_.Reset();
	}

	inline void MidRangeEnemyAttackComponent::Initialize()
	{
		ResetAttackState(); // BombProjectileSettingsの既定値またはFromJson済み調整値を上書きせず、実行時状態だけを初期化する。
	}

	inline void MidRangeEnemyAttackComponent::Update(float deltaTime)
	{
		UpdateBombs(deltaTime);
		auto* owner = dynamic_cast<EnemyActor*>(GetOwner());
		if (!owner || owner->IsDead()) return;

		if (suicideActive_)
		{
			UpdateSuicideMode(deltaTime);
			return;
		}
		if (suicideEnabled_ && owner->GetHpRate() <= suicideTriggerHpRate_)
		{
			BeginSuicideMode();
			return;
		}
		if (!attackEnabled_ || !targetActor_ || targetActor_->IsDead()) return;

		cooldownTimer_ = std::max(0.0f, cooldownTimer_ - std::max(0.0f, deltaTime));
		if (casting_)
		{
			castTimer_ += std::max(0.0f, deltaTime);
			if (!thrownThisCast_ && castTimer_ >= castTime_)
			{
				LaunchBomb();
				thrownThisCast_ = true;
			}
			if (castTimer_ >= castTime_ + 0.25f)
			{
				casting_ = false;
				cooldownTimer_ = cooldown_;
				stateName_ = "Cooldown";
			}
			return;
		}

		const float distance = GetTargetDistanceXZ();
		if (cooldownTimer_ <= 0.0f && distance >= attackMinRange_ && distance <= attackMaxRange_) BeginCast();
	}

	inline void MidRangeEnemyAttackComponent::Draw()
	{
		for (const auto& bomb : bombs_) if (bomb) bomb->Draw();
	}

	inline void MidRangeEnemyAttackComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("中距離敵攻撃");
		ImGui::Text("状態: %s / Bomb: %d", stateName_.c_str(), GetAliveBombCount());
		ImGui::Text("Cooldown: %.2f / Cast: %.2f", cooldownTimer_, castTimer_);
		ImGui::Text("直撃: %d / 爆発: %d", bombSettings_.directHitDamage, bombSettings_.explosionDamage);
		ImGui::Text("自爆: %s / %.2f", suicideActive_ ? "有効" : "待機", suicideTimer_);
#endif
	}

	inline void MidRangeEnemyAttackComponent::Finalize()
	{
		bombs_.clear();
	}

	inline void MidRangeEnemyAttackComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["AttackMinRange"] = attackMinRange_;
		outJson["AttackMaxRange"] = attackMaxRange_;
		outJson["Cooldown"] = cooldown_;
		outJson["CastTime"] = castTime_;
		outJson["ThrowHeightOffset"] = throwHeightOffset_;
		outJson["DirectHitDamage"] = bombSettings_.directHitDamage;
		outJson["ExplosionDamage"] = bombSettings_.explosionDamage;
		outJson["ExplosionRadius"] = bombSettings_.explosionRadius;
		outJson["SuicideEnabled"] = suicideEnabled_;
		outJson["SuicideTriggerHpRate"] = suicideTriggerHpRate_;
		outJson["SuicideTimeLimit"] = suicideTimeLimit_;
		outJson["SuicideChaseSpeed"] = suicideChaseSpeed_;
		outJson["SuicideExplosionRadius"] = suicideExplosionRadius_;
		outJson["SuicideDamage"] = suicideDamage_;
	}

	inline void MidRangeEnemyAttackComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		attackMinRange_ = std::max(0.0f, inJson.value("AttackMinRange", attackMinRange_));
		attackMaxRange_ = std::max(attackMinRange_ + 0.1f, inJson.value("AttackMaxRange", attackMaxRange_));
		cooldown_ = std::max(0.05f, inJson.value("Cooldown", cooldown_));
		castTime_ = std::max(0.0f, inJson.value("CastTime", castTime_));
		throwHeightOffset_ = inJson.value("ThrowHeightOffset", throwHeightOffset_);
		bombSettings_.directHitDamage = std::max(1, inJson.value("DirectHitDamage", bombSettings_.directHitDamage));
		bombSettings_.explosionDamage = std::max(0, inJson.value("ExplosionDamage", bombSettings_.explosionDamage));
		bombSettings_.explosionRadius = std::max(0.1f, inJson.value("ExplosionRadius", bombSettings_.explosionRadius));
		suicideEnabled_ = inJson.value("SuicideEnabled", suicideEnabled_);
		suicideTriggerHpRate_ = std::clamp(inJson.value("SuicideTriggerHpRate", suicideTriggerHpRate_), 0.0f, 1.0f);
		suicideTimeLimit_ = std::max(0.1f, inJson.value("SuicideTimeLimit", suicideTimeLimit_));
		suicideChaseSpeed_ = std::max(0.0f, inJson.value("SuicideChaseSpeed", suicideChaseSpeed_));
		suicideExplosionRadius_ = std::max(0.1f, inJson.value("SuicideExplosionRadius", suicideExplosionRadius_));
		suicideDamage_ = std::max(1, inJson.value("SuicideDamage", suicideDamage_));
	}

	inline void MidRangeEnemyAttackComponent::ApplyDifficultyMultipliers(float cooldownMultiplier, float damageMultiplier)
	{
		cooldown_ = std::max(0.05f, cooldown_ * std::max(0.1f, cooldownMultiplier));
		const float damageScale = std::max(0.1f, damageMultiplier);
		bombSettings_.directHitDamage = std::max(1, static_cast<int>(std::round(bombSettings_.directHitDamage * damageScale)));
		bombSettings_.explosionDamage = std::max(1, static_cast<int>(std::round(bombSettings_.explosionDamage * damageScale)));
		suicideDamage_ = std::max(1, static_cast<int>(std::round(suicideDamage_ * damageScale)));
	}

	inline void MidRangeEnemyAttackComponent::StopAttacking()
	{
		attackEnabled_ = false;
		casting_ = false;
		suicideActive_ = false;
		if (auto* owner = dynamic_cast<EnemyActor*>(GetOwner())) owner->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}

	inline void MidRangeEnemyAttackComponent::ResetAttackState()
	{
		bombs_.clear();
		cooldownTimer_ = 0.0f;
		castTimer_ = 0.0f;
		suicideTimer_ = 0.0f;
		blinkTimer_ = 0.0f;
		attackEnabled_ = true;
		casting_ = false;
		thrownThisCast_ = false;
		suicideActive_ = false;
		stateName_ = "Idle";
	}

	inline void MidRangeEnemyAttackComponent::UpdateBombs(float deltaTime)
	{
		for (const auto& bomb : bombs_) if (bomb) bomb->Update(deltaTime);
		std::erase_if(bombs_, [](const std::unique_ptr<::MidRangeBombProjectile>& bomb) { return !bomb || !bomb->IsAlive(); });
	}

	inline void MidRangeEnemyAttackComponent::BeginCast()
	{
		casting_ = true;
		thrownThisCast_ = false;
		castTimer_ = 0.0f;
		stateName_ = "Casting";
		if (auto* owner = dynamic_cast<CharacterActor*>(GetOwner()))
		{
			if (CharacterMovementComponent* movement = owner->GetMovementComponent()) movement->Stop();
		}
	}

	inline void MidRangeEnemyAttackComponent::LaunchBomb()
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		if (!owner || !targetActor_ || !owner->GetRootComponent()) return;
		auto bomb = std::make_unique<::MidRangeBombProjectile>();
		bomb->Initialize();
		Vector3 start = owner->GetRootComponent()->GetWorldPosition();
		start.y += throwHeightOffset_;
		bomb->Launch(start, targetActor_->GetTargetPosition(), bombSettings_);
		bombs_.push_back(std::move(bomb)); // Projectileの所有権は攻撃Componentへ集約し、Actor本体へ配列を戻さない。
		stateName_ = "Thrown";
	}

	inline void MidRangeEnemyAttackComponent::BeginSuicideMode()
	{
		suicideActive_ = true;
		casting_ = false;
		suicideTimer_ = 0.0f;
		blinkTimer_ = 0.0f;
		stateName_ = "SuicideChase";
	}

	inline void MidRangeEnemyAttackComponent::UpdateSuicideMode(float deltaTime)
	{
		auto* owner = dynamic_cast<EnemyActor*>(GetOwner());
		if (!owner || !targetActor_ || targetActor_->IsDead())
		{
			ExplodeSuicide();
			return;
		}
		suicideTimer_ += std::max(0.0f, deltaTime);
		blinkTimer_ += std::max(0.0f, deltaTime);
		const Vector3 toTarget = targetActor_->GetTargetPosition() - owner->GetCenterPosition();
		if (CharacterMovementComponent* movement = owner->GetMovementComponent())
		{
			movement->FaceDirectionXZ(toTarget, suicideRotateSpeed_, deltaTime);
			movement->SetVelocity(MidRangeEnemyComponentDetail::NormalizeXZ(toTarget) * suicideChaseSpeed_);
		}
		const float blink = std::sin(blinkTimer_ * 10.0f) >= 0.0f ? 1.0f : 0.15f;
		owner->SetColor({ 1.0f, blink, 0.1f, 1.0f });
		if (Vector3::LengthXZ(toTarget) <= suicideExplodeDistance_ || suicideTimer_ >= suicideTimeLimit_) ExplodeSuicide();
	}

	inline void MidRangeEnemyAttackComponent::ExplodeSuicide()
	{
		auto* owner = dynamic_cast<EnemyActor*>(GetOwner());
		if (!owner || !suicideActive_) return;
		suicideActive_ = false;
		if (CharacterMovementComponent* movement = owner->GetMovementComponent()) movement->Stop();
		const float targetDistance = targetActor_ ? Vector3::Length(targetActor_->GetTargetPosition() - owner->GetCenterPosition()) : suicideExplosionRadius_ + 1.0f;
		if (targetDistance <= suicideExplosionRadius_)
		{
			if (auto* runtime = dynamic_cast<::IPlayerRuntime*>(targetActor_)) runtime->ApplyRuntimeDamage(static_cast<float>(suicideDamage_));
			else if (targetActor_) targetActor_->ApplyDamage(static_cast<float>(suicideDamage_));
		}
		owner->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		owner->KillAfterSuicide(); // 自爆中の無敵判定を通さずEnemyBaseの死亡演出へ一度だけ接続する。
		stateName_ = "Exploded";
	}

	inline float MidRangeEnemyAttackComponent::GetTargetDistanceXZ() const
	{
		const auto* owner = dynamic_cast<const CharacterActor*>(GetOwner());
		if (!owner || !targetActor_) return 0.0f;
		return Vector3::LengthXZ(targetActor_->GetTargetPosition() - owner->GetTargetPosition());
	}
} // namespace Ken4lowEngine
