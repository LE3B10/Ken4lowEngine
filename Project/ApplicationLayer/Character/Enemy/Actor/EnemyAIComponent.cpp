#include "EnemyAIComponent.h"

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
		constexpr float kPi = std::numbers::pi_v<float>;
		constexpr float kTwoPi = kPi * 2.0f;

		/// 旧MeleeEnemyと同じXZ長さと0判定で移動方向を正規化する。
		Vector3 NormalizeDirectionXZ(const Vector3& direction)
		{
			const float length = Vector3::LengthXZ(direction);
			if (length < kDirectionEpsilon) return {};
			return { direction.x / length, 0.0f, direction.z / length };
		}

		/// Yaw差分を-πから+πへ正規化し、常に短い向きへ回転させる。
		float WrapAngle(float angle)
		{
			angle = std::fmod(angle + kPi, kTwoPi);
			if (angle < 0.0f) angle += kTwoPi;
			return angle - kPi;
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
		navigator_.SetSettings(settings); // 判定値を変えず、旧MeleeEnemyと同じNavigator実装を共有する。
		navigator_.SetWorldAABBs(navigationObstacles_);
		ResetBehavior();
	}

	void EnemyAIComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterMovementComponent* movement = owner ? owner->GetMovementComponent() : nullptr;
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		if (!owner || !movement || !root || !behaviorEnabled_ || owner->IsDead())
		{
			if (movement) movement->Stop();
			stateName_ = owner && owner->IsDead() ? "Dead" : "Disabled";
			return;
		}

		if (!targetActor_ || targetActor_->IsDead())
		{
			movement->Stop();
			pathFound_ = false;
			stateName_ = "NoTarget";
			return;
		}

		const Vector3 current = root->GetWorldPosition();
		const Vector3 target = targetActor_->GetTargetPosition();
		distanceToTarget_ = Vector3::LengthXZ(target - current);
		if (distanceToTarget_ <= attackStartRange_)
		{
			movement->Stop();
			FaceDirection(*root, target - current, deltaTime); // 停止後もTargetへ正面を合わせたまま攻撃する。
			stateName_ = "AttackRange";
			return;
		}

		Vector3 waypoint = target;
		pathFound_ = navigator_.GetNextWaypoint(current, target, current.y, deltaTime, waypoint);
		const Vector3 direction = NormalizeDirectionXZ((pathFound_ ? waypoint : target) - current);
		FaceDirection(*root, direction, deltaTime);
		movement->SetVelocity(direction * moveSpeed_); // 移動積分は共通Movementだけが実行し、AI側では位置を直接変更しない。
		stateName_ = pathFound_ ? "ChasePath" : "ChaseDirect";
	}

	void EnemyAIComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵AI");
		ImGui::Text("状態: %s", stateName_.c_str());
		ImGui::Text("移動速度: %.2f", moveSpeed_);
		ImGui::Text("回転速度: %.2f / Root Yaw: %.2f", rotateSpeed_, GetOwner() && GetOwner()->GetRootComponent() ? GetOwner()->GetRootComponent()->GetWorldRotation().y : 0.0f);
		ImGui::Text("Target距離: %.2f", distanceToTarget_);
		ImGui::Text("A*経路: %s / %zu nodes", pathFound_ ? "有効" : "未生成", navigator_.GetCurrentPath().size());
#endif
	}

	void EnemyAIComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["MoveSpeed"] = moveSpeed_;
		outJson["RotateSpeed"] = rotateSpeed_;
		outJson["StopDistance"] = stopDistance_;
		outJson["AttackStartRange"] = attackStartRange_;
	}

	void EnemyAIComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		moveSpeed_ = std::max(0.0f, inJson.value("MoveSpeed", moveSpeed_));
		rotateSpeed_ = std::max(0.0f, inJson.value("RotateSpeed", rotateSpeed_));
		stopDistance_ = std::max(0.0f, inJson.value("StopDistance", stopDistance_));
		attackStartRange_ = std::max(stopDistance_, inJson.value("AttackStartRange", attackStartRange_));
	}

	void EnemyAIComponent::SetNavigationObstacles(const std::vector<AABB>* obstacles)
	{
		if (navigationObstacles_ == obstacles) return; // PIE中の参照再接続で毎フレーム経路を破棄しない。
		navigationObstacles_ = obstacles;
		navigator_.SetWorldAABBs(obstacles);
		navigator_.Reset(); // 障害物集合が変わった場合は旧経路を次フレームへ持ち越さない。
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
		distanceToTarget_ = 0.0f;
		stateName_ = "Idle";
		navigator_.Reset();
	}

	void EnemyAIComponent::FaceDirection(SceneComponent& root, const Vector3& direction, float deltaTime)
	{
		const Vector3 normalized = NormalizeDirectionXZ(direction);
		if (Vector3::LengthXZ(normalized) < kDirectionEpsilon || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

		const float targetYaw = std::atan2(-normalized.x, normalized.z); // モデルの+Z正面を旧Enemyと同じワールド方向へ合わせる。
		Vector3 rotation = root.GetLocalRotation();
		const float maxStep = rotateSpeed_ * deltaTime;
		const float deltaYaw = std::clamp(WrapAngle(targetYaw - rotation.y), -maxStep, maxStep);
		rotation.y = WrapAngle(rotation.y + deltaYaw);
		root.SetLocalRotation(rotation);
		root.RefreshWorldTransform(); // Root更新後に動くVisual、Collider、Targetへ同じフレームのYawを伝播する。
	}
} // namespace Ken4lowEngine
