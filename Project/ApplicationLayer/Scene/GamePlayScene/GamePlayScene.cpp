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

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG


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
	particleManager = ParticleManager::GetInstance();

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.0f, 1.0f, false);

	// terrainの生成と初期化
	objectTerrain_ = std::make_unique<Object3D>();
	objectTerrain_->Initialize("terrain.obj");

	objectBall_ = std::make_unique<Object3D>();
	objectBall_->Initialize("sphere.gltf");

	particleManager->CreateParticleGroup("Fire", "uvChecker.png");
	particleEmitter_ = std::make_unique<ParticleEmitter>(particleManager, "Fire");
	particleEmitter_->SetPosition({ 0.0f,3.0f,10.0f });
	particleEmitter_->SetEmissionRate(1000.0f);

	animationModel_ = std::make_unique<AnimationModel>();
	animationModel_->Initialize("walk.gltf", true, true);

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
	}
#endif // _DEBUG

	// オブジェクトの更新処理
	objectTerrain_->Update();
	objectBall_->Update();

	particleEmitter_->Update(1.0f / 120.0f);

	animationModel_->Update();

	skyBox_->Update();

	// 衝突判定と応答
	CheckAllCollisions();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();
	//skyBox_->Draw();

#pragma endregion


#pragma region スプライトの描画

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region オブジェクト3Dの描画

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	// Terrain.obj の描画
	objectTerrain_->Draw();
	//objectBall_->Draw();

	animationModel_->Draw();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

#pragma endregion

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 20.0f, { 0.25f, 0.25f, 0.25f,1.0f });
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{

}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	ImGui::Begin("Test Window");

	// TerrainのImGui
	objectTerrain_->DrawImGui();

	ImGui::End();
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
