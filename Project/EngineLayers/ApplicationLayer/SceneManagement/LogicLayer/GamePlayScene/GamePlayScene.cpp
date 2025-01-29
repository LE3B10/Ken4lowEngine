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

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.1f, 1.0f, false);

	/// ---------- カメラ初期化処理 ---------- ///
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.3f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,15.0f,-40.0f });
	object3DCommon_->SetDefaultCamera(camera_.get());

	// プレイヤーの生成と初期化
	player_ = std::make_unique<Player>();
	player_->Initialize(object3DCommon_.get());

	// 複数の床を生成
	for (int i = 0; i < 5; ++i)
	{
		auto floor = std::make_unique<Floor>();
		floor->Initialize(object3DCommon_.get());
		floor->SetTranslate({ 0.0f, -2.0f, static_cast<float>(i * 10) }); // 例えば奥に配置
		floorBlocks_.emplace_back(std::move(floor));
	}

	// 衝突マネージャの生成と初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();


}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// プレイヤー更新処理
	player_->Update();

	// フロアの更新処理
	for (auto& floor : floorBlocks_)
	{
		floor->Update();
	}

	// コリジョンマネージャーの更新処理
	collisionManager_->Update();
	// 衝突判定と応答
	CheckAllCollisions();

	// カメラの更新処理
	camera_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{
	// プレイヤーの描画処理
	player_->Draw();

	// フロアの描画処理
	for (auto& floor : floorBlocks_)
	{
		floor->Draw();
	}

	// コリジョンマネージャーの描画処理
	collisionManager_->Draw();
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	collisionManager_->Reset();
}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	ImGui::Begin("Test Window");

	ImGui::End();

	collisionManager_->DrawParameter();

	// カメラのImGui
	camera_->DrawImGui();
}


void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセ絵っと
	collisionManager_->Reset();

	/// ---------- コライダーをリストに登録 ---------- ///

	// プレイヤーを登録
	collisionManager_->AddCollider(player_.get());

	// 複数の床を登録
	for (auto& floor : floorBlocks_)
	{
		collisionManager_->AddCollider(floor.get());
	}

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}