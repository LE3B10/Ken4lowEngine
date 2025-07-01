#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include <AnimationPipelineBuilder.h>
#include "SkyBoxManager.h"
#include <ParameterManager.h>
#include <ParticleManager.h>
#include "Wireframe.h"
#include "AudioManager.h"
#include <SceneManager.h>
#include <ScoreManager.h>

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

	// カーソルをロック
	Input::GetInstance()->SetLockCursor(true);
	ShowCursor(false);// 表示・非表示も連動（オプション）

	// スコアの初期化
	ScoreManager::GetInstance()->Initialize();

	// プレイヤーの生成と初期化
	player_ = std::make_unique<Player>();
	player_->Initialize();

	dummyModel_ = std::make_unique<DummyModel>();
	dummyModel_->Initialize();

	boss_ = std::make_unique<Boss>();
	boss_->Initialize();
	boss_->SetPlayer(player_.get());

	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(player_.get());

	// 追従カメラの生成と初期化
	fpsCamera_ = std::make_unique<FpsCamera>();
	fpsCamera_->Initialize(player_.get());
	fpsCamera_->SetDeltaTime(player_->GetDeltaTime());

	// プレイヤーにカメラを設定
	player_->SetCamera(fpsCamera_->GetCamera());
	player_->SetFpsCamera(fpsCamera_.get());

	// クロスヘアの生成と初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	player_->SetCrosshair(crosshair_.get()); // プレイヤーにクロスヘアを設定

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// HUDマネージャーの生成と初期化
	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->Initialize();

	// 結果マネージャーの生成と初期化
	resultManager_ = std::make_unique<ResultManager>();
	resultManager_->Initialize();

	// 初期化内に追加（プレイヤー近くに1個スポーン）
	itemManager_ = std::make_unique<ItemManager>();
	itemManager_->Initialize();

	enemyManager_->SetItemManager(itemManager_.get());

	terrein_ = std::make_unique<Object3D>();
	// 地形オブジェクトの初期化
	terrein_->Initialize("Terrain.gltf");
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		if (isPaused_) return; // ポーズ中はデバッグカメラ切り替えを無効化

		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		ParticleManager::GetInstance()->SetDebugCamera(!ParticleManager::GetInstance()->GetDebugCamera());
		//skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		player_->SetDebugCamera(!player_->IsDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		Input::GetInstance()->SetLockCursor(isDebugCamera_);
		ShowCursor(!isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG

	// --- ポーズトグル（ESCキーでON/OFF） ---
	if (input_->TriggerKey(DIK_ESCAPE))
	{
		if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

		if (gameState_ == GameState::Playing)
		{
			isPaused_ = true; // ポーズ状態にする
			gameState_ = GameState::Paused;
			Input::GetInstance()->SetLockCursor(false);
			ShowCursor(true);
		}
		else if (gameState_ == GameState::Paused)
		{
			isPaused_ = false; // ポーズ解除
			gameState_ = GameState::Playing;
			Input::GetInstance()->SetLockCursor(true);
			ShowCursor(false);
		}
	}

	switch (gameState_)
	{
	case GameState::Playing:

		if (player_->IsDead())
		{
			gameState_ = GameState::Result;
			Input::GetInstance()->SetLockCursor(false);
			ShowCursor(true);

			// ★ 結果情報を設定
			resultManager_->SetFinalScore(ScoreManager::GetInstance()->GetScore());
			resultManager_->SetKillCount(ScoreManager::GetInstance()->GetKills());
			resultManager_->SetWaveCount(enemyManager_->GetCurrentWave()); // EnemyManager に GetCurrentWave() が必要

			break;
		}

		// エネミーマネージャーの更新
		enemyManager_->Update();

		// Waveがすべて終わったら次Waveをスタート
		if (enemyManager_->IsWaveClear()) {
			enemyManager_->StartNextWave();
		}

		{
			Weapon* weapon = player_->GetCurrentWeapon();
			if (weapon) {
				if (weapon->IsReloading())
				{
					crosshair_->SetVisible(false);
					hudManager_->SetReloading(true, weapon->GetReloadProgress());
				}
				else
				{
					crosshair_->SetVisible(true);
					hudManager_->SetReloading(false, 0.0f);
				}
			}
		}

		// アイテムの更新と衝突判定
		itemManager_->Update(player_.get());

		hudManager_->Update();
		hudManager_->SetAmmoFromWeapon(player_->GetCurrentWeapon());// 新しいコード（武器の情報を直接渡す）
		hudManager_->SetScore(ScoreManager::GetInstance()->GetScore());
		hudManager_->SetKills(ScoreManager::GetInstance()->GetKills());
		hudManager_->SetHP(player_->GetHP(), player_->GetMaxHP());
		hudManager_->SetWeapon(player_->GetCurrentWeapon());
		hudManager_->SetStamina(player_->GetStamina(), player_->GetMaxStamina());

		player_->Update();
		boss_->Update();

		dummyModel_->Update();

		fpsCamera_->Update();

		crosshair_->Update();
		CheckAllCollisions();
		collisionManager_->Update();
		break;

	case GameState::Paused:

#ifdef _DEBUG
		// 衝突判定
		collisionManager_->Update();
#endif // _DEBUG

		fpsCamera_->Update(true); // 回転行列だけ更新するなら true を渡す
		// 何も動かさない（またはポーズUIだけ更新）
		break;

	case GameState::Result:

		resultManager_->Update();
		if (resultManager_->IsRestartRequested())
		{
			sceneManager_->ChangeScene("GamePlayScene");
		}
		if (resultManager_->IsReturnToTitleRequested())
		{
			sceneManager_->ChangeScene("TitleScene");
		}

		break;
	}

	terrein_->Update();
}


