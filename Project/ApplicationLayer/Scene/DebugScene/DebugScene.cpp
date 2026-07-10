#define NOMINMAX
#include "DebugScene.h"
#include "PhysicsDebugController.h"
#include "AnimationModelBatchTest.h"
#include "GpuParticlePreviewController.h"
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <LightManager.h>
#include <GameTimer.h>
#include <InstancedObject3DRenderer.h>
#include <GpuParticleEffectEditor.h>
#include <AnimationModel.h>

#include "DebugActorRegistration.h"
#include "TestActor.h"
#include "TestGroundActor.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Ken4lowEngine;

namespace
{
	constexpr const char* kModelSourceRoot = "Resources/Models/Sources/";
	constexpr const char* kModelRoot = "Resources/Models/";

	void CopyToBuffer(const std::string& text, char* buffer, size_t bufferSize)
	{
		if (!buffer || bufferSize == 0) { return; }
		std::snprintf(buffer, bufferSize, "%s", text.c_str());
	}

	bool StartsWith(const std::string& text, const char* prefix)
	{
		const std::string prefixString(prefix);
		return text.rfind(prefixString, 0) == 0;
	}

	std::string ToLogicalModelPath(const std::string& inputPath)
	{
		if (StartsWith(inputPath, kModelSourceRoot))
		{
			return inputPath.substr(std::char_traits<char>::length(kModelSourceRoot));
		}
		if (StartsWith(inputPath, kModelRoot))
		{
			return inputPath.substr(std::char_traits<char>::length(kModelRoot));
		}
		return inputPath;
	}

	std::filesystem::path ToSourceModelPath(const std::string& logicalPath)
	{
		if (StartsWith(logicalPath, kModelSourceRoot) || StartsWith(logicalPath, kModelRoot))
		{
			return std::filesystem::path(logicalPath);
		}
		return std::filesystem::path(kModelSourceRoot) / logicalPath;
	}

	bool HasAnimationClipName(const Ken4lowEngine::AnimationModel& model, const std::string& name)
	{
		const auto& clips = model.GetAnimationClips();
		return std::any_of(clips.begin(), clips.end(), [&](const auto& clip) { return clip.name == name; });
	}
}

