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

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine
{
	class AnimationModel;
	class Input;
	class InstancedObject3DRenderer;
}

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///　　　　　　　　　　デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public K4E::BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	DebugScene();
	~DebugScene() override;

	// 仮想初期化処理
	void Initialize() override;

	// 仮想更新処理
	void Update() override;
	// Editor停止中もDebugScene専用負荷検証だけは更新する。
	void UpdateEditor(float deltaTime) override;

	/// <summary>
	/// ActorWorldが所有するActor / Component階層を統合World Outlinerへ公開する。
	/// </summary>
	void CollectEditorObjects(std::vector<K4E::EditorObjectInfo>& outObjects) override
	{
		K4E::CollectActorWorldEditorObjects(actorWorld_, outObjects, "DebugScene");
	}

	// ShadowSystemがCasterを決める前にActorのLightComponentを同期する。
	void PrepareShadowPass() override
	{
		std::vector<K4E::LightManager::PunctualLightGPU> componentLights;
		for (const auto& actor : actorWorld_.GetActors())
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive())
			{
				continue;
			}

			for (const K4E::LightComponent* lightComponent : actor->GetComponents<K4E::LightComponent>())
			{
				if (!lightComponent || !lightComponent->IsActiveInHierarchy() || !lightComponent->IsEnabled() ||
					lightComponent->GetLightType() == K4E::LightComponent::LightType::None)
				{
					continue;
				}

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
				componentLights.push_back(light); // Editor操作直後のLight位置と種類を同じフレームのShadow Passへ渡す。
			}
		}

		K4E::LightManager::GetInstance()->SetLightComponentLights(componentLights);
	}

	// 仮想3D描画処理
	void Draw3DObjects() override;

	// 仮想シャドウマップ描画処理
	void DrawShadowObjects() override;

	// 仮想2D描画処理
	void Draw2DSprites() override;

	// 仮想終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// デバッグカメラの更新
	void UpdateDebug();
	// ImGui設定から大量配置データを再構築する。
	void RebuildInstancingTest();
	void ReloadAnimationModelTest();
	void DrawAnimationModelTestImGui();
	void UpdateAnimationModelInputTest(float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // Inputのポインタ
	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突管理マネージャー

	std::unique_ptr<PhysicsDebugController> physicsDebugController_; // DebugScene専用の物理確認コントローラ
	std::unique_ptr<AnimationModelBatchTest> animationModelBatchTest_; // AnimationModel大量描画のDebugScene専用テスト
	std::unique_ptr<K4E::AnimationModel> animationModelTest_; // 複数アニメーション切り替え確認用の単体モデル

	// 3万個のObject3Dを生成せず、専用GPUインスタンシング経路をON/OFFして負荷確認する。
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

	// DebugSceneからEffect / Emitter設定を編集するためのGPU Particle Editor用データ。
	K4E::GpuParticleEffectDesc editingGpuParticleEffect_;
	int selectedGpuParticleEmitterIndex_ = -1;
	bool showGpuParticleEditor_ = true;
	std::string gpuParticleEffectJsonPath_ = "Resources/JSON/GpuParticles/DebugEffect.json";
	std::string gpuParticleEditorStatus_ = "GPU Particle Editor ready.";
	bool gpuParticleEditorLastOperationSucceeded_ = true;

	// DebugScene専用のGPUパーティクル試射状態。本番EffectやGamePlaySceneとは共有しない。
	std::unique_ptr<GpuParticlePreviewController> gpuParticlePreviewController_;
	bool gpuParticlePreviewAutoPlay_ = false;
	bool gpuParticlePreviewSelectedOnly_ = true;
	K4E::Vector3 gpuParticlePreviewPosition_{ 0.0f, 1.0f, 0.0f };
	uint32_t gpuParticlePreviewEmitCount_ = 32;

	// DebugScene上でAnimationModelの複数clip・LOD・Skinningを単体確認するための設定。
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

private: /// ---------- テスト ---------- ///

	K4E::ActorWorld actorWorld_; // ActorWorldのテスト用

	K4E::PhysicsWorld actorPhysicsWorld_; // ActorComponent用Colliderを登録する物理World。
	K4E::PhysicsDebugDraw actorPhysicsDebugDraw_; // ActorComponent用ColliderをWireframe表示するDebug描画。
};
