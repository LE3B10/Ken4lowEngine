#define NOMINMAX
#include "DebugScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <LightManager.h>
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void DebugScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	// 衝突判定マネージャーの初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	//float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	collisionManager_->CheckAllCollisions();
	collisionManager_->Update();
}

/// -------------------------------------------------------------
///							3D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw3DObjects()
{


#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f, 1.0f });

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

	input_ = nullptr;
	dxCommon_ = nullptr;
}

/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	// ライトのImGui描画
	LightManager::GetInstance()->DrawImGui();

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