DebugScene::DebugScene() = default;
DebugScene::~DebugScene() = default;

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void DebugScene::Initialize()
{
	RegisterDebugActors(); // DebugScene専用のActorを登録する

	input_ = Input::GetInstance();

	// GPU描画へはまだ接続せず、DebugSceneで編集・JSON保存するための既定Effectを用意する。
	editingGpuParticleEffect_ = CreateDefaultGpuParticleEffectDesc();
	editingGpuParticleEffect_.effectName = "DebugEffect";
	selectedGpuParticleEmitterIndex_ = editingGpuParticleEffect_.emitters.empty() ? -1 : 0;
	gpuParticlePreviewController_ = std::make_unique<GpuParticlePreviewController>();
	gpuParticlePreviewController_->Initialize();

	// 衝突判定マネージャーの初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// 物理確認処理は専用コントローラへ委譲し、DebugSceneは呼び出し役に留める。
	physicsDebugController_ = std::make_unique<PhysicsDebugController>();
	physicsDebugController_->Initialize();

	// この検証はDebugScene内だけで所有し、実ゲームの敵・プレイヤー・ボスへ接続しない。
	animationModelBatchTest_ = std::make_unique<AnimationModelBatchTest>();
	animationModelBatchTest_->Initialize();

	CopyToBuffer(animationModelTestPath_, animationModelTestPathBuffer_.data(), animationModelTestPathBuffer_.size());
	for (size_t i = 0; i < animationModelTestLodPaths_.size(); ++i)
	{
		CopyToBuffer(animationModelTestLodPaths_[i], animationModelTestLodPathBuffers_[i].data(), animationModelTestLodPathBuffers_[i].size());
	}
	CopyToBuffer(animationModelIdleAnimationName_, animationModelIdleNameBuffer_.data(), animationModelIdleNameBuffer_.size());
	CopyToBuffer(animationModelWalkAnimationName_, animationModelWalkNameBuffer_.data(), animationModelWalkNameBuffer_.size());
	CopyToBuffer(animationModelRunAnimationName_, animationModelRunNameBuffer_.data(), animationModelRunNameBuffer_.size());
	CopyToBuffer(animationModelAttackAnimationName_, animationModelAttackNameBuffer_.data(), animationModelAttackNameBuffer_.size());
	CopyToBuffer(animationModelDamageAnimationName_, animationModelDamageNameBuffer_.data(), animationModelDamageNameBuffer_.size());
	CopyToBuffer(animationModelDeathAnimationName_, animationModelDeathNameBuffer_.data(), animationModelDeathNameBuffer_.size());
	animationStateController_.SetAnimationName(AnimationState::Idle, animationModelIdleAnimationName_);
	animationStateController_.SetAnimationName(AnimationState::Walk, animationModelWalkAnimationName_);
	animationStateController_.SetAnimationName(AnimationState::Run, animationModelRunAnimationName_);
	animationStateController_.SetAnimationName(AnimationState::Attack, animationModelAttackAnimationName_);
	animationStateController_.SetAnimationName(AnimationState::Damage, animationModelDamageAnimationName_);
	animationStateController_.SetAnimationName(AnimationState::Death, animationModelDeathAnimationName_);
	ReloadAnimationModelTest();

	// 静的な3万行列を一度だけ用意し、毎フレームのObject3D生成・更新コストを発生させない。
	instancingTestRenderer_ = std::make_unique<InstancedObject3DRenderer>();
	instancingTestRenderer_->Initialize("Sample/stanford-bunny.gltf", 30000);

	// 高ポリゴンモデルを大量描画してTDRを起こさないよう、Debug用の描画予算を設定する。
	instancingTestRenderer_->SetDebugIndexBudget(instancingIndexBudget_);

	RebuildInstancingTest();

	actorWorld_.SetPhysicsWorld(&actorPhysicsWorld_);
	actorPhysicsWorld_.SetUseFixedStep(false);
	actorWorld_.SpawnActor<TestActor>();
	actorWorld_.SpawnActor<TestGroundActor>();
	actorWorld_.Initialize();
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	const float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	// 本編へ接続せず、DebugScene専用の物理確認だけを更新する。
	if (physicsDebugController_)
	{
		physicsDebugController_->Update(deltaTime);
	}
	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->Update(deltaTime);
	}
	if (animationModelTestLoaded_ && animationModelTest_)
	{
		UpdateAnimationModelInputTest(deltaTime);
		animationModelTest_->Update();
	}
	if (gpuParticlePreviewController_)
	{
		// DebugScene上でプレビュー用Emitter設定と発生要求を更新する。
		// GPU Particle Runtime本体のUpdateは、このScene更新より前にFramework共通処理から毎フレーム呼ばれる。
		gpuParticlePreviewController_->Update(
			deltaTime,
			editingGpuParticleEffect_,
			selectedGpuParticleEmitterIndex_,
			gpuParticlePreviewPosition_,
			gpuParticlePreviewEmitCount_,
			gpuParticlePreviewAutoPlay_,
			gpuParticlePreviewSelectedOnly_);
	}

	collisionManager_->CheckAllCollisions();
	collisionManager_->Update();

	actorWorld_.Update(deltaTime);

	// ActorComponent由来のCollider同士を判定・イベント更新する
	actorPhysicsWorld_.Update(deltaTime);

	// PhysicsWorldの結果をActor/Component側のTransformへ反映する
	actorWorld_.PostPhysicsUpdate(deltaTime);
}

void DebugScene::UpdateEditor(float deltaTime)
{
	// DebugScene専用のAnimationModel大量描画負荷検証は、EditorのPlay停止中も更新して比較できるようにする。
	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->Update(deltaTime);
	}
	if (animationModelTestLoaded_ && animationModelTest_)
	{
		animationModelTest_->Update();
	}
	if (gpuParticlePreviewController_)
	{
		// EditorのPlay停止中もPreview操作を止めず、発生要求を次フレームの共通Runtime Updateへ渡す。
		gpuParticlePreviewController_->Update(
			deltaTime,
			editingGpuParticleEffect_,
			selectedGpuParticleEmitterIndex_,
			gpuParticlePreviewPosition_,
			gpuParticlePreviewEmitCount_,
			gpuParticlePreviewAutoPlay_,
			gpuParticlePreviewSelectedOnly_);
	}
}

/// -------------------------------------------------------------
///							3D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw3DObjects()
{
	// Preview粒子のDrawはGameApplication共通3Dパスが、この関数とWireframe描画の後に呼び出す。
	// Scene内から重複Drawせず、Preview StatusのRuntime Draw Calledで共通経路への到達を確認する。
	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->Draw();
	}
	if (animationModelTestLoaded_ && animationModelTest_)
	{
		animationModelTest_->Draw();
	}

	if (isInstancingTestEnabled_ && instancingTestRenderer_)
	{
		instancingTestRenderer_->Draw();
	}

	actorWorld_.Draw();

	// ActorComponent由来のColliderをWireframe表示する
	actorPhysicsDebugDraw_.Draw(actorPhysicsWorld_);

#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f, 1.0f });

	// DebugScene専用の物理テスト形状を描画する。
	if (physicsDebugController_)
	{
		physicsDebugController_->Draw();
	}

	collisionManager_->Draw();
#endif // _DEBUG
}

/// -------------------------------------------------------------
///					シャドウマップ描画処理
/// -------------------------------------------------------------
void DebugScene::DrawShadowObjects()
{
	actorWorld_.DrawShadow();
}

/// -------------------------------------------------------------
///							2D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// Actorに追加されたScreen Space Spriteを3D描画後にまとめて描画する
	actorWorld_.DrawScreenSpaceUI();

#pragma endregion
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void DebugScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->Finalize();
	}
	if (animationModelTest_)
	{
		animationModelTest_->Clear();
	}
	if (gpuParticlePreviewController_)
	{
		gpuParticlePreviewController_->Clear();
	}

	// Actorの外部登録を解除し、所有メンバ自体の破棄はDebugSceneのデストラクタへ任せる。
	actorWorld_.Finalize();
	input_ = nullptr;
}

/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	// ライトのImGui描画
	LightManager::GetInstance()->DrawImGui();

	// DebugScene専用のPhysicsWorld確認パネルを描画する。
	if (physicsDebugController_)
	{
		physicsDebugController_->DrawImGui();
	}
	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->DrawImGui();
	}
	DrawAnimationModelTestImGui();

	actorWorld_.DrawImGui();

	actorPhysicsDebugDraw_.GetSettings().drawPhysicsDebug = true;
	actorPhysicsDebugDraw_.GetSettings().drawColliders = true;
	actorPhysicsDebugDraw_.DrawImGui(actorPhysicsWorld_);

	ImGui::Checkbox("Show GPU Particle Editor", &showGpuParticleEditor_);
	if (showGpuParticleEditor_)
	{
		// Effect / Emitter設定をDebugSceneから編集するための独立したImGuiウィンドウ。
		if (ImGui::Begin("GPU Particle Editor", &showGpuParticleEditor_))
		{
			DrawGpuParticleEffectEditor(
				editingGpuParticleEffect_,
				selectedGpuParticleEmitterIndex_,
				gpuParticleEffectJsonPath_,
				gpuParticleEditorStatus_,
				gpuParticleEditorLastOperationSucceeded_);
			if (gpuParticlePreviewController_)
			{
				gpuParticlePreviewController_->DrawImGui(
					editingGpuParticleEffect_,
					selectedGpuParticleEmitterIndex_,
					gpuParticlePreviewPosition_,
					gpuParticlePreviewEmitCount_,
					gpuParticlePreviewAutoPlay_,
					gpuParticlePreviewSelectedOnly_);
			}
		}
		ImGui::End();
	}

	ImGui::SeparatorText("GPU Instancing Test");
	ImGui::Checkbox("Draw Instanced Objects", &isInstancingTestEnabled_);
	ImGui::Checkbox("Auto Clamp Heavy Model", &instancingAutoClamp_);

	ImGui::SliderInt("Instance Count", &instancingTestCount_, 1, 30000);
	ImGui::DragFloat("Spacing", &instancingTestSpacing_, 0.05f, 0.1f, 20.0f, "%.2f");
	ImGui::Checkbox("Random Scale", &instancingRandomScale_);
	ImGui::Checkbox("Random Rotation", &instancingRandomRotation_);
	ImGui::Checkbox("Random Color", &instancingRandomColor_);
	if (ImGui::Checkbox("Frustum Culling", &instancingFrustumCulling_) && instancingTestRenderer_)
	{
		instancingTestRenderer_->SetFrustumCullingEnabled(instancingFrustumCulling_);
	}
	if (ImGui::Button("Rebuild Instances"))
	{
		RebuildInstancingTest();
	}
	if (instancingTestRenderer_)
	{
		const uint64_t modelIndexCount = instancingTestRenderer_->GetModelTotalIndexCount();
		const uint64_t safeCount = modelIndexCount > 0
			? std::max<uint64_t>(1ull, instancingIndexBudget_ / modelIndexCount)
			: 30000ull;

		instancingSafeCount_ = static_cast<int>(std::min<uint64_t>(safeCount, 30000ull));

		ImGui::Text("Model Indices: %llu", static_cast<unsigned long long>(modelIndexCount));
		ImGui::Text("Safe Instance Count: %d", instancingSafeCount_);
		ImGui::Text("Estimated Draw Indices: %llu",
			static_cast<unsigned long long>(instancingTestRenderer_->GetEstimatedDrawIndexCount()));

		if (instancingTestRenderer_->WasDrawSkippedByBudget())
		{
			ImGui::TextColored(
				ImVec4(1.0f, 0.25f, 0.2f, 1.0f),
				"Draw skipped: estimated indices exceeded budget."
			);
		}

		ImGui::Text("Visible: %zu / Total: %zu / Capacity: %zu",
			instancingTestRenderer_->GetVisibleInstanceCount(),
			instancingTestRenderer_->GetInstanceCount(),
			instancingTestRenderer_->GetMaxInstanceCount());
	}

