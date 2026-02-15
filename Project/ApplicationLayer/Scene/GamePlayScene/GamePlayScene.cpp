#define NOMINMAX
#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	K4E::DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	input_->SetLockCursor(true);
	input_->SetCursorVisible(false);

	skyBox_ = std::make_unique<K4E::SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	// 衝突マネージャーの初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// 弾丸マネージャーの初期化
	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(collisionManager_.get());

	// キャラクター関連の初期化
	GameContext ctx{};
	ctx.collisionManager_ = collisionManager_.get();
	ctx.bulletManager_ = bulletManager_.get();
	characters_.Initialize(ctx);

	characters_.SpawnEnemy(EnemyArchetype::RifleGrunt, { 0.0f, 0.0f, 30.0f });
	characters_.SpawnEnemy(EnemyArchetype::SMGFlanker, { 6.0f, 0.0f, 28.0f });
	characters_.SpawnEnemy(EnemyArchetype::Sniper, { -6.0f, 0.0f, 38.0f });
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// デルタタイムの取得
	const float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

	// デバッグカメラの更新
	UpdateDebug();

	// キャラクター関連の更新
	characters_.Update(deltaTime);

	// 弾丸マネージャーの更新
	bulletManager_->Update(deltaTime);

	// 衝突判定の更新
	CollisionUpdate();

	skyBox_->Update();
}

/// -------------------------------------------------------------
///				　		3Dオブジェクトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();

	//skyBox_->Draw();

#pragma endregion


#pragma region オブジェクト3Dの描画

	// キャラクターの描画
	characters_.Draw();

	// 弾丸の描画
	bulletManager_->Draw();

#pragma endregion


#pragma region アニメーションモデルの描画

#pragma endregion


#ifdef _DEBUG
	// 衝突判定を行うオブジェクトの描画
	collisionManager_->Draw();

	// FPSカメラの描画
	//fpsCamera_->DrawDebugCamera();

	// ワイヤーフレームの描画
	K4E::Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });

#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　		2Dスプライトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();


#pragma endregion
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	// ★重要：CharacterWorld は CollisionManager を使って RemoveCollider する
	//         ので、先に characters_ を Finalize してから manager 類を破棄する
	characters_.Finalize();

	// 弾丸マネージャーの終了処理（Collision を参照している可能性があるため先）
	bulletManager_.reset();

	// 衝突マネージャーの終了処理
	if (collisionManager_) {
		collisionManager_->Reset();
	}
	collisionManager_.reset();

	// 3D背景など
	skyBox_.reset();

	// 生ポインタ参照は最後に切る
	input_ = nullptr;
	dxCommon_ = nullptr;
}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	// ライト
	K4E::LightManager::GetInstance()->DrawImGui();

#ifdef USE_IMGUI



#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　			Debug用更新処理
/// -------------------------------------------------------------
void GamePlayScene::UpdateDebug()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		K4E::Object3DCommon::GetInstance()->SetDebugCamera(!K4E::Object3DCommon::GetInstance()->GetDebugCamera());
		K4E::Wireframe::GetInstance()->SetDebugCamera(!K4E::Wireframe::GetInstance()->GetDebugCamera());
		//K4E::ParticleManager::GetInstance()->SetDebugCamera(!K4E::ParticleManager::GetInstance()->GetDebugCamera());
		skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		characters_.SetDebug(!isDebugCamera_);
		input_->SetLockCursor(!isDebugCamera_);
		input_->SetCursorVisible(isDebugCamera_);
	}
#endif // _DEBUG
}

/// -------------------------------------------------------------
///				　		衝突判定更新処理
/// -------------------------------------------------------------
void GamePlayScene::CollisionUpdate()
{
	if (!collisionManager_) return;
	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();
}
