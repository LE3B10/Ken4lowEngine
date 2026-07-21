#pragma once

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/GamePlayScene/Core/GamePlayStageContext.h"

#include <Actor.h>
#include <ActorWorld.h>
#include <InstancedModelComponent.h>
#include <SceneComponent.h>
#include <TextComponent.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// 崩落都市の脱出地点を、遠方から見える立体ビーコンと画面上の距離表示で案内する。
class Stage4EscapeGoalActor final : public K4E::Actor
{
private:
	static constexpr size_t kInstanceCount = 10u;

public:
	std::string GetClassTypeName() const override { return "Stage4EscapeGoalActor"; }

	void Initialize() override
	{
		auto& root = CreateRootComponent<K4E::SceneComponent>();
		root.SetName("Stage 4 Escape Goal Root");
		root.SetUpdateOrder(-100);

		auto& visual = AddComponent<K4E::InstancedModelComponent>();
		visual.SetName("Stage 4 Escape Goal Beacon Instances");
		visual.SetUpdateOrder(-20);
		visual.SetDrawOrder(4);
		visual.SetModelPath("Sample/cube.gltf");
		visual.SetInstanceCount(static_cast<int>(kInstanceCount));
		visual.SetCastShadowEnabled(true);
		visual.AttachTo(&root);
		visual_ = &visual;

		auto& guide = AddComponent<K4E::TextComponent>();
		guide.SetName("Stage 4 Escape Goal Guide");
		guide.SetDrawOrder(1410);
		guide.SetPosition({ 960.0f, 116.0f });
		guide.SetAnchor({ 0.5f, 0.5f });
		guide.SetFontName("DotGothic16");
		guide.SetFontSize(25.0f);
		guide.SetColor({ 0.30f, 0.92f, 1.0f, 1.0f });
		guide.SetText("青いビーコンが脱出地点");
		guide_ = &guide;

		SetName("Stage4EscapeGoal");
		SetLayer("StageObjective");
		AddTag("Stage4EscapeGoal");
		K4E::Actor::Initialize();
		ApplyBeaconTransforms(0.0f);
	}

	void Configure(const K4E::Vector3& goalPosition)
	{
		goalPosition_ = goalPosition;
		configured_ = true;
		ApplyBeaconTransforms(0.0f);
		if (visual_) visual_->Update(0.0f);
	}

	bool UpdateGuide(const K4E::Vector3& playerPosition, float deltaTime)
	{
		elapsedSeconds_ += std::max(0.0f, deltaTime);
		ApplyBeaconTransforms(elapsedSeconds_);

		const K4E::Vector3 delta = goalPosition_ - playerPosition;
		const float horizontalDistance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
		const float verticalDistance = std::fabs(delta.y);
		const bool reached = configured_ && horizontalDistance <= reachRadius_ && verticalDistance <= reachVerticalTolerance_;

		if (guide_)
		{
			char text[128]{};
			if (reached)
			{
				guide_->SetText("脱出地点に到達");
				guide_->SetColor({ 0.30f, 1.0f, 0.48f, 1.0f });
			}
			else if (horizontalDistance <= 18.0f)
			{
				std::snprintf(text, sizeof(text), "青い脱出エリアへ入れ　残り %.0fm", horizontalDistance);
				guide_->SetText(text);
				guide_->SetColor({ 0.30f, 0.92f, 1.0f, 1.0f });
			}
			else
			{
				std::snprintf(text, sizeof(text), "青いビーコンが脱出地点　残り %.0fm", horizontalDistance);
				guide_->SetText(text);
				guide_->SetColor({ 0.30f, 0.92f, 1.0f, 1.0f });
			}
		}
		return reached;
	}

private:
	void ApplyBeaconTransforms(float elapsedSeconds)
	{
		if (!visual_) return;
		const float pulse = 0.70f + 0.30f * std::sin(elapsedSeconds * 4.5f);
		const float rotation = elapsedSeconds * 0.85f;
		const K4E::Vector4 cyan{ 0.16f + pulse * 0.16f, 0.76f + pulse * 0.18f, 1.0f, 0.92f };
		const K4E::Vector4 white{ 0.78f, 0.96f, 1.0f, 1.0f };

		K4E::InstancedModelComponent::InstanceTransform pad{};
		pad.position = goalPosition_ + K4E::Vector3{ 0.0f, 0.10f, 0.0f };
		pad.scale = { 5.0f + pulse * 0.25f, 0.08f, 5.0f + pulse * 0.25f };
		pad.color = cyan;
		visual_->SetInstanceLocalTransform(0u, pad);

		K4E::InstancedModelComponent::InstanceTransform column{};
		column.position = goalPosition_ + K4E::Vector3{ 0.0f, 12.0f, 0.0f };
		column.scale = { 0.34f + pulse * 0.08f, 12.0f, 0.34f + pulse * 0.08f };
		column.color = cyan;
		visual_->SetInstanceLocalTransform(1u, column);

		for (size_t index = 0; index < 4u; ++index)
		{
			const float angle = rotation + static_cast<float>(index) * 1.57079632679f;
			const float x = std::cos(angle) * 4.1f;
			const float z = std::sin(angle) * 4.1f;
			K4E::InstancedModelComponent::InstanceTransform arm{};
			arm.position = goalPosition_ + K4E::Vector3{ x, 5.4f + pulse * 0.35f, z };
			arm.rotation = { 0.0f, -angle, 0.0f };
			arm.scale = { 1.8f, 0.12f, 0.24f };
			arm.color = index % 2u == 0u ? cyan : white;
			visual_->SetInstanceLocalTransform(2u + index, arm);
		}

		const std::array<K4E::Vector3, 4> cornerOffsets = {{
			{ -4.2f, 1.6f, -4.2f },
			{ 4.2f, 1.6f, -4.2f },
			{ -4.2f, 1.6f, 4.2f },
			{ 4.2f, 1.6f, 4.2f }
		}};
		for (size_t index = 0; index < cornerOffsets.size(); ++index)
		{
			K4E::InstancedModelComponent::InstanceTransform post{};
			post.position = goalPosition_ + cornerOffsets[index];
			post.scale = { 0.20f, 1.6f, 0.20f };
			post.color = cyan;
			visual_->SetInstanceLocalTransform(6u + index, post);
		}
	}

