#define NOMINMAX
#include "TitleScene.h"
#include <DirectXCommon.h>
#include <SpriteManager.h>
#include <Object3DCommon.h>
#include <ImGuiManager.h>
#include "SceneManager.h"
#include "Input.h"
#include <Wireframe.h>

namespace K4E = ::Ken4lowEngine;


/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void TitleScene::Initialize()
{
	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	skyBox_ = std::make_unique<K4E::SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	// ロビー地形の初期化
	terrain_ = std::make_unique<K4E::Object3D>();
	terrain_->Initialize("lobby03.gltf");
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void TitleScene::Update()
{
	// デバッグ更新
	UpdateDebug();

	// 地形更新
	if (terrain_) terrain_->Update();

	// スカイボックス更新
	skyBox_->Update();

	if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効
}


/// -------------------------------------------------------------
///				　	3Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw3DObjects()
{
#pragma region オブジェクト3Dの描画

	skyBox_->Draw();

	if (terrain_) terrain_->Draw();

#pragma endregion

}


/// -------------------------------------------------------------
///				　	2Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw2DSprites()
{
#pragma region 背景の描画（後面）

	// 背景用の共通描画設定（後面）
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();


#pragma endregion


#pragma region UIの描画（前面）
	// UI用の共通描画設定
	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();

#pragma endregion

}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void TitleScene::Finalize()
{
	terrain_.reset();
	skyBox_.reset();

	camera_ = nullptr;
	input_ = nullptr;
	dxCommon_ = nullptr;
}


/// -------------------------------------------------------------
///				　		　ImGui描画処理
/// -------------------------------------------------------------
void TitleScene::DrawImGui()
{
#ifdef USE_IMGUI
	
#endif // USE_IMGUI

	K4E::LightManager::GetInstance()->DrawImGui();
}

/// -------------------------------------------------------------
///				　	デバッグ用更新（キー入力など）
/// -------------------------------------------------------------
void TitleScene::UpdateDebug()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_BACK))
	{
		if (sceneManager_)
		{
			sceneManager_->ChangeScene("PhysicalScene"); // 戻るキーでゲームプレイシーンに戻る
		}
	}

	if (input_->TriggerKey(DIK_F12))
	{
		K4E::Object3DCommon::GetInstance()->SetDebugCamera(!K4E::Object3DCommon::GetInstance()->GetDebugCamera());
		K4E::Wireframe::GetInstance()->SetDebugCamera(!K4E::Wireframe::GetInstance()->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
	}
#endif // _DEBUG
}