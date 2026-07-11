#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"
#include "GpuParticleEffectDesc.h"
#include "AnimationStateController.h"

#include <ActorWorld.h>
#include <Editor/ActorWorldEditorBridge.h>
#include <LightComponent.h>
#include <LightManager.h>
#include <PhysicsWorld.h>
#include <PhysicsDebugDraw.h>
#include <RigidbodyComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

class PhysicsDebugController;
class AnimationModelBatchTest;
class GpuParticlePreviewController;

namespace Ken4lowEngine
{
	class Actor;
	class AnimationModel;
	class Input;
	class InstancedObject3DRenderer;
}

namespace K4E = ::Ken4lowEngine;

class DebugScene : public K4E::BaseScene
{
public:
	DebugScene();
	~DebugScene() override;

	void Initialize() override;
	void Update() override;
	void UpdateEditor(float deltaTime) override;

	void BeginEditorPlay() override
	{
		editorActorSnapshots_.clear();
		for (const auto& actorOwner : actorWorld_.GetActors())
		{
			K4E::Actor* actor = actorOwner.get();
			K4E::SceneComponent* root = actor ? actor->GetRootComponent() : nullptr;
			if (!actor || !root || actor->IsPendingDestroy())
			{
				continue;
			}
			editorActorSnapshots_.push_back({ actor, root->GetLocalPosition(), root->GetLocalRotation(), root->GetLocalScale() });
		}
		// Play開始時点のEdit Worldだけを保存し、Pause再開では取り直さない。
	}

	void EndEditorPlay() override
	{
		for (const EditorActorTransformSnapshot& snapshot : editorActorSnapshots_)
		{
			const bool actorExists = std::any_of(actorWorld_.GetActors().begin(), actorWorld_.GetActors().end(),
				[&snapshot](const std::unique_ptr<K4E::Actor>& actor) { return actor.get() == snapshot.actor; });
			if (!actorExists || !snapshot.actor)
			{
				continue;
			}

			if (K4E::SceneComponent* root = snapshot.actor->GetRootComponent())
			{
				root->SetLocalPosition(snapshot.position);
				root->SetLocalRotation(snapshot.rotation);
				root->SetLocalScale(snapshot.scale);
				root->RefreshWorldTransform();
			}
			for (K4E::RigidbodyComponent* rigidbody : snapshot.actor->GetComponents<K4E::RigidbodyComponent>())
			{
				if (rigidbody) rigidbody->SetVelocity({ 0.0f, 0.0f, 0.0f });
			}
		}
		editorActorSnapshots_.clear(); // Stop後はPlay中の物理変化を残さず編集状態へ戻す。
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
	struct EditorActorTransformSnapshot
	{
		K4E::Actor* actor = nullptr;
		K4E::Vector3 position{};
		K4E::Vector3 rotation{};
		K4E::Vector3 scale{ 1.0f, 1.0f, 1.0f };
	};

	void UpdateDebug();
	void RebuildInstancingTest();
	void ReloadAnimationModelTest();
	void DrawAnimationModelTestImGui();
	void UpdateAnimationModelInputTest(float deltaTime);

	K4E::Input* input_ = nullptr;
	bool isDebugCamera_ = false;
	std::unique_ptr<CollisionManager> collisionManager_;
	std::unique_ptr<PhysicsDebugController> physicsDebugController_;
	std::unique_ptr<AnimationModelBatchTest> animationModelBatchTest_;
	std::unique_ptr<K4E::AnimationModel> animationModelTest_;
	std::unique_ptr<K4E::InstancedObject3DRenderer> instancingTestRenderer_;
	bool isInstancingTestEnabled_ = false;
	int instancingTestCount_ = 30000;
	float instancingTestSpacing_ = 2.0f;
	int instancingSafeCount_ = 30000;
	uint64_t instancingIndexBudget_ = 50'000'000ull;
	bool instancingAutoClamp_ = true;
	bool instancingRandomScale_ = false;
	bool instancingRandomRotation_ = false;
	bool instancingRandomColor_ = false;
	bool instancingFrustumCulling_ = false;

	K4E::GpuParticleEffectDesc editingGpuParticleEffect_;
	int selectedGpuParticleEmitterIndex_ = -1;
	bool showGpuParticleEditor_ = true;
	std::string gpuParticleEffectJsonPath_ = "Resources/JSON/GpuParticles/DebugEffect.json";
	std::string gpuParticleEditorStatus_ = "GPU Particle Editor ready.";
	bool gpuParticleEditorLastOperationSucceeded_ = true;
	std::unique_ptr<GpuParticlePreviewController> gpuParticlePreviewController_;
	bool gpuParticlePreviewAutoPlay_ = false;
	bool gpuParticlePreviewSelectedOnly_ = true;
	K4E::Vector3 gpuParticlePreviewPosition_{ 0.0f, 1.0f, 0.0f };
	uint32_t gpuParticlePreviewEmitCount_ = 32;

	std::string animationModelTestPath_ = "Animation/SampleHumanAnim.gltf";
	std::array<std::string, 3> animationModelTestLodPaths_{ "", "", "" };
	std::array<char, 260> animationModelTestPathBuffer_{};
	std::array<std::array<char, 260>, 3> animationModelTestLodPathBuffers_{};
	std::string animationModelTestStatus_ = "AnimationModel Test ready.";
	bool animationModelTestLoaded_ = false;
	bool animationModelTestLastOperationSucceeded_ = true;
	bool animationModelTestUseLods_ = false;
	bool animationModelTestForceLod_ = false;
	int animationModelTestForcedLodIndex_ = 0;
	float animationModelTestSpeed_ = 1.0f;
	bool animationModelTestLoop_ = true;
	bool animationModelTestShowDetailedDebugView_ = false;
	bool animationModelInputTestEnabled_ = false;
	bool animationStateControllerEnabled_ = false;
	bool animationModelUseCrossFadeForCombo_ = true;
	float animationModelCrossFadeDuration_ = 0.2f;
	float animationModelWalkSpeed_ = 2.0f;
	float animationModelRunSpeed_ = 5.0f;
	std::string animationModelRequestedAnimationName_;
	std::string animationModelIdleAnimationName_ = "Idle";
	std::string animationModelWalkAnimationName_ = "Walk";
	std::string animationModelRunAnimationName_ = "Run";
	std::string animationModelAttackAnimationName_ = "Attack";
	std::string animationModelDamageAnimationName_ = "Damage";
	std::string animationModelDeathAnimationName_ = "Death";
	std::array<char, 64> animationModelIdleNameBuffer_{};
	std::array<char, 64> animationModelWalkNameBuffer_{};
	std::array<char, 64> animationModelRunNameBuffer_{};
	std::array<char, 64> animationModelAttackNameBuffer_{};
	std::array<char, 64> animationModelDamageNameBuffer_{};
	std::array<char, 64> animationModelDeathNameBuffer_{};
	K4E::AnimationStateController animationStateController_{};

	K4E::ActorWorld actorWorld_;
	K4E::PhysicsWorld actorPhysicsWorld_;
	K4E::PhysicsDebugDraw actorPhysicsDebugDraw_;
	std::vector<EditorActorTransformSnapshot> editorActorSnapshots_;
};
