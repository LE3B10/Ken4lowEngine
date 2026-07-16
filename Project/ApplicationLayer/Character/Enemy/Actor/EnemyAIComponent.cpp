#include "EnemyAIComponent.h"

#include "EnemyAttackComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDirectionEpsilon = 0.0001f;
		constexpr float kGoldenAngle = 2.39996323f;

		/// 旧MeleeEnemyと同じXZ長さと0判定で移動方向を正規化する。
		Vector3 NormalizeDirectionXZ(const Vector3& direction)
		{
			const float length = Vector3::LengthXZ(direction);
			if (length < kDirectionEpsilon) return {};
			return { direction.x / length, 0.0f, direction.z / length };
		}
	}

	void EnemyAIComponent::Initialize()
	{
		EnemyAStarNavigator::Settings settings{};
		settings.cellSize = 1.5f;
		settings.agentRadius = 0.9f;
		settings.searchRangeCells = 28;
		settings.waypointReachDistance = 0.85f;
		settings.repathIntervalSec = 0.25f;
		settings.disableCornerCutting = true;
		navigator_.SetSettings(settings); // 旧MeleeEnemyと同じNavigator実装を共有し、経路探索の差を増やさない。
		navigator_.SetWorldAABBs(navigationObstacles_);
		ResetBehavior();
	}

	void EnemyAIComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterMovementComponent* movement = owner ? owner->GetMovementComponent() : nullptr;
		EnemyAttackComponent* attack = owner ? owner->GetCharacterComponent<EnemyAttackComponent>() : nullptr;
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
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
			spawnOriginCaptured_ = true; // Spawn後の実座標を最初の更新で保存し、徘徊中心が原点へ戻らないようにする。
		}

		if (attack && attack->IsAttacking())
		{
			movement->Stop(); // Scratchは停止し、LungeのActive中だけ後段のCharge Behaviorが速度を上書きする。
			stateName_ = "Attacking";
			return;
		}

		const Vector3 current = root->GetWorldPosition();
		const bool hasLivingTarget = targetActor_ && !targetActor_->IsDead();
		if (hasLivingTarget)
		{
			const Vector3 target = targetActor_->GetTargetPosition();
			const Vector3 toTarget = target - current;
			distanceToTarget_ = Vector3::LengthXZ(toTarget);
			if (distanceToTarget_ <= detectRange_)
			{
				if (attack && attack->ShouldHoldForAttack())
				{
					movement->Stop();
					movement->FaceDirectionXZ(toTarget, rotateSpeed_, deltaTime);
					pathFound_ = false;
					stateName_ = "AttackRange";
					return;
				}

				Vector3 waypoint = target;
				pathFound_ = navigator_.GetNextWaypoint(current, target, current.y, deltaTime, waypoint);
				const Vector3 direction = NormalizeDirectionXZ((pathFound_ ? waypoint : target) - current);
				if (Vector3::LengthXZ(direction) < kDirectionEpsilon && distanceToTarget_ <= stopDistance_)
				{
					movement->Stop();
					movement->FaceDirectionXZ(toTarget, rotateSpeed_, deltaTime);
					stateName_ = "TargetHeightOutOfRange";
					return;
				}
				movement->FaceDirectionXZ(direction, rotateSpeed_, deltaTime);
				movement->SetVelocity(direction * GetChaseSpeed()); // 徘徊速度は維持し、Player発見後だけ追跡速度を引き上げる。
				stateName_ = pathFound_ ? "ChasePath" : "ChaseDirect";
				return;
			}
		}
		else
		{
			distanceToTarget_ = 0.0f;
		}

		pathFound_ = false;
		if (!wanderEnabled_ || wanderRadius_ <= 0.0f)
		{
			movement->Stop();
			stateName_ = hasLivingTarget ? "TargetOutOfRange" : "Idle";
			return;
		}

		wanderTimer_ = std::max(0.0f, wanderTimer_ - std::max(0.0f, deltaTime));
		const Vector3 toWanderTarget = wanderTarget_ - current;
		if (wanderTimer_ <= 0.0f || Vector3::LengthXZ(toWanderTarget) <= 0.6f)
		{
			wanderAngle_ = std::fmod(wanderAngle_ + kGoldenAngle, std::numbers::pi_v<float> * 2.0f);
			wanderTarget_ = spawnOrigin_ + Vector3{ std::cos(wanderAngle_) * wanderRadius_, 0.0f, std::sin(wanderAngle_) * wanderRadius_ };
			wanderTimer_ = wanderInterval_;
		}

		const Vector3 wanderDirection = NormalizeDirectionXZ(wanderTarget_ - current);
		movement->FaceDirectionXZ(wanderDirection, rotateSpeed_ * 0.65f, deltaTime);
		movement->SetVelocity(wanderDirection * (moveSpeed_ * wanderSpeedScale_));
		stateName_ = hasLivingTarget ? "WanderTargetOutOfRange" : "Wander";
	}

	void EnemyAIComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵AI");
		ImGui::Text("状態: %s", stateName_.c_str());
		ImGui::Text("基本速度: %.2f / 追跡速度: %.2f", moveSpeed_, GetChaseSpeed());
		ImGui::Text("追跡倍率: %.2f / 索敵距離: %.2f", chaseSpeedMultiplier_, detectRange_);
		ImGui::Text("回転速度: %.2f / Root Yaw: %.2f", rotateSpeed_, GetOwner() && GetOwner()->GetRootComponent() ? GetOwner()->GetRootComponent()->GetWorldRotation().y : 0.0f);
		ImGui::Text("Target水平距離: %.2f", distanceToTarget_);
		ImGui::Text("徘徊: %s / 半径 %.2f", wanderEnabled_ ? "有効" : "無効", wanderRadius_);
		ImGui::Text("A*経路: %s / %zu nodes", pathFound_ ? "有効" : "未生成", navigator_.GetCurrentPath().size());
