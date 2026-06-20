#define NOMINMAX
#include "DebugScene.h"
#include "PhysicsDebugController.h"
#include "AnimationModelBatchTest.h"
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <LightManager.h>
#include <GameTimer.h>
#include <InstancedObject3DRenderer.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Ken4lowEngine;

DebugScene::DebugScene() = default;
DebugScene::~DebugScene() = default;

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void DebugScene::Initialize()
{
	input_ = Input::GetInstance();

	// 衝突判定マネージャーの初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// 物理確認処理は専用コントローラへ委譲し、DebugSceneは呼び出し役に留める。
	physicsDebugController_ = std::make_unique<PhysicsDebugController>();
	physicsDebugController_->Initialize();

	// この検証はDebugScene内だけで所有し、実ゲームの敵・プレイヤー・ボスへ接続しない。
	animationModelBatchTest_ = std::make_unique<AnimationModelBatchTest>();
	animationModelBatchTest_->Initialize();

	// 静的な3万行列を一度だけ用意し、毎フレームのObject3D生成・更新コストを発生させない。
	instancingTestRenderer_ = std::make_unique<InstancedObject3DRenderer>();
	instancingTestRenderer_->Initialize("Sample/cube.gltf", 30000);
	RebuildInstancingTest();
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

	collisionManager_->CheckAllCollisions();
	collisionManager_->Update();
}

void DebugScene::UpdateEditor(float deltaTime)
{
	// DebugScene専用のAnimationModel大量描画負荷検証は、EditorのPlay停止中も更新して比較できるようにする。
	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->Update(deltaTime);
	}
}

/// -------------------------------------------------------------
///							3D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw3DObjects()
{
	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->Draw();
	}

	if (isInstancingTestEnabled_ && instancingTestRenderer_)
	{
		instancingTestRenderer_->Draw();
	}


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

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();


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

	collisionManager_.reset();
	physicsDebugController_.reset();
	if (animationModelBatchTest_)
	{
		animationModelBatchTest_->Finalize();
		animationModelBatchTest_.reset();
	}
	instancingTestRenderer_.reset();

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

	ImGui::SeparatorText("GPU Instancing Test");
	ImGui::Checkbox("Draw Instanced Objects", &isInstancingTestEnabled_);
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
		ImGui::Text("Visible: %zu / Total: %zu / Capacity: %zu",
			instancingTestRenderer_->GetVisibleInstanceCount(),
			instancingTestRenderer_->GetInstanceCount(),
			instancingTestRenderer_->GetMaxInstanceCount());
	}

#endif // USE_IMGUI
}

void DebugScene::RebuildInstancingTest()
{
	if (!instancingTestRenderer_) { return; }

	const int count = std::clamp(instancingTestCount_, 1, 30000);
	const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
	std::mt19937 random(0x4B3445u); // 同じ設定なら同じ配置になる固定seed。
	std::uniform_real_distribution<float> scaleDistribution(0.55f, 1.45f);
	std::uniform_real_distribution<float> rotationDistribution(0.0f, std::numbers::pi_v<float> * 2.0f);
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

