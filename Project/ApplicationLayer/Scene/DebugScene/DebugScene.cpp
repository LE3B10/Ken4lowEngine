#define NOMINMAX
#include "DebugScene.h"
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <GameTimer.h>

#include "DebugActorRegistration.h"
#include "TestActor.h"
#include "TestGroundActor.h"

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
	RegisterDebugActors(); // DebugScene専用のActorを登録する

	input_ = Input::GetInstance();

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

	actorWorld_.Update(deltaTime);

	// ActorComponent由来のCollider同士を判定・イベント更新する
	actorPhysicsWorld_.Update(deltaTime);

	// PhysicsWorldの結果をActor/Component側のTransformへ反映する
	actorWorld_.PostPhysicsUpdate(deltaTime);
}

/// -------------------------------------------------------------
///							3D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw3DObjects()
{
	actorWorld_.Draw();

	// ActorComponent由来のColliderをWireframe表示する
	actorPhysicsDebugDraw_.Draw(actorPhysicsWorld_);

#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f, 1.0f });

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

	actorWorld_.DrawImGui();

	actorPhysicsDebugDraw_.GetSettings().drawPhysicsDebug = true;
	actorPhysicsDebugDraw_.GetSettings().drawColliders = true;
	actorPhysicsDebugDraw_.DrawImGui(actorPhysicsWorld_);

#endif // USE_IMGUI
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