#endif // USE_IMGUI
}

void DebugScene::UpdateAnimationModelInputTest(float deltaTime)
{
	if (!animationModelInputTestEnabled_ || !animationModelTestLoaded_ || !animationModelTest_ || !input_) { return; }
	if (animationModelTest_->GetAnimationClips().empty()) { return; }

	Vector3 move{};
	if (input_->PushKey(DIK_A)) { move.x -= 1.0f; }
	if (input_->PushKey(DIK_D)) { move.x += 1.0f; }
	if (input_->PushKey(DIK_W)) { move.z += 1.0f; }
	if (input_->PushKey(DIK_S)) { move.z -= 1.0f; }

	const bool moving = Vector3::LengthSquared(move) > 0.0001f;
	const bool sprinting = moving && (input_->PushKey(DIK_LSHIFT) || input_->PushKey(DIK_RSHIFT));
	const AnimationState nextState = !moving
		? AnimationState::Idle
		: (sprinting ? AnimationState::Run : AnimationState::Walk);

	if (animationStateControllerEnabled_)
	{
		animationStateController_.SetCrossFadeDuration(animationModelCrossFadeDuration_);
		const std::string& nextAnimationName = animationStateController_.GetAnimationName(nextState);
		if (animationStateController_.RequestState(nextState, *animationModelTest_))
		{
			animationModelRequestedAnimationName_ = nextAnimationName;
		}
		else if (animationModelRequestedAnimationName_ != nextAnimationName)
		{
			animationModelRequestedAnimationName_ = nextAnimationName;
		}
	}
	else
	{
		const std::string& nextAnimationName = !moving
			? animationModelIdleAnimationName_
			: (sprinting ? animationModelRunAnimationName_ : animationModelWalkAnimationName_);

		if (animationModelRequestedAnimationName_ != nextAnimationName)
		{
			// 入力状態が変わった瞬間に前後のアニメーションを短時間ブレンドして、切り替わりの硬さを抑える。
			animationModelTest_->CrossFadeAnimationByName(nextAnimationName, animationModelCrossFadeDuration_);
			animationModelRequestedAnimationName_ = nextAnimationName;
		}
	}

	if (moving)
	{
		const Vector3 direction = Vector3::NormalizeSafe(move);
		const float speed = sprinting ? animationModelRunSpeed_ : animationModelWalkSpeed_;
		if (auto* wt = animationModelTest_->GetWorldTransformPtr())
		{
			wt->translate_ += direction * (speed * std::max(deltaTime, 0.0f));
		}
	}
}

