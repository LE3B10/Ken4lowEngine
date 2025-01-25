#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();
	textureManager = TextureManager::GetInstance();
	particleManager = ParticleManager::GetInstance();

	/// ---------- Object3Dの初期化 ---------- ///
	object3DCommon_ = std::make_unique<Object3DCommon>();

	// .objのパスをリストで管理
	objectFiles = {
		"terrain.gltf",
		"sphere.gltf",
	};

	std::vector<Vector3> initialPositions = {
		{ 0.0f, 0.0f, 0.0f},     // terrain.obj の座標
		{ 0.0f, 0.0f, 0.0f},     // sphere.gltf の座標
	};

	/// ---------- カメラ初期化処理 ---------- ///
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.3f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,10.0f,-30.0f });
	object3DCommon_->SetDefaultCamera(camera_.get());

	// terrainの生成と初期化
	objectTerrain_ = std::make_unique<Object3D>();
	objectTerrain_->Initialize(object3DCommon_.get(), objectFiles[0]);
	objectTerrain_->SetTranslate({ 0.0f,0.0f,0.0f });

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.1f, 1.0f, false);

	textureManager->LoadTexture("Resources/uvChecker.png");
	particleManager->Initialize(dxCommon_, camera_.get());
	particleManager->CreateParticleGroup("fire", "Resources/uvChecker.png");

	fireEmitter = std::make_unique<ParticleEmitter>(particleManager, "fire");
	fireEmitter->SetPosition({ 0.0f,0.0f,20.0f }); // 射出する位置を設定
	fireEmitter->SetEmissionRate(1200.0f);		  // 1秒間に20このパーティクルを射出
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	objectTerrain_->Update();

	// カメラの更新処理
	camera_->Update();

	// パーティクルエミッターの更新
	fireEmitter->Update(1.0f / 60.0f); // フレーム時間を渡す
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{
	// Terrain.obj の描画
	objectTerrain_->Draw();
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

	// パーティクルエミッターのImGui
	if (ImGui::CollapsingHeader("ParticleEmitter Settings")) {
		Vector3 position = fireEmitter->GetPosition();
		if (ImGui::DragFloat3("Emitter Position", &position.x, 0.1f)) {
			fireEmitter->SetPosition(position); // 射出位置を更新
		}

		float rate = fireEmitter->GetEmissionRate();
		if (ImGui::DragFloat("Emission Rate", &rate, 1.0f, 1.0f, 100.0f)) {
			fireEmitter->SetEmissionRate(rate); // 射出レートを更新
		}
	}

	ImGui::End();

	// カメラのImGui
	camera_->DrawImGui();
}
