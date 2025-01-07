#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input_ = Input::GetInstance();

	/// ---------- Object3Dの初期化 ---------- ///
	object3DCommon_ = std::make_unique<Object3DCommon>();

	/// ---------- カメラ初期化処理 ---------- ///
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,0.0f,-15.0f });
	object3DCommon_->SetDefaultCamera(camera_.get());

	// プレイヤーの初期化
	player_ = std::make_unique<Player>();
	player_->Initialize(object3DCommon_.get());
	player_->SetLanePositions({ -3.0f,0.0f,3.0f }); // レーンの位置を設定
	player_->SetCamera(camera_.get());

	floor_ = std::make_unique<Floor>();
	floor_->Initialize(object3DCommon_.get(), 30, 50.0f, 10.0f, 3.0f); // 床タイルを五枚生成

	obstacleManager_ = std::make_unique<ObstacleManager>();
	obstacleManager_->Initialize(object3DCommon_.get(), 30, 80.0f, 10.0f, 3.0f);

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/GamePlayBGM.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.2f, 1.0f, true);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// プレイヤーの更新処理
	player_->Update(input_, floor_.get(),obstacleManager_.get());

	obstacleManager_->Update(0.2f);

	// フロアの更新処理
	floor_->Update(0.2f, camera_.get()); // スクロール速度を設定

	// カメラの更新
	camera_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{
	// プレイヤーの描画処理
	player_->Draw();

	obstacleManager_->Draw(camera_.get());

	// 床を描画
	floor_->Draw(camera_.get());
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
	camera_->DrawImGui();
}