void DebugScene::ReloadAnimationModelTest()
{
	animationModelTestPath_ = ToLogicalModelPath(std::string(animationModelTestPathBuffer_.data()));
	for (size_t i = 0; i < animationModelTestLodPaths_.size(); ++i)
	{
		animationModelTestLodPaths_[i] = ToLogicalModelPath(std::string(animationModelTestLodPathBuffers_[i].data()));
	}

	animationModelTestLoaded_ = false;
	animationModelTestLastOperationSucceeded_ = false;
	animationModelRequestedAnimationName_.clear();
	animationStateController_.Reset();
	if (animationModelTest_)
	{
		animationModelTest_->Clear();
		animationModelTest_.reset();
	}

	if (animationModelTestPath_.empty())
	{
		animationModelTestStatus_ = "Model path is empty.";
		return;
	}

	const std::filesystem::path sourcePath = ToSourceModelPath(animationModelTestPath_);
	if (!std::filesystem::exists(sourcePath))
	{
		animationModelTestStatus_ = "Model file not found: " + sourcePath.generic_string();
		return;
	}

	std::vector<std::string> lodPaths;
	if (animationModelTestUseLods_)
	{
		for (const std::string& lodPath : animationModelTestLodPaths_)
		{
			if (lodPath.empty()) { continue; }
			const std::filesystem::path lodSourcePath = ToSourceModelPath(lodPath);
			if (!std::filesystem::exists(lodSourcePath))
			{
				animationModelTestStatus_ = "LOD file not found: " + lodSourcePath.generic_string();
				return;
			}
			lodPaths.push_back(lodPath);
		}
	}

	try
	{
		auto model = std::make_unique<K4E::AnimationModel>();
		if (animationModelTestUseLods_ && !lodPaths.empty())
		{
			model->Initialize(animationModelTestPath_, lodPaths, true);
		}
		else
		{
			model->Initialize(animationModelTestPath_, true);
		}
		model->ReloadAnimationForDebugBatchTest();
		model->SetTranslate({ 0.0f, 0.0f, 5.0f });
		model->SetScale({ 1.0f, 1.0f, 1.0f });
		model->SetAnimationSpeed(animationModelTestSpeed_);
		model->SetAnimationLoop(animationModelTestLoop_);
		model->SetForceLOD(animationModelTestForceLod_, animationModelTestForcedLodIndex_);
		model->SetUseDebugSkinningViewProjection(true);

		animationModelTest_ = std::move(model);
		animationModelTestLoaded_ = true;
		animationModelTestLastOperationSucceeded_ = true;
		animationModelTestStatus_ = "Loaded: " + animationModelTestPath_;
	} catch (const std::exception& e)
	{
		animationModelTestStatus_ = std::string("Load failed: ") + e.what();
	} catch (...)
	{
		animationModelTestStatus_ = "Load failed: unknown exception.";
	}
}