#endif
	}

	void EnemyAIComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["MoveSpeed"] = moveSpeed_;
		outJson["ChaseSpeedMultiplier"] = chaseSpeedMultiplier_;
		outJson["RotateSpeed"] = rotateSpeed_;
		outJson["StopDistance"] = stopDistance_;
		outJson["AttackStartRange"] = attackStartRange_;
		outJson["DetectRange"] = detectRange_;
		outJson["WanderEnabled"] = wanderEnabled_;
		outJson["WanderRadius"] = wanderRadius_;
		outJson["WanderInterval"] = wanderInterval_;
		outJson["WanderSpeedScale"] = wanderSpeedScale_;
	}

	void EnemyAIComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		moveSpeed_ = std::max(0.0f, inJson.value("MoveSpeed", moveSpeed_));
		chaseSpeedMultiplier_ = std::clamp(inJson.value("ChaseSpeedMultiplier", chaseSpeedMultiplier_), 0.1f, 4.0f);
		rotateSpeed_ = std::max(0.0f, inJson.value("RotateSpeed", rotateSpeed_));
		stopDistance_ = std::max(0.0f, inJson.value("StopDistance", stopDistance_));
		attackStartRange_ = std::max(stopDistance_, inJson.value("AttackStartRange", attackStartRange_));
		detectRange_ = std::max(attackStartRange_, inJson.value("DetectRange", detectRange_));
		wanderEnabled_ = inJson.value("WanderEnabled", wanderEnabled_);
		wanderRadius_ = std::max(0.0f, inJson.value("WanderRadius", wanderRadius_));
		wanderInterval_ = std::max(0.1f, inJson.value("WanderInterval", wanderInterval_));
		wanderSpeedScale_ = std::clamp(inJson.value("WanderSpeedScale", wanderSpeedScale_), 0.0f, 1.0f);
	}

	void EnemyAIComponent::SetNavigationObstacles(const std::vector<AABB>* obstacles)
	{
		if (navigationObstacles_ == obstacles) return;
		navigationObstacles_ = obstacles;
		navigator_.SetWorldAABBs(obstacles);
		navigator_.Reset(); // 障害物集合が変わった場合は旧経路を次フレームへ持ち越さない。
	}

	void EnemyAIComponent::ApplyMoveSpeedMultiplier(float multiplier)
	{
		moveSpeed_ *= std::max(0.1f, multiplier); // Director倍率は基準速度へ一度だけ適用し、追跡倍率との積で最終速度を決める。
	}

	void EnemyAIComponent::StopBehavior()
	{
		behaviorEnabled_ = false;
		if (auto* owner = dynamic_cast<CharacterActor*>(GetOwner()))
		{
			if (CharacterMovementComponent* movement = owner->GetMovementComponent()) movement->Stop();
		}
		stateName_ = "Stopped";
	}

	void EnemyAIComponent::ResetBehavior()
	{
		behaviorEnabled_ = true;
		pathFound_ = false;
		spawnOriginCaptured_ = false;
		wanderTimer_ = 0.0f;
		wanderAngle_ = 0.0f;
		distanceToTarget_ = 0.0f;
		stateName_ = "Idle";
		navigator_.Reset();
	}
} // namespace Ken4lowEngine