/// -------------------------------------------------------------
///				　		3Dオブジェクトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
	switch (gameState_) {
	case GameState::Playing:
		DrawPlaying();
		break;
	case GameState::Paused:
		DrawPaused();
		break;
	case GameState::Result:
		DrawResult();
		break;
	}

#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();

#pragma endregion


#pragma region オブジェクト3Dの描画

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	terrein_->Draw();

	// アイテムの描画
	itemManager_->Draw();

	enemyManager_->Draw();

	// プレイヤーの描画
	player_->Draw();

#pragma endregion


#pragma region アニメーションモデルの描画

	// アニメーションモデルの共通描画設定
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

	boss_->Draw();

	dummyModel_->Draw();

#pragma endregion


#ifdef _DEBUG
	// 衝突判定を行うオブジェクトの描画
	collisionManager_->Draw();

	// FPSカメラの描画
	fpsCamera_->DrawDebugCamera();

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 100.0f, { 0.25f, 0.25f, 0.25f,1.0f });

#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　		2Dスプライトの描画
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

	// HUDマネージャーの描画
	hudManager_->Draw();

	if (gameState_ == GameState::Result) resultManager_->Draw();

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

	boss_->DrawImGui();

	dummyModel_->DrawImGui();

	// エネミースポナー
	//enemyManager_->DrawImGui();
}


/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// コライダーをリストに登録
	enemyManager_->RegisterColliders(collisionManager_.get());
	player_->RegisterColliders(collisionManager_.get()); // プレイヤーのコライダーを登録
	dummyModel_->RegisterColliders(collisionManager_.get());
	boss_->RegisterColliders(collisionManager_.get());

	// プレイヤーの弾の登録
	for (const auto& bullet : player_->GetBullets())
	{
		if (!bullet->IsDead()) collisionManager_->AddCollider(bullet.get());
	}


	// プレイヤーが死亡している場合、コライダーを削除
	if (player_->IsDead()) collisionManager_->RemoveCollider(player_.get());

	// アイテムのコライダーを登録
	itemManager_->RegisterColliders(collisionManager_.get());

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}

void GamePlayScene::UpdatePlaying()
{
}

void GamePlayScene::UpdatePaused()
{
}

void GamePlayScene::UpdateResult()
{
}

void GamePlayScene::DrawPlaying()
{
}

void GamePlayScene::DrawPaused()
{
}

void GamePlayScene::DrawResult()
{
}