void DebugScene::DrawAnimationModelTestImGui()
{
#ifdef USE_IMGUI
	if (ImGui::Begin("AnimationModel Test"))
	{
		// 複数アニメーション対応をゲーム本編へ入れる前に、DebugScene上で切り替え確認する。
		ImGui::TextWrapped("Status: %s", animationModelTestStatus_.c_str());
		if (!animationModelTestLastOperationSucceeded_)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "AnimationModel test model is not loaded.");
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("Model Path", animationModelTestPathBuffer_.data(), animationModelTestPathBuffer_.size());
		ImGui::Checkbox("Use LOD Models", &animationModelTestUseLods_);
		if (animationModelTestUseLods_)
		{
			for (int i = 0; i < static_cast<int>(animationModelTestLodPathBuffers_.size()); ++i)
			{
				ImGui::PushID(i);
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputText("LOD Path", animationModelTestLodPathBuffers_[i].data(), animationModelTestLodPathBuffers_[i].size());
				ImGui::PopID();
			}
		}
		if (ImGui::Button("Reload"))
		{
			ReloadAnimationModelTest();
		}

		ImGui::Separator();
		ImGui::Text("Loaded Model Path: %s", animationModelTestPath_.c_str());
		if (!animationModelTestLoaded_ || !animationModelTest_)
		{
			ImGui::TextUnformatted("No AnimationModel loaded.");
			ImGui::End();
			return;
		}

		const auto& clips = animationModelTest_->GetAnimationClips();
		const int currentIndex = animationModelTest_->GetCurrentAnimationIndex();
		ImGui::Text("Animation Count: %d", static_cast<int>(clips.size()));
		ImGui::Text("Current Index: %d", currentIndex);
		ImGui::Text("Current Name: %s", animationModelTest_->GetCurrentAnimationName().empty()
			? "(none)"
			: animationModelTest_->GetCurrentAnimationName().c_str());
		ImGui::Text("Duration: %.3f", animationModelTest_->GetAnimationDurationForDebugBatchTest());
		ImGui::Text("Playing: %s", animationModelTest_->IsAnimationPlaying() ? "true" : "false");
		ImGui::Text("Previous Name: %s", animationModelTest_->GetPreviousAnimationName().empty()
			? "(none)"
			: animationModelTest_->GetPreviousAnimationName().c_str());
		ImGui::Text("Is CrossFading: %s", animationModelTest_->IsCrossFading() ? "true" : "false");
		ImGui::Text("CrossFade Time: %.3f", animationModelTest_->GetCrossFadeTime());
		ImGui::Text("CrossFade Duration: %.3f", animationModelTest_->GetCrossFadeDuration());

		if (clips.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "No Animation Clips");
		}
		else
		{
			const char* preview = (0 <= currentIndex && currentIndex < static_cast<int>(clips.size()) && !clips[currentIndex].name.empty())
				? clips[currentIndex].name.c_str()
				: "(none)";
			if (ImGui::BeginCombo("Animation Combo", preview))
			{
				for (int i = 0; i < static_cast<int>(clips.size()); ++i)
				{
					const std::string fallbackName = "Animation_" + std::to_string(i);
					const std::string& clipName = clips[i].name.empty() ? fallbackName : clips[i].name;
					const bool selected = (i == currentIndex);
					if (ImGui::Selectable(clipName.c_str(), selected))
					{
						if (0 <= i && i < static_cast<int>(clips.size()))
						{
							if (animationModelUseCrossFadeForCombo_)
							{
								animationModelTest_->CrossFadeAnimationByIndex(static_cast<uint32_t>(i), animationModelCrossFadeDuration_);
							}
							else
							{
								animationModelTest_->PlayAnimationByIndex(static_cast<uint32_t>(i), true);
							}
						}
					}
					if (selected) { ImGui::SetItemDefaultFocus(); }
				}
				ImGui::EndCombo();
			}
		}

		ImGui::SeparatorText("CrossFade Input Test");
		ImGui::Checkbox("Enable Input Animation Test", &animationModelInputTestEnabled_);
		ImGui::Checkbox("Enable Animation State Controller", &animationStateControllerEnabled_);
		ImGui::Checkbox("Use CrossFade For Combo", &animationModelUseCrossFadeForCombo_);
		ImGui::DragFloat("CrossFade Duration", &animationModelCrossFadeDuration_, 0.01f, 0.0f, 2.0f, "%.2f");
		animationStateController_.SetCrossFadeDuration(animationModelCrossFadeDuration_);
		ImGui::DragFloat("Walk Move Speed", &animationModelWalkSpeed_, 0.05f, 0.0f, 20.0f, "%.2f");
		ImGui::DragFloat("Run Move Speed", &animationModelRunSpeed_, 0.05f, 0.0f, 40.0f, "%.2f");
		ImGui::Text("Requested Animation Name: %s", animationModelRequestedAnimationName_.empty()
			? "(none)"
			: animationModelRequestedAnimationName_.c_str());
		ImGui::Text("Controller Current State: %s", animationStateController_.HasCurrentState()
			? AnimationStateController::ToString(animationStateController_.GetCurrentState())
			: "(none)");
		ImGui::Text("Controller Requested State: %s", animationStateController_.HasCurrentState()
			? AnimationStateController::ToString(animationStateController_.GetRequestedState())
			: "(none)");
		if (ImGui::InputText("Idle Name", animationModelIdleNameBuffer_.data(), animationModelIdleNameBuffer_.size()))
		{
			animationModelIdleAnimationName_ = animationModelIdleNameBuffer_.data();
			animationStateController_.SetAnimationName(AnimationState::Idle, animationModelIdleAnimationName_);
			animationModelRequestedAnimationName_.clear();
			animationStateController_.Reset();
		}
		if (ImGui::InputText("Walk Name", animationModelWalkNameBuffer_.data(), animationModelWalkNameBuffer_.size()))
		{
			animationModelWalkAnimationName_ = animationModelWalkNameBuffer_.data();
			animationStateController_.SetAnimationName(AnimationState::Walk, animationModelWalkAnimationName_);
			animationModelRequestedAnimationName_.clear();
			animationStateController_.Reset();
		}
		if (ImGui::InputText("Run Name", animationModelRunNameBuffer_.data(), animationModelRunNameBuffer_.size()))
		{
			animationModelRunAnimationName_ = animationModelRunNameBuffer_.data();
			animationStateController_.SetAnimationName(AnimationState::Run, animationModelRunAnimationName_);
			animationModelRequestedAnimationName_.clear();
			animationStateController_.Reset();
		}
		if (ImGui::InputText("Attack Name", animationModelAttackNameBuffer_.data(), animationModelAttackNameBuffer_.size()))
		{
			animationModelAttackAnimationName_ = animationModelAttackNameBuffer_.data();
			animationStateController_.SetAnimationName(AnimationState::Attack, animationModelAttackAnimationName_);
			animationModelRequestedAnimationName_.clear();
			animationStateController_.Reset();
		}
		if (ImGui::InputText("Damage Name", animationModelDamageNameBuffer_.data(), animationModelDamageNameBuffer_.size()))
		{
			animationModelDamageAnimationName_ = animationModelDamageNameBuffer_.data();
			animationStateController_.SetAnimationName(AnimationState::Damage, animationModelDamageAnimationName_);
			animationModelRequestedAnimationName_.clear();
			animationStateController_.Reset();
		}
		if (ImGui::InputText("Death Name", animationModelDeathNameBuffer_.data(), animationModelDeathNameBuffer_.size()))
		{
			animationModelDeathAnimationName_ = animationModelDeathNameBuffer_.data();
			animationStateController_.SetAnimationName(AnimationState::Death, animationModelDeathAnimationName_);
			animationModelRequestedAnimationName_.clear();
			animationStateController_.Reset();
		}
		if (animationModelInputTestEnabled_)
		{
			if (!HasAnimationClipName(*animationModelTest_, animationModelIdleAnimationName_))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Warning: Idle animation name was not found.");
			}
			if (!HasAnimationClipName(*animationModelTest_, animationModelWalkAnimationName_))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Warning: Walk animation name was not found.");
			}
			if (!HasAnimationClipName(*animationModelTest_, animationModelRunAnimationName_))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Warning: Run animation name was not found.");
			}
			if (animationStateControllerEnabled_)
			{
				if (!animationModelAttackAnimationName_.empty() && !HasAnimationClipName(*animationModelTest_, animationModelAttackAnimationName_))
				{
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Warning: Attack animation name was not found.");
				}
				if (!animationModelDamageAnimationName_.empty() && !HasAnimationClipName(*animationModelTest_, animationModelDamageAnimationName_))
				{
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Warning: Damage animation name was not found.");
				}
				if (!animationModelDeathAnimationName_.empty() && !HasAnimationClipName(*animationModelTest_, animationModelDeathAnimationName_))
				{
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Warning: Death animation name was not found.");
				}
			}
		}

		if (ImGui::Button(animationModelTest_->IsAnimationPlaying() ? "Pause" : "Play"))
		{
			animationModelTest_->SetAnimationPlaying(!animationModelTest_->IsAnimationPlaying());
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Time"))
		{
			animationModelTest_->ResetAnimationTime();
		}

		animationModelTestSpeed_ = animationModelTest_->GetAnimationSpeed();
		if (ImGui::DragFloat("Animation Speed", &animationModelTestSpeed_, 0.01f, -4.0f, 4.0f, "%.2f"))
		{
			animationModelTest_->SetAnimationSpeed(animationModelTestSpeed_);
		}
		animationModelTestLoop_ = animationModelTest_->IsAnimationLoop();
		if (ImGui::Checkbox("Loop", &animationModelTestLoop_))
		{
			animationModelTest_->SetAnimationLoop(animationModelTestLoop_);
		}

		ImGui::SeparatorText("LOD");
		const int lodCount = static_cast<int>(animationModelTest_->GetLODs().size());
		const int maxLodIndex = lodCount > 0 ? lodCount - 1 : 0;
		ImGui::Text("Current LOD: %d / %d", animationModelTest_->GetLOD(), maxLodIndex);
		if (ImGui::Checkbox("Force LOD", &animationModelTestForceLod_))
		{
			animationModelTest_->SetForceLOD(animationModelTestForceLod_, animationModelTestForcedLodIndex_);
		}
		if (ImGui::SliderInt("Forced LOD Index", &animationModelTestForcedLodIndex_, 0, maxLodIndex))
		{
			animationModelTest_->SetForceLOD(animationModelTestForceLod_, animationModelTestForcedLodIndex_);
		}

		ImGui::SeparatorText("Transform");
		if (auto* wt = animationModelTest_->GetWorldTransformPtr())
		{
			ImGui::DragFloat3("Translate", reinterpret_cast<float*>(&wt->translate_), 0.01f);
			ImGui::DragFloat3("Rotate", reinterpret_cast<float*>(&wt->rotate_), 0.01f);
			ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&wt->scale_), 0.01f, 0.01f, 100.0f);
		}

		ImGui::Checkbox("Open Detailed AnimationModel DebugView", &animationModelTestShowDetailedDebugView_);
	}
	ImGui::End();

	if (animationModelTestShowDetailedDebugView_ && animationModelTestLoaded_ && animationModelTest_)
	{
		animationModelTest_->DrawImGui();
	}
