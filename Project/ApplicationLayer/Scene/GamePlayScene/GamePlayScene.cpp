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

namespace
{
	OBB testObb =
	{
		.center = {0.0f, 0.0f, 0.0f},
		.orientations = {
			{1.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 1.0f}
		},
		.size = {1.0f, 1.0f, 1.0f}
	};
	Vector4 testColor = { 1.0f, 1.0f, 0.0f, 1.0f };
}


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

	// カーソルをロック
	Input::GetInstance()->SetLockCursor(true);
	ShowCursor(true);// 表示・非表示も連動（オプション）

	// プレイヤーの生成と初期化
	player_ = std::make_unique<Player>();
	player_->Initialize();

	// エネミーの生成と初期化
	enemy_ = std::make_unique<Enemy>();
	enemy_->Initialize();
	enemy_->SetTarget(player_.get());

	// 追従カメラの生成と初期化
	fpsCamera_ = std::make_unique<FpsCamera>();
	fpsCamera_->Initialize(player_.get());

	// プレイヤーにカメラを設定
	player_->SetCamera(fpsCamera_->GetCamera());

	// クロスヘアの生成と初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// 
	ParticleManager::GetInstance()->CreateParticleGroup("TestParticle", "gradationLine.png");
	particleEmitter_ = std::make_unique<ParticleEmitter>(ParticleManager::GetInstance(), "TestParticle");
	particleEmitter_->SetPosition({ 0.0f, 15.0f, 20.0f });
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
		//skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		Input::GetInstance()->SetLockCursor(isDebugCamera_);
		ShowCursor(!isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG

	// --- ポーズトグル（ESCキーでON/OFF） ---
	if (input_->TriggerKey(DIK_ESCAPE))
	{
		if (gameState_ == GameState::Playing)
		{
			gameState_ = GameState::Paused;
			Input::GetInstance()->SetLockCursor(false);
			ShowCursor(true);
		}
		else if (gameState_ == GameState::Paused)
		{
			gameState_ = GameState::Playing;
			Input::GetInstance()->SetLockCursor(true);
			ShowCursor(false);
		}
	}

	switch (gameState_)
	{
	case GameState::Playing:
		player_->Update();
		fpsCamera_->Update(false);
		crosshair_->Update();
		enemy_->Update();
		collisionManager_->Update();
		CheckAllCollisions();
		break;

	case GameState::Paused:

#ifdef _DEBUG
		// 衝突判定
		collisionManager_->Update();
#endif // _DEBUG

		fpsCamera_->Update(true); // 回転行列だけ更新するなら true を渡す
		// 何も動かさない（またはポーズUIだけ更新）
		break;
	}

	particleEmitter_->Update();
}


/// -------------------------------------------------------------
///				　			　 3Dオブジェクトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();

#pragma endregion


#pragma region オブジェクト3Dの描画

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	// プレイヤーの描画
	player_->Draw();

	// エネミーの描画
	enemy_->Draw();

#pragma endregion


#ifdef _DEBUG
	// 衝突判定を行うオブジェクトの描画
	collisionManager_->Draw();

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(1000.0f, 100.0f, { 0.25f, 0.25f, 0.25f,1.0f });
#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　			　 2Dスプライトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	// クロスヘアの描画
	crosshair_->Draw();

#pragma endregion
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
	// ライト
	LightManager::GetInstance()->DrawImGui();

	// プレイヤー
	player_->DrawImGui();

	// エネミー
	enemy_->DrawImGui();

	if (ImGui::Begin("Test OBB"))
	{
		ImGui::DragFloat3("Center", &testObb.center.x, 0.1f);
		ImGui::DragFloat3("HalfSize", &testObb.size.x, 0.1f);
		ImGui::ColorEdit4("Color", &testColor.x);
		ImGui::End();
	}
}


/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// コライダーをリストに登録
	collisionManager_->AddCollider(player_.get());

	// 敵のコライダーを登録
	collisionManager_->AddCollider(enemy_.get());

	// プレイヤーの弾の登録
	for (const auto& bullet : player_->GetBullets())
	{
		if (!bullet->IsDead())
		{
			collisionManager_->AddCollider(bullet.get());
		}
	}

	// 死亡したらコライダーを削除
	if (enemy_->IsDead()) collisionManager_->RemoveCollider(enemy_.get());

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}