	K4E::InstancedModelComponent* visual_ = nullptr;
	K4E::TextComponent* guide_ = nullptr;
	K4E::Vector3 goalPosition_{ 0.0f, 1.0f, 181.0f };
	float elapsedSeconds_ = 0.0f;
	float reachRadius_ = 6.0f;
	float reachVerticalTolerance_ = 7.0f;
	bool configured_ = false;
};

/// Stage 4のGoalPointを可視化し、Playerが脱出範囲へ入った瞬間だけ共通Objectiveへ通知する。
class Stage4EscapeGoalRuntime final
{
public:
	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		const auto rule = stageContext.GetCurrentStageRule();
		active_ = rule.objectiveType == GamePlayStageContext::StageObjectiveType::ReachGoal;
		if (!active_) return;
		if (!stageContext.GetGoalPoints().empty())
		{
			goalPosition_ = stageContext.GetGoalPoints().front().position;
		}
		else
		{
			goalPosition_ = { 0.0f, 1.0f, 181.0f }; // JSONのGoalPoint欠落時も崩落都市をクリア不能にしない終端Fallback。
		}
	}

	void Finalize()
	{
		if (goalActor_) goalActor_->Destroy();
		goalActor_ = nullptr;
		goalPosition_ = { 0.0f, 1.0f, 181.0f };
		reached_ = false;
		active_ = false;
	}

	bool Update(IPlayerRuntime* player, K4E::ActorWorld& actorWorld, float deltaTime)
	{
		if (!active_ || reached_) return false;
		SpawnActorIfNeeded(actorWorld);
		if (!goalActor_ || !player) return false;
		if (!goalActor_->UpdateGuide(player->GetWorldPosition(), deltaTime)) return false;
		reached_ = true;
		return true; // 距離判定を一度だけ通知し、毎フレームの重複クリア要求を防ぐ。
	}

	bool IsActive() const { return active_; }
	bool IsReached() const { return reached_; }
	const K4E::Vector3& GetGoalPosition() const { return goalPosition_; }

private:
	void SpawnActorIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (goalActor_) return;
		auto& actor = actorWorld.SpawnActor<Stage4EscapeGoalActor>();
		actor.Configure(goalPosition_);
		actor.Update(0.0f);
		goalActor_ = &actor;
	}

	Stage4EscapeGoalActor* goalActor_ = nullptr;
	K4E::Vector3 goalPosition_{ 0.0f, 1.0f, 181.0f };
	bool reached_ = false;
	bool active_ = false;
};