#endif // USE_IMGUI
}

void DebugScene::RebuildInstancingTest()
{
	if (!instancingTestRenderer_) { return; }

	int count = std::clamp(instancingTestCount_, 1, 30000);

	if (instancingAutoClamp_ && instancingTestRenderer_)
	{
		const uint64_t modelIndexCount = instancingTestRenderer_->GetModelTotalIndexCount();
		if (modelIndexCount > 0)
		{
			const uint64_t safeCount = std::max<uint64_t>(1ull, instancingIndexBudget_ / modelIndexCount);
			count = std::min<int>(count, static_cast<int>(std::min<uint64_t>(safeCount, 30000ull)));
		}
	}
	const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
	std::mt19937 random(0x4B3445u); // 同じ設定なら同じ配置になる固定seed。
	std::uniform_real_distribution<float> scaleDistribution(0.55f, 1.45f);
	std::uniform_real_distribution<float> rotationDistribution(0.0f, std::numbers::pi_v<float> *2.0f);
	std::uniform_real_distribution<float> colorDistribution(0.25f, 1.0f);

	std::vector<InstancedObject3DRenderer::InstanceTransform> transforms;
	transforms.reserve(static_cast<size_t>(count));
	for (int i = 0; i < count; ++i)
	{
		const int x = i % columns;
		const int z = i / columns;
		InstancedObject3DRenderer::InstanceTransform transform{};
		transform.position = {
			(static_cast<float>(x) - columns * 0.5f) * instancingTestSpacing_,
			0.0f,
			(static_cast<float>(z) - columns * 0.5f) * instancingTestSpacing_
		};
		const float randomScale = instancingRandomScale_ ? scaleDistribution(random) : 1.0f;
		transform.scale = { 0.35f * randomScale, 0.35f * randomScale, 0.35f * randomScale };
		if (instancingRandomRotation_) { transform.rotation.y = rotationDistribution(random); }
		transform.color = instancingRandomColor_
			? Vector4{ colorDistribution(random), colorDistribution(random), colorDistribution(random), 1.0f }
		: Vector4{ 0.35f, 0.8f, 1.0f, 1.0f };
		transforms.push_back(transform);
	}

	instancingTestRenderer_->SetFrustumCullingEnabled(instancingFrustumCulling_);
	instancingTestRenderer_->SetTransforms(transforms);
}

/// -------------------------------------------------------------
///						Debug用の更新処理
/// -------------------------------------------------------------
void DebugScene::UpdateDebug()
{
	// Inputがない場合は何もしない（安全策）
	if (!input_) return;

	// F9はEditor操作中でも使いたいDebugショートカットなのでRaw入力で判定する
	if (input_->TriggerRawKey(DIK_F9))
	{
		// デバッグカメラの使用状態をトグルで切り替える
		const bool nextDebugCamera = !CameraManager::GetInstance()->IsUsingDebugCamera();

		// 切り替えた状態をCameraManagerとWireframeに伝える
		CameraManager::GetInstance()->SetUseDebugCamera(nextDebugCamera);
		Wireframe::GetInstance()->SetDebugCamera(nextDebugCamera);

		// DebugScene自身も状態を保持して、必要に応じて入力のロックやカーソルの表示を切り替える
		isDebugCamera_ = nextDebugCamera;

		// デバッグカメラ使用中はカーソルをロックして非表示にする。通常カメラ使用中はカーソルを表示してロック解除する。
		input_->SetLockCursor(!isDebugCamera_);
		input_->SetCursorVisible(isDebugCamera_);
	}
}

