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

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

#include "Player.h"


#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"
#include "WeaponMasterDataWriter.h"
#include <filesystem>

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

	characters_.SpawnEnemy(EnemyArchetype::RifleGrunt, { -12.0f, 0.0f, 30.0f });
	characters_.SpawnEnemy(EnemyArchetype::SMGFlanker, { 12.0f, 0.0f, 25.0f });
	characters_.SpawnEnemy(EnemyArchetype::Sniper, { 0.0f, 0.0f, 40.0f });

	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->SetPlayer(characters_.GetPlayer());
	hudManager_->Initialize();
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// ------------------------------------------------------------
	// Pause toggle (ESC)
	// ------------------------------------------------------------
	if (input_->TriggerKey(DIK_ESCAPE))
	{
		isPaused_ = !isPaused_;
		if (isPaused_)
		{
			// ポーズ中はカーソルを出してロック解除
			input_->SetLockCursor(false);
			input_->SetCursorVisible(true);
		}
		else
		{
			// 復帰時はデバッグカメラ状態に合わせて戻す
			const bool lock = !isDebugCamera_;
			input_->SetLockCursor(lock);
			input_->SetCursorVisible(!lock);
		}
	}

	// ポーズ中はゲーム進行を止める（ESCで解除可能）
	if (isPaused_)
	{
		// HUDは更新してOK（アニメ無しなら実質固定表示）
		if (hudManager_)
		{
			hudManager_->SetHP(characters_.GetPlayer()->GetHP(), characters_.GetPlayer()->GetMaxHP());
			hudManager_->Update();
		}
		return;
	}

	// プレイヤーが死んだらゲームを初期化し直す（リトライ）
	if (characters_.GetPlayer()->GetHP() <= 0)
	{
		Finalize();
		Initialize();
		return;
	}

	// エネミーが全滅したら次のウェーブをスポーン
	if (characters_.GetEnemyCount() == 0)
	{
		characters_.SpawnEnemy(EnemyArchetype::RifleGrunt, { -12.0f, 0.0f, 30.0f });
		characters_.SpawnEnemy(EnemyArchetype::SMGFlanker, { 12.0f, 0.0f, 25.0f });
		characters_.SpawnEnemy(EnemyArchetype::Sniper, { 0.0f, 0.0f, 40.0f });
	}
	

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

	hudManager_->SetHP(characters_.GetPlayer()->GetHP(), characters_.GetPlayer()->GetMaxHP());
	hudManager_->Update();
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

#endif // _DEBUG

	// ワイヤーフレームの描画
	K4E::Wireframe::GetInstance()->DrawGrid(200.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });
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

	hudManager_->Draw();

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

	hudManager_.reset();

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
#ifdef USE_IMGUI

	// ライト
	K4E::LightManager::GetInstance()->DrawImGui();


	characters_.DrawImGui();

	/// ---------- 武器マスターデータエディタ ---------- ///
	static WeaponMasterDataDatabase weaponDB;
	static WeaponMasterDataEditor weaponEditor;
	static WeaponEditorHooks hooks;
	static bool initialized = false;
	static int32_t lastAppliedID = 0;

	static const std::filesystem::path kRoot = "Resources/JSON/weapons";

	if (!initialized)
	{
		initialized = true;

		// ★ ここでロードする（既存jsonを表示したいなら必須）
		// 空から始めたいなら LoadFromDirectory をコメントアウトしてOK
		{
			std::string err;
			weaponDB.LoadFromDirectory(kRoot, &err);
			// errをImGuiに出したいなら保持して表示
		}

		hooks.SaveAll = [&]()
			{
				std::string err;
				WeaponMasterDataWriter::SaveAllByCategory(weaponDB, kRoot, &err);
			};

		hooks.RequestReloadFocus = [](int32_t) {};
		hooks.RebuildLoadout = []() {};

		hooks.ApplyToRuntimeIfCurrent =
			[&](int32_t weaponID, const FWeaponMasterData&)
			{
				lastAppliedID = weaponID;
			};

		hooks.RequestDelete =
			[&](int32_t weaponID)
			{
				std::string err;

				// ディスク上のjson削除
				WeaponMasterDataWriter::DeleteFilesByWeaponID(kRoot, weaponID, &err);

				// DBから削除
				weaponDB.RemoveByID(weaponID);
			};

		hooks.RequestAdd = [](const std::string&, int32_t) {}; // Editor側はDB直操作なので空でOK

		// ★ これがあると毎回空になる。1枚目が「常に0」なのはこれが原因。
		// weaponDB.Clear();
	}

	// ★ 外側で Begin/End しない。これだけ呼ぶ。
	weaponEditor.DrawImGui(weaponDB, hooks);

	// どうしても lastAppliedID を別窓で出したいなら “別タイトル” で出す
	ImGui::Begin("Weapon Master Debug");
	ImGui::Text("Last Applied ID: %d", lastAppliedID);
	ImGui::End();

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

		characters_.GetPlayer()->SetDebugCamera(isDebugCamera_);

		// カーソルのロックと表示を切り替える
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
