#define NOMINMAX
#include "EnemyEffectComponent.h"

#include <Actor.h>
#include <Object3D.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void EnemyEffectComponent::Initialize()
	{
		effectSystem_.Initialize();
		ResetEffectState();
	}

	void EnemyEffectComponent::Update(float deltaTime)
	{
		if (!deathEffectActive_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
		deathTimer_ += deltaTime;
		if (deathTimer_ < deathPresentationDuration_) return;

		deathEffectActive_ = false;
		if (Actor* owner = GetOwner())
		{
			if (auto* visual = owner->GetComponent<HumanoidVisualComponent>()) visual->SetAllPartsVisible(false);
		}
	}

	void EnemyEffectComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵Effect");
		ImGui::Text("Hit: %d / Death: %d", hitEffectCount_, deathEffectCount_);
		ImGui::Text("死亡演出: %s %.2f / %.2f", deathEffectActive_ ? "再生中" : "停止", deathTimer_, deathPresentationDuration_);
#endif
	}

	void EnemyEffectComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["DeathPresentationDuration"] = deathPresentationDuration_;
	}

	void EnemyEffectComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		deathPresentationDuration_ = std::max(0.1f, inJson.value("DeathPresentationDuration", deathPresentationDuration_));
	}

	void EnemyEffectComponent::TriggerHitEffect(const Vector3& worldPosition)
	{
		effectSystem_.SpawnHitEffect(worldPosition);
		++hitEffectCount_;
	}

	void EnemyEffectComponent::TriggerDeathEffect(const Vector3& worldPosition)
	{
		if (deathEffectActive_) return;
		effectSystem_.SpawnDeathEffect(worldPosition);
		deathEffectActive_ = true;
		deathTimer_ = 0.0f;
		++deathEffectCount_;

		if (Actor* owner = GetOwner())
		{
			if (auto* visual = owner->GetComponent<HumanoidVisualComponent>())
			{
				for (HumanoidVisualComponent::BodyPart& part : visual->GetParts())
				{
					if (part.object) part.object->SetColor({ 1.0f, 0.35f, 0.25f, 1.0f });
				}
			}
		}
	}

	void EnemyEffectComponent::ResetEffectState()
	{
		deathTimer_ = 0.0f;
		hitEffectCount_ = 0;
		deathEffectCount_ = 0;
		deathEffectActive_ = false;
		if (Actor* owner = GetOwner())
		{
			if (auto* visual = owner->GetComponent<HumanoidVisualComponent>())
			{
				visual->SetAllPartsVisible(true);
				for (HumanoidVisualComponent::BodyPart& part : visual->GetParts())
				{
					if (part.object) part.object->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
				}
			}
		}
	}
} // namespace Ken4lowEngine
