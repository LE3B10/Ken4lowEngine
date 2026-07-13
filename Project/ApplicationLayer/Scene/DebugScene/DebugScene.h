#pragma once
#include "BaseScene.h"

#include <ActorWorld.h>
#include <Editor/ActorWorldEditorBridge.h>
#include <LightComponent.h>
#include <LightManager.h>
#include <PhysicsWorld.h>
#include <PhysicsDebugDraw.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace Ken4lowEngine
{
	class Actor;
	class Input;
}

namespace K4E = ::Ken4lowEngine;

class DebugScene : public K4E::BaseScene
{
public:
	DebugScene();
	~DebugScene() override;

	void Initialize() override;
	void Update() override;

	void BeginEditorPlay() override
	{
		// ActorWorldの複製とEditor状態退避はEditorPlaySessionManagerへ一元化する。
	}

	void EndEditorPlay() override
	{
		// Runtime World固有の後処理だけをScene Hookへ残し、Editor World復元はManagerへ任せる。
	}

	void CollectEditorObjects(std::vector<K4E::EditorObjectInfo>& outObjects) override
	{
		K4E::CollectActorWorldEditorObjects(actorWorld_, outObjects, "DebugScene");
	}

	K4E::ActorWorld* GetEditorActorWorld() override { return &actorWorld_; }

	void PrepareShadowPass() override
	{
		std::vector<K4E::LightManager::PunctualLightGPU> componentLights;
		for (const auto& actor : actorWorld_.GetActors())
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive()) continue;
			for (const K4E::LightComponent* lightComponent : actor->GetComponents<K4E::LightComponent>())
			{
				if (!lightComponent || !lightComponent->IsActiveInHierarchy() || !lightComponent->IsEnabled() ||
					lightComponent->GetLightType() == K4E::LightComponent::LightType::None) continue;
				const K4E::Vector3& color = lightComponent->GetColor();
				K4E::LightManager::PunctualLightGPU light{};
				light.lightType = lightComponent->GetLightTypeValue();
				light.color = { color.x, color.y, color.z, 1.0f };
				light.intensity = lightComponent->GetIntensity();
				light.position = lightComponent->GetWorldPosition();
				light.radius = lightComponent->GetRange();
				light.decay = lightComponent->GetDecay();
				light.direction = lightComponent->CalculateDirection();
				light.distance = lightComponent->GetRange();
				const float outerAngle = std::clamp(lightComponent->GetOuterAngle(), 0.1f, 179.0f);
				const float innerAngle = std::clamp(lightComponent->GetInnerAngle(), 0.0f, outerAngle);
				light.cosAngle = std::cos(outerAngle * std::numbers::pi_v<float> / 180.0f);
				light.cosFalloffStart = std::cos(innerAngle * std::numbers::pi_v<float> / 180.0f);
				light.areaSize = lightComponent->GetAreaSize();
				light.enabled = 1u;
				componentLights.push_back(light);
			}
		}
		K4E::LightManager::GetInstance()->SetLightComponentLights(componentLights);
	}

	void Draw3DObjects() override;
	void DrawShadowObjects() override;
	void Draw2DSprites() override;
	void Finalize() override;
	void DrawImGui() override;

private:
	void UpdateDebug();

private:

	K4E::Input* input_ = nullptr;
	bool isDebugCamera_ = false;

	K4E::ActorWorld actorWorld_;
	K4E::PhysicsWorld actorPhysicsWorld_;
	K4E::PhysicsDebugDraw actorPhysicsDebugDraw_;
};
