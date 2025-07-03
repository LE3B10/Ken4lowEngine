#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include <ParameterManager.h>
#include <ParticleManager.h>
#include "Wireframe.h"
#include "AudioManager.h"
#include "AnimationPipelineBuilder.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#include <json.hpp>
#include <iostream>


/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	auto levelData = LevelLoader::Load("Resources/enemy1.json");
	levelManager_ = std::make_unique<LevelObjectManager>();
	levelManager_->Initialize(*levelData);

	// terrainの生成と初期化
	objectTerrain_ = std::make_unique<Object3D>();
	objectTerrain_->Initialize("terrain.obj");
	objectTerrain_->SetReflectivity(0.0f);

	objectBall_ = std::make_unique<Object3D>();
	objectBall_->Initialize("sphere.gltf");
	objectBall_->SetTranslate({ -2.0f, 0.0f, 0.0f });

	animationModel_ = std::make_unique<AnimationModel>();
	animationModel_->Initialize("PlayerStateModel/human.gltf");

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("rostock_laage_airport_4k.dds");

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		ParticleManager::GetInstance()->SetDebugCamera(!ParticleManager::GetInstance()->GetDebugCamera());
		skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		Input::GetInstance()->SetLockCursor(isDebugCamera_);
		ShowCursor(!isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG

	// オブジェクトの更新処理
	objectTerrain_->Update();
	objectBall_->Update();

	skyBox_->Update();

	// レベルマネージャの更新
	levelManager_->Update();

	animationModel_->Update();

	// 衝突判定と応答
	CheckAllCollisions();
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();
	//skyBox_->Draw();

#pragma endregion


#pragma region オブジェクト3Dの描画

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	objectTerrain_->Draw();

	// 球体の描画
	//objectBall_->Draw();

	// レベルマネージャの描画
	//levelManager_->Draw();

#pragma endregion


#pragma region アニメーションモデルの描画

	// アニメーションモデルの共通描画設定
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

	animationModel_->Draw();

#pragma endregion

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 100.0f, { 0.25f, 0.25f, 0.25f,1.0f });
}


void GamePlayScene::Draw2DSprites()
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
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	AudioManager::GetInstance()->StopBGM();
}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	// ライト
	LightManager::GetInstance()->DrawImGui();
}


/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// コライダーをリストに登録
	// collisionManager_->AddCollider();

	// 複数について

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}
