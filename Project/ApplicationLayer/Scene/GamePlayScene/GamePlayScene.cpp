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
	AudioManager::GetInstance()->PlayBGM("Peritune_Gentle_Brew.mp3", 0.1f, 1.0f, true);

	// terrainの生成と初期化
	objectTerrain_ = std::make_unique<Object3D>();
	objectTerrain_->Initialize("terrain.obj");
	objectTerrain_->SetTranslate({ 0.0f, -1.0f, 0.0f });
	objectTerrain_->SetReflectivity(0.0f);

	objectBall_ = std::make_unique<Object3D>();
	objectBall_->Initialize("sphere.gltf");
	objectBall_->SetTranslate({ -2.0f, 0.0f, 0.0f });

	animationModelNoskeleton_ = std::make_unique<AnimationModel>();
	animationModelNoskeleton_->Initialize("AnimatedCube.gltf", true, false);
	animationModelNoskeleton_->SetTranslate({ 10.0f, 0.0f, 0.0f });
	animationModelNoskeleton_->SetReflectivity(0.0f);

	animationModelSkeleton_ = std::make_unique<AnimationModel>();
	animationModelSkeleton_->Initialize("walk.gltf", true, true);

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("rostock_laage_airport_4k.dds");

	// パーティクルエミッターの初期化
	ParticleManager::GetInstance()->CreateParticleGroup("TestParticle2", "gradationLine.png", ParticleEffectType::Cylinder);
	cylinderEmitter_ = std::make_unique<ParticleEmitter>(ParticleManager::GetInstance(), "TestParticle2");
	cylinderEmitter_->SetPosition({ 0.0f, -3.0f, 0.0f });

	ParticleManager::GetInstance()->CreateParticleGroup("MuzzleFlash", "gradationLine.png", ParticleEffectType::Star);
	muzzleFlashEffect_ = std::make_unique<ParticleEmitter>(ParticleManager::GetInstance(), "MuzzleFlash");
	muzzleFlashEffect_->SetEmissionRate(1.0f);

	ParticleManager::GetInstance()->CreateParticleGroup("muzzleSmoke", "smoke.png", ParticleEffectType::Smoke);
	smokeEffect_ = std::make_unique<ParticleEmitter>(ParticleManager::GetInstance(), "muzzleSmoke");
	smokeEffect_->SetEmissionRate(1.0f);

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

	//animationModelNoskeleton_->Update();

	//animationModelSkeleton_->Update();

	skyBox_->Update();

	// 衝突判定と応答
	CheckAllCollisions();

	cylinderEmitter_->Update();

	muzzleFlashEffect_->Update();
	muzzleFlashEffect_->Burst(1);

	smokeEffect_->Update();
	smokeEffect_->Burst(1);
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

	// Terrain.obj の描画
	//objectTerrain_->Draw();

	// 球体の描画
	//objectBall_->Draw();

	//animationModelNoskeleton_->Draw();

	//animationModelSkeleton_->Draw();

#pragma endregion

	// ワイヤーフレームの描画
	//Wireframe::GetInstance()->DrawGrid(100.0f, 20.0f, { 0.25f, 0.25f, 0.25f,1.0f });
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
	ImGui::Begin("Test Window");

	// TerrainのImGui
	//objectTerrain_->DrawImGui();

	objectBall_->DrawImGui();

	ImGui::End();

	animationModelSkeleton_->DrawImGui();

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
