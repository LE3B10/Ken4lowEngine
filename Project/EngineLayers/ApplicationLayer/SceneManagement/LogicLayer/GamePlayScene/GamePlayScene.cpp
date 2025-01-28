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

	/// ---------- カメラ初期化処理 ---------- ///
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.3f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,20.0f,-50.0f });
	camera_->SetFarClip(1000.0f);
	object3DCommon_->SetDefaultCamera(camera_.get());

	// プレイヤーの生成と初期化
	player_ = std::make_unique<Player>();
	player_->Initialize(object3DCommon_.get());

	// 地面の生成と初期化
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(object3DCommon_.get());

	// スカイドームの生成と初期化
	skydome_ = std::make_unique<Skydome>();
	skydome_->Initialize(object3DCommon_.get());

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.1f, 1.0f, false);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	player_->Update();
	ground_->Update();
	skydome_->Update();

	// カメラの更新処理
	camera_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{
	player_->Draw();
	ground_->Draw();
	skydome_->Draw();
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

	ImGui::End();

	// カメラのImGui
	camera_->DrawImGui();
}
