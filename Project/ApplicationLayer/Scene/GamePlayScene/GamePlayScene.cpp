#define NOMINMAX
#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"
#include "AudioManager.h"
#include <SceneManager.h>
#include "LevelLoader.h"
#include "LinearInterpolation.h"

#include "StageRepository.h"
#include "PauseOverlay.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG
#include <CollisionUtility.h>

// ステージごとの設定テーブル
namespace
{
	using std::vector;

	// ここを好きなように調整すればOK
	const std::array<StageConfig, 5> kStageConfigs = {
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			// waves
			{
				WaveConfig{ 3, 25.0f },
				WaveConfig{ 5, 30.0f },
			},
			// bossHp
			1000.0f,
			// bossSpawnPos
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage1: 基本パラメータ
			200.0f,  // maxHp
			0.03f,   // walk
			0.08f,   // chase
			25.0f,   // damage
			0.80f,   // cooldown
			10.0f    // detect
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{ 5, 20.0f },
				WaveConfig{ 7, 25.0f },
				WaveConfig{10, 30.0f },
			},
			2500.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage2: ちょい強化
			400.0f,
			0.032f,
			0.09f,
			30.0f,
			0.75f,
			11.0f
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{ 8, 20.0f },
				WaveConfig{12, 25.0f },
			},
			4000.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage3: さらに強化
			800.0f,
			0.034f,
			0.10f,
			35.0f,
			0.70f,
			12.0f
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{10, 20.0f },
				WaveConfig{14, 25.0f },
			},
			6000.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage4
			1000.0f,
			0.036f,
			0.11f,
			40.0f,
			0.65f,
			13.0f
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{12, 20.0f },
				WaveConfig{16, 25.0f },
				WaveConfig{20, 30.0f },
			},
			8000.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage5: 地獄
			2000.0f,
			0.038f,
			0.12f,
			50.0f,
			0.60f,
			14.0f
		},
	};
}

// 角度差を -π ～ +π に正規化して返す関数
static inline float DeltaAngle(float a, float b)
{
	// -π..+π の差に正規化
	float d = std::fmod(b - a + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
	if (d < 0.0f) d += 2.0f * std::numbers::pi_v<float>;
	return d - std::numbers::pi_v<float>;
}

// 角度をスムーズに目標値へ近づける関数
float GamePlayScene::SmoothDampAngle(float current, float target, float& currentVelocity, float smoothTime, float deltaTime)
{
	const float eps = 1e-5f;
	smoothTime = (smoothTime < eps) ? eps : smoothTime;
	float omega = 2.0f / smoothTime;

	float delta = DeltaAngle(current, target);
	float temp = (currentVelocity + omega * delta) * deltaTime;

	// 近似的な指数減衰（臨界減衰）
	float x = omega * deltaTime;
	float expDecay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

	float result = current + (delta + temp) * expDecay;
	currentVelocity = (currentVelocity - omega * temp) * expDecay;
	return result;
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

	//StartIntroCutscene();
	//gameState_ = GameState::CutScene;   // 最初は必ずCutSceneへ
	Input::GetInstance()->SetLockCursor(true);
	ShowCursor(false);

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	// 衝突マネージャーの初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// プレイヤーの初期化
	player_ = std::make_unique<Player>();
	player_->Initialize();

	// クロスヘアの初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	// アイテムマネージャーの初期化
	itemManager_ = std::make_unique<ItemManager>();
	itemManager_->Initialize();

	auto levelLoader = std::make_unique<LevelLoader>();

	// StageSelectScene から渡されたインデックスを読む
	int stageIndex = 0;
	if (auto idxOpt = StageRepository::GetInstance().GetStartIndex()) {
		stageIndex = std::clamp(*idxOpt, 0, (int)kStageConfigs.size() - 1);
	}

	currentStageIndex_ = stageIndex;
	currentStageConfig_ = kStageConfigs[stageIndex];

	// ステージ読み込みを差し替え
	levelObjectManager_ = std::make_unique<LevelObjectManager>();
	levelObjectManager_->Initialize(
		*levelLoader->LoadLevel(currentStageConfig_.levelJson),
		currentStageConfig_.levelModel
	);
	player_->SetLevelObjectManager(levelObjectManager_.get());

	// ----------------- Result画面用ボタンの初期化 -----------------

	// 画面サイズ
	float screenW = static_cast<float>(dxCommon_->GetSwapChainDesc().Width);
	float screenH = static_cast<float>(dxCommon_->GetSwapChainDesc().Height);

	// ボタンの見た目サイズ（仮）
	float btnW = 220.0f;
	float btnH = 80.0f;
	float margin = 24.0f;

	// 左下：リタイア（タイトルへ戻る）
	retireButtonSprite_ = std::make_unique<Sprite>();
	retireButtonSprite_->Initialize("white.png");
	retireButtonSprite_->SetAnchorPoint({ 0.0f, 1.0f }); // 左下基準
	retireButtonSprite_->SetPosition({ margin, screenH - margin });
	retireButtonSprite_->SetSize({ btnW, btnH });
	// ちょい赤っぽく透かす
	retireButtonSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 0.6f });

	// 右下：リトライ（リスタート）
	retryButtonSprite_ = std::make_unique<Sprite>();
	retryButtonSprite_->Initialize("white.png");
	retryButtonSprite_->SetAnchorPoint({ 1.0f, 1.0f }); // 右下基準
	retryButtonSprite_->SetPosition({ screenW - margin, screenH - margin });
	retryButtonSprite_->SetSize({ btnW, btnH });
	// ちょい緑っぽく透かす
	retryButtonSprite_->SetColor({ 0.2f, 1.0f, 0.2f, 0.6f });

	// マウス当たり矩形も保持（左上原点の箱に直しておく）
	retireRect_.w = btnW;
	retireRect_.h = btnH;
	retireRect_.x = margin;
	retireRect_.y = screenH - margin - btnH;

	retryRect_.w = btnW;
	retryRect_.h = btnH;
	retryRect_.x = screenW - margin - btnW;
	retryRect_.y = screenH - margin - btnH;


	// --------- クリア演出用パネル／テキスト ---------
	clearPanelSprite_ = std::make_unique<Sprite>();
	clearPanelSprite_->Initialize("white.png"); // 仮の真っ白テクスチャ
	clearPanelSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	clearPanelSprite_->SetPosition({ screenW * 0.5f, screenH * 0.5f }); // 位置
	clearPanelSprite_->SetSize({ screenW * 0.6f, screenH * 0.8f });     // 大きさ
	clearPanelSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });          // 最初は透明

	clearTextSprite_ = std::make_unique<Sprite>();
	clearTextSprite_->Initialize("white.png");
	clearTextSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	clearTextSprite_->SetPosition({ screenW * 0.5f, screenH * 0.3f }); // 位置

	gameClearTimer_ = 0.0f;
	gameClearInputAccepted_ = false;

	// --------- 星３つスプライト ---------
	const float starY = screenH * 0.3f;   // パネルの上の方
	const float centerX = screenW * 0.5f;
	const float offsetX = 140.0f;            // 星同士の間隔
	clearStarBaseSize_ = 96.0f;
	clearStarPopDuration_ = 0.25f;

	for (int i = 0; i < kClearStarCount; ++i)
	{
		clearStarSprites_[i] = std::make_unique<Sprite>();
		clearStarSprites_[i]->Initialize("white.png"); // ★星テクスチャを用意しておく
		clearStarSprites_[i]->SetAnchorPoint({ 0.5f, 0.5f });

		float x = centerX + (i - 1) * offsetX; // -1,0,1 で左右に配置
		clearStarSprites_[i]->SetPosition({ x, starY });

		// 最初はサイズ0 & 完全透明
		clearStarSprites_[i]->SetSize({ 0.0f, 0.0f });
		clearStarSprites_[i]->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });

		// パネルが出てから順番に 0.6, 0.9, 1.2 秒後にポップ
		clearStarDelay_[i] = 0.6f + 0.3f * i;
		clearStarBurstPlayed_[i] = false; // 花火はまだ出してない
	}

	// パネルの位置・サイズ（あなたが今調整している値）
	const float panelX = screenW * 0.5f;
	const float panelY = screenH * 0.5f;
	//const float panelW = screenW * 0.45f;
	//const float panelH = screenH * 0.18f;

	// --------- GameClear 三択ボタン(長方形) ---------
	const float optionWidth = 220.0f;           // ボタンの横幅
	const float optionHeight = 48.0f;            // ボタンの高さ

	// 一番上のボタンの Y（パネル中心よりちょい上）
	const float firstOptionY = panelY - optionHeight * 0.5f;
	// ボタン同士の縦方向の間隔
	const float optionGapY = optionHeight + 32.0f;

	// X は全部パネル中央に固定
	const float optionX = panelX;

	for (int i = 0; i < kClearOptionCount; ++i)
	{
		clearOptionSprites_[i] = std::make_unique<Sprite>();
		clearOptionSprites_[i]->Initialize("white.png"); // とりあえず白い四角
		clearOptionSprites_[i]->SetAnchorPoint({ 0.5f, 0.5f });

		// 上から順に縦に並べる
		float y = firstOptionY + optionGapY * i;
		clearOptionSprites_[i]->SetPosition({ optionX, y });
		clearOptionSprites_[i]->SetSize({ optionWidth, optionHeight });

		// 見やすいように少し明るめに
		clearOptionSprites_[i]->SetColor({ 1.0f, 1.0f, 1.0f, 0.9f });

		// クリック判定用の矩形（左上＋サイズ）
		clearOptionRects_[i].w = optionWidth;
		clearOptionRects_[i].h = optionHeight;
		clearOptionRects_[i].x = optionX - optionWidth * 0.5f;
		clearOptionRects_[i].y = y - optionHeight * 0.5f;
	}

	// 敵ウェーブの初期化
	InitializeWaves();

	// --- 雑魚のドロップテーブル設定 ---
	normalDropTable_.Clear();
	normalDropTable_.SetDropChance(60);              // 60% の確率で何か落とす
	normalDropTable_.AddEntry(ItemType::HealSmall, 35);
	normalDropTable_.AddEntry(ItemType::AmmoSmall, 45);
	normalDropTable_.AddEntry(ItemType::ScoreBonus, 20);
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

	// --- ポーズオーバーレイの更新 ---
	if (pauseOverlay_)
	{
		pauseOverlay_->Update();

		if (pauseOverlay_->IsClose())
		{
			bool goTitle = pauseOverlay_->IsGoTitle();

			pauseOverlay_.reset();

			if (!goTitle)
			{
				// 続きから = ゲーム再開のときだけロックON
				isPaused_ = false;
				gameState_ = GameState::Playing;

				Input::GetInstance()->SetLockCursor(true);
				ShowCursor(false);
			}
		}
	}

	// --- ポーズトグル（ESCキーでON/OFF） ---
	if (gameState_ == GameState::Playing || gameState_ == GameState::Paused)
	{
		if (input_->TriggerKey(DIK_ESCAPE))
		{
			if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

			if (pauseOverlay_)
			{
				// ポーズ中に ESC → ポーズ解除
				pauseOverlay_.reset();
				isPaused_ = false;
				gameState_ = GameState::Playing;

				Input::GetInstance()->SetLockCursor(true);
				ShowCursor(false);
			}
			else
			{
				// プレイ中に ESC → ポーズ開始
				Input::GetInstance()->SetLockCursor(false);
				ShowCursor(true);

				pauseOverlay_ = std::make_unique<PauseOverlay>();
				pauseOverlay_->Open(sceneManager_);

				isPaused_ = true;
				gameState_ = GameState::Paused;
			}
		}
	}

	// カットシーン中の処理
	if (gameState_ == GameState::CutScene)
	{
		// カットシーン更新のみ
		bool finished = UpdateIntroCutscene(deltaTime);

		// 画的に必要な更新だけ許可
		skyBox_->Update();
		itemManager_->Update(player_.get(), deltaTime);
		levelObjectManager_->Update();

		if (finished)
		{
			gameState_ = GameState::Playing;
			introDone_ = true;
			/*Input::GetInstance()->SetLockCursor(true);
			ShowCursor(false);*/
		}
		return; // ここで早期リターン → 以降のプレイ処理を止める
	}

	switch (gameState_)
	{
	case GameState::Playing:
	{
		player_->Update(deltaTime);

		for (auto& e : enemies_) {
			e->Update(deltaTime);
		}

		if (boss_) {
			boss_->Update(deltaTime);
		}

		skyBox_->Update();
		crosshair_->Update();

		if (player_->IsDeadNow())
		{
			gameState_ = GameState::GameOver;
			Input::GetInstance()->SetLockCursor(false);
			ShowCursor(true);
		}

		for (auto& e : enemies_) {
			if (e->IsDeadNow()) {
				ItemType drop;
				if (normalDropTable_.RollForDrop(drop)) {
					const Vector3 pos = e->GetDropPosAtDeath(); // ←変更
					itemManager_->Spawn(drop, pos);
				}
			}
		}

		itemManager_->Update(player_.get(), deltaTime);

		// 通常敵の死亡したやつを消す
		enemies_.erase(
			std::remove_if(
				enemies_.begin(), enemies_.end(),
				[](const std::unique_ptr<Enemy>& e) {
					return e->IsDeadNow();
				}),
			enemies_.end()
		);

		bool hasAliveEnemies = !enemies_.empty();  // ← これでOK（このブロック内限定）

		if (!bossSpawned_)
		{
			if (!hasAliveEnemies)
			{
				if (currentWaveIndex_ + 1 < static_cast<int>(waveConfigs_.size()))
				{
					currentWaveIndex_++;
					SpawnWave(currentWaveIndex_);
				}
				else
				{
					allWavesCleared_ = true;
					SpawnBoss();
				}
			}
		}
		else
		{
			if (boss_ && boss_->IsDeadNow())
			{
				Input::GetInstance()->SetLockCursor(false);
				ShowCursor(true);
				OnStageClear();
			}
		}

		break;
	}
	case GameState::Paused:
		break;
	case GameState::GameOver:
	{
		// 死亡演出を最後まで回すためにプレイヤーだけは更新
		player_->Update(deltaTime);

		// 背景など（敵が居れば更新）
		for (auto& e : enemies_)
		{
			e->Update(deltaTime);
		}

		// ボス更新
		if (boss_)
		{
			boss_->Update(deltaTime);
		}

		skyBox_->Update();
		crosshair_->Update();
		itemManager_->Update(player_.get(), deltaTime);
		retryButtonSprite_->Update();
		retireButtonSprite_->Update();

		// キー操作でもまだ残しておく（デバッグ用）
		if (input_->TriggerKey(DIK_R))
		{
			SceneManager::GetInstance()->ChangeScene("GamePlayScene");
			return;
		}
		if (input_->TriggerKey(DIK_Q) || input_->TriggerKey(DIK_ESCAPE))
		{
			SceneManager::GetInstance()->ChangeScene("TitleScene");
			return;
		}

		// --------- マウスクリック判定 ---------
		// マウス座標を取得（例：スクリーン座標のfloat2を返す想定）
		Vector2 mousePos = input_->GetMousePosition(); // 想定API

		auto IsInside = [](const Vector2& p, const ButtonRect& r) {
			return (p.x >= r.x && p.x <= r.x + r.w &&
				p.y >= r.y && p.y <= r.y + r.h);
			};

		bool leftClick = input_->TriggerMouse(0); // 左クリックが「今フレーム押した」

		if (leftClick)
		{
			// 右下(リトライ)
			if (IsInside(mousePos, retryRect_))
			{
				SceneManager::GetInstance()->ChangeScene("GamePlayScene");
				return;
			}

			// 左下(リタイア/タイトルへ)
			if (IsInside(mousePos, retireRect_))
			{
				SceneManager::GetInstance()->ChangeScene("TitleScene");
				return;
			}
		}
	}
	break;

	case GameState::GameClear:
	{
		// 背景やアイテムの軽い更新
		skyBox_->Update();
		itemManager_->Update(player_.get(), deltaTime);

		// タイマー進行
		gameClearTimer_ += deltaTime;

		// =========================================================
		// ① パネル＆テキストのフェードイン
		// =========================================================
		const float panelFadeTime = 0.5f;
		float panelT = std::clamp(gameClearTimer_ / panelFadeTime, 0.0f, 1.0f);

		if (clearPanelSprite_)
		{
			auto col = clearPanelSprite_->GetColor();
			col.w = panelT * 0.8f;  // 少し透けた黒
			clearPanelSprite_->SetColor(col);
			clearPanelSprite_->Update();
		}

		if (clearTextSprite_)
		{
			auto col = clearTextSprite_->GetColor();
			col.w = panelT;
			clearTextSprite_->SetColor(col);
			clearTextSprite_->Update();
		}

		// =========================================================
		// ② 星３つのポップ演出
		// =========================================================
		bool allStarsFinished = true;

		for (int i = 0; i < kClearStarCount; ++i)
		{
			if (!clearStarSprites_[i]) continue;

			float local = (gameClearTimer_ - clearStarDelay_[i]) / clearStarPopDuration_;

			if (local <= 0.0f)
			{
				// まだ出番が来ていない星
				allStarsFinished = false;
				continue;
			}

			float u = std::clamp(local, 0.0f, 1.0f);

			// サイズを0→基準サイズに補間
			float size = clearStarBaseSize_ * u;
			clearStarSprites_[i]->SetSize({ size, size });

			// アルファも0→1
			auto col = clearStarSprites_[i]->GetColor();
			col.w = u;
			clearStarSprites_[i]->SetColor(col);
			clearStarSprites_[i]->Update();

			// ちょうど出始めのフレームで一回だけ花火パーティクルを出す想定
			if (!clearStarBurstPlayed_[i])
			{
				if (local >= 0.0f) // 0を跨いだ瞬間
				{
					// ★ここに自分のパーティクル呼び出しを入れる
					//   例）SpriteParticleManager::GetInstance()->EmitFirework(
					//           clearStarSprites_[i]->GetPosition());
					clearStarBurstPlayed_[i] = true;
				}
			}

			if (u < 1.0f)
			{
				// まだアニメ途中の星がある
				allStarsFinished = false;
			}
		}

		// ボタンも軽くアップデート
		if (retireButtonSprite_) retireButtonSprite_->Update();
		if (retryButtonSprite_)  retryButtonSprite_->Update();
		for (auto& s : clearOptionSprites_) {
			if (s) s->Update();
		}

		// =========================================================
		// ③ 全部の星が出そろったら入力受付開始
		// =========================================================
		// 星のポップが全部終わったら入力受付開始
		if (!gameClearInputAccepted_ && allStarsFinished)
		{
			gameClearInputAccepted_ = true;
		}

		if (gameClearInputAccepted_)
		{
			// --- マウス操作（三つの長方形で三択） ---
			Vector2 mousePos = input_->GetMousePosition();
			auto IsInside = [](const Vector2& p, const ButtonRect& r) {
				return (p.x >= r.x && p.x <= r.x + r.w &&
					p.y >= r.y && p.y <= r.y + r.h);
				};
			bool leftClick = input_->TriggerMouse(0);

			if (leftClick)
			{
				// 0: 左ボタン → もう一度同じステージを遊ぶ
				if (IsInside(mousePos, clearOptionRects_[0]))
				{
					auto& repo = StageRepository::GetInstance();
					// currentStageIndex_ は今のステージ番号を持っている前提
					repo.SetStartIndex(currentStageIndex_);
					SceneManager::GetInstance()->ChangeScene("GamePlayScene");
					return;
				}

				// 1: 真ん中ボタン → 次のステージへ進む
				if (IsInside(mousePos, clearOptionRects_[1]))
				{
					// OnStageClear() 内で repo.SetStartIndex(next) 済みのはずなので、
					// そのまま GamePlayScene をロードすると「次ステージ」が始まる
					SceneManager::GetInstance()->ChangeScene("GamePlayScene");
					return;
				}

				// 2: 右ボタン → セレクトシーンに戻る
				if (IsInside(mousePos, clearOptionRects_[2]))
				{
					SceneManager::GetInstance()->ChangeScene("StageSelectScene");
					return;
				}
			}

			// キーボードで簡易操作：Enter / Space は「セレクトへ戻る」にしておく
			if (input_->TriggerKey(DIK_RETURN) || input_->TriggerKey(DIK_SPACE))
			{
				SceneManager::GetInstance()->ChangeScene("StageSelectScene");
				return;
			}
		}
	}
	break;
	}

	levelObjectManager_->Update();

	// 衝突マネージャの更新はゲーム中のみ
	if (gameState_ == GameState::Playing)
	{
		collisionManager_->Update();
		CheckAllCollisions();
	}
}


/// -------------------------------------------------------------
///				　		3Dオブジェクトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();

	skyBox_->Draw();

#pragma endregion


#pragma region オブジェクト3Dの描画

	if (gameState_ != GameState::CutScene)
	{
		player_->Draw();

		for (auto& e : enemies_) {
			e->Draw();
		}

		if (boss_) {
			boss_->Draw();
		}

		itemManager_->Draw();
	}

	//targetModel_->Draw();
	levelObjectManager_->Draw();

#pragma endregion


#pragma region アニメーションモデルの描画

#pragma endregion


#ifdef _DEBUG
	// 衝突判定を行うオブジェクトの描画
	collisionManager_->Draw();

	// FPSカメラの描画
	//fpsCamera_->DrawDebugCamera();

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });

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

	// カットシーン中はクロスヘア非表示
	if (gameState_ != GameState::CutScene) {
		crosshair_->Draw();
	}

	// ---------- Result(ゲームオーバー / クリア)時のUI ----------
	if (gameState_ == GameState::GameOver || gameState_ == GameState::GameClear)
	{
		if (gameState_ == GameState::GameClear)
		{
			if (clearPanelSprite_) clearPanelSprite_->Draw();
			if (clearTextSprite_)  clearTextSprite_->Draw();

			// 星（飾り）：そのまま描画
			for (auto& s : clearStarSprites_) {
				if (s) s->Draw();
			}

			// ★ 三択ボタン
			for (auto& s : clearOptionSprites_) {
				if (s) s->Draw();
			}
		}

		// GameOver 用の下部ボタンを併用したいならここはそのままでもOK
		if (gameState_ == GameState::GameOver)
		{
			if (retireButtonSprite_) retireButtonSprite_->Draw();
			if (retryButtonSprite_)  retryButtonSprite_->Draw();
		}
	}

	// ---------- ポーズオーバーレイ ----------
	if (pauseOverlay_)
	{
		pauseOverlay_->Draw2D();
	}

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

	player_->DrawImGui();

	for (auto& e : enemies_) {
		e->DrawImGui();
	}

	if (boss_) {
		boss_->DrawImGui();
	}

#ifdef USE_IMGUI

	if (ImGui::Begin("Intro Cutscene")) {

		// --- 再生制御 ---
		if (ImGui::Button("Play / Restart Intro")) {
			RestartIntroCutscene();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Loop while Editing", &introLoop_);
		ImGui::SameLine();
		ImGui::Checkbox("Snap Start Yaw", &forceSnapFirstYaw_);

		ImGui::SliderFloat("Smooth Time (yaw)", &introSmoothTime_, 0.2f, 1.2f, "%.2f");

		// 再生の現在時刻を編集（プレビュー用）
		ImGui::SliderFloat("Time", &introTime_, 0.0f, std::max(0.001f, introLength_), "%.2f");
		// 長さの確認
		ImGui::Text("Length: %.2f sec", introLength_);

		ImGui::Separator();

		// --- カメラキー編集 ---
		if (ImGui::TreeNode("Camera Keys")) {
			for (int i = 0; i < (int)introKeys_.size(); ++i) {
				ImGui::PushID(i);
				ImGui::DragFloat("time", &introKeys_[i].time, 0.01f, 0.0f, 999.0f, "%.2f");
				ImGui::DragFloat3("pos", &introKeys_[i].position.x, 0.1f);
				ImGui::DragFloat3("look", &introKeys_[i].lookAt.x, 0.1f);
				if (ImGui::Button("Remove") && introKeys_.size() > 2) {
					introKeys_.erase(introKeys_.begin() + i);
					// 再生長さを再計算
					introLength_ = introKeys_.back().time;
					ImGui::PopID();
					break;
				}
				ImGui::Separator();
				ImGui::PopID();
			}
			if (ImGui::Button("Add Key (duplicate last)")) {
				auto last = introKeys_.back();
				last.time += 1.0f;
				introKeys_.push_back(last);
				introLength_ = introKeys_.back().time;
			}
			if (ImGui::Button("Sort Keys by time")) {
				std::sort(introKeys_.begin(), introKeys_.end(),
					[](auto& a, auto& b) { return a.time < b.time; });
				introLength_ = introKeys_.back().time;
			}
			ImGui::TreePop();
		}

		ImGui::Separator();

		// --- Yawキー編集 ---
		if (ImGui::TreeNode("Yaw Keys (deg)")) {
			for (int i = 0; i < (int)introYawKeys_.size(); ++i) {
				ImGui::PushID(1000 + i);
				ImGui::DragFloat("time", &introYawKeys_[i].time, 0.01f, 0.0f, 999.0f, "%.2f");
				ImGui::DragFloat("deg", &introYawKeys_[i].deg, 0.1f, -180.0f, 180.0f, "%.1f");
				if (ImGui::Button("Remove##yaw") && introYawKeys_.size() > 1) {
					introYawKeys_.erase(introYawKeys_.begin() + i);
					ImGui::PopID();
					break;
				}
				ImGui::Separator();
				ImGui::PopID();
			}
			if (ImGui::Button("Add Yaw Key (duplicate last)")) {
				auto last = introYawKeys_.back();
				last.time += 1.0f;
				introYawKeys_.push_back(last);
			}
			if (ImGui::Button("Sort Yaw Keys by time")) {
				std::sort(introYawKeys_.begin(), introYawKeys_.end(),
					[](auto& a, auto& b) { return a.time < b.time; });
			}
			ImGui::TreePop();
		}

		ImGui::Separator();

		// その場でプレビュー反映（編集しながら絵を確認できる）
		if (ImGui::Button("Preview Here (no restart)")) {
			// その場の introTime_ で UpdateIntroCutscene(0) を1回呼ぶ
			// deltaTime=0 でも補間結果は出せるよう、Update側は dt=0 を許容している前提
			UpdateIntroCutscene(0.0f);
		}
	}
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
		if (isPaused_) return; // ポーズ中はデバッグカメラ切り替えを無効化

		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		//ParticleManager::GetInstance()->SetDebugCamera(!ParticleManager::GetInstance()->GetDebugCamera());
		skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		player_->SetDebugCamera(!player_->IsDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		Input::GetInstance()->SetLockCursor(!isDebugCamera_);
		ShowCursor(isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG
}

/// -------------------------------------------------------------
///				　			イントロカットシーン
/// -------------------------------------------------------------
void GamePlayScene::StartIntroCutscene()
{
	// --- Yaw キー ---
	introYawKeys_.clear();
	introYawKeys_.push_back({ 0.0f,  10.0f });
	introYawKeys_.push_back({ 2.5f,  -50.0f });
	introYawKeys_.push_back({ 5.0f, -100.0f });
	introYawKeys_.push_back({ 7.0f,  -140.0f });
	introYawKeys_.push_back({ 9.0f,   -20.0f });

	// --- Camera キー　---
	introKeys_.clear();
	introKeys_.push_back({ 0.0f, { -55, 18,  45 }, {  0, -0.5f,  0 } });  // 左奥上空
	introKeys_.push_back({ 3.0f, { 35, 16,  35 }, {  0, -0.5f,  0 } });  // 右手前方向へ
	introKeys_.push_back({ 6.0f, { 20, 10,   0 }, {  0, 0.5f,  0 } }); // スタート地点上空
	introKeys_.push_back({ 10.0f, {  -10,  3,  10 }, {  0, 1.8f,  0 } }); // 徐々に地上へ

	introLength_ = introKeys_.back().time;  // 長さセット
	introTime_ = 0.0f;
}

/// -------------------------------------------------------------
///				　		イントロカットシーン更新
/// -------------------------------------------------------------
bool GamePlayScene::UpdateIntroCutscene(float dt)
{
	introTime_ += dt;
	float t = (introTime_ < introLength_) ? introTime_ : introLength_;

	// 区間を探す
	int i = 0;
	while (i + 1 < (int)introKeys_.size() && introKeys_[i + 1].time < t) ++i;
	int i1 = std::max(0, i - 1);
	int i2 = i;
	int i3 = std::min((int)introKeys_.size() - 1, i + 1);
	int i4 = std::min((int)introKeys_.size() - 1, i + 2);

	float segT0 = introKeys_[i2].time;
	float segT1 = introKeys_[i3].time;
	float u = (segT1 > segT0) ? (t - segT0) / (segT1 - segT0) : 0.0f;
	u = clamp01(u);

	Vector3 pos = CatmullRom(introKeys_[i1].position, introKeys_[i2].position,
		introKeys_[i3].position, introKeys_[i4].position, u);

	// 接線（少し先の位置）
	float uAhead = std::min(1.0f, u + 0.02f);
	Vector3 posAhead = CatmullRom(introKeys_[i1].position, introKeys_[i2].position,
		introKeys_[i3].position, introKeys_[i4].position, uAhead);
	Vector3 dir = Vector3::Normalize(posAhead - pos);
	float pathYaw = std::atan2(dir.x, dir.z); // +Z前提

	float yawOffsetRad = SampleYawDeg(t) * (std::numbers::pi_v<float> / 180.0f);
	float targetYaw = pathYaw + yawOffsetRad;

	// 初回だけ進行方向にスナップ（“変な向きから開始”対策）
	if (forceSnapFirstYaw_ && introTime_ == dt) {
		introYawRad_ = pathYaw + (SampleYawDeg(0.0f) * (std::numbers::pi_v<float> / 180.0f));
		introYawVel_ = 0.0f;
	}

	// スムーズ時間をImGuiから可変
	introYawRad_ = SmoothDampAngle(introYawRad_, targetYaw, introYawVel_, introSmoothTime_, dt);

	auto* cam = Object3DCommon::GetInstance()->GetDefaultCamera();
	cam->SetTranslate(pos);
	cam->SetRotate({ 0.0f, introYawRad_, 0.0f });
	cam->Update();

	// ループ再生対応：終端に達したら巻き戻す or 終了
	if (introTime_ >= introLength_) {
		if (introLoop_) {
			introTime_ = 0.0f;
			// ループ頭でも向きが暴れないよう速度リセット
			introYawVel_ = 0.0f;
			return false; // 継続
		}
		return true; // 終了（Playingへ）
	}
	return false;
}

float GamePlayScene::SampleYawDeg(float t) const
{
	if (introYawKeys_.empty()) return 0.0f;
	if (t <= introYawKeys_.front().time) return introYawKeys_.front().deg;
	if (t >= introYawKeys_.back().time)  return introYawKeys_.back().deg;
	for (size_t i = 0; i + 1 < introYawKeys_.size(); ++i) {
		const auto& a = introYawKeys_[i];
		const auto& b = introYawKeys_[i + 1];
		if (t >= a.time && t <= b.time) {
			float u = (t - a.time) / (b.time - a.time);
			return a.deg + (b.deg - a.deg) * u;
		}
	}
	return 0.0f;
}

void GamePlayScene::RestartIntroCutscene()
{
	introTime_ = 0.0f;
	introDone_ = false;
	introYawRad_ = 0.0f;
	introYawVel_ = 0.0f;

	gameState_ = GameState::CutScene;
	Input::GetInstance()->SetLockCursor(true);
	ShowCursor(false);
}

/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// レベルオブジェクトのコライダーを登録
	for (auto& uptr : levelObjectManager_->GetWorldColliders())
	{
		collisionManager_->AddCollider(uptr.get());
	}

	// コライダーをリストに登録
	collisionManager_->AddCollider(player_.get()); // プレイヤー
	player_->RegisterColliders(collisionManager_.get());

	// 敵キャラクターのコライダーを登録
	for (auto& e : enemies_)
	{
		if (e->IsActive()) {
			collisionManager_->AddCollider(e.get());
		}
	}
	if (boss_ && boss_->IsActive()) {
		collisionManager_->AddCollider(boss_.get());
	}

	// アイテムのコライダーを登録
	itemManager_->RegisterColliders(collisionManager_.get());

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}

/// -------------------------------------------------------------
///				　			ステージクリア時の処理
/// -------------------------------------------------------------
void GamePlayScene::OnStageClear()
{
	// 1) 状態を GameClear に
	gameState_ = GameState::GameClear;

	// 2) クリア演出用タイマー初期化
	gameClearTimer_ = 0.0f;
	gameClearInputAccepted_ = false;

	// 3) ステージ解放処理はそのまま残す
	auto& repo = StageRepository::GetInstance();
	auto stages = repo.GetStages();
	auto startIndexOpt = repo.GetStartIndex();

	if (startIndexOpt && !stages.empty())
	{
		int idx = *startIndexOpt;         // 今回プレイしていたステージ
		int next = idx + 1;

		if (next < (int)stages.size())
		{
			// 次ステージを解放
			if (stages[next].locked) {
				stages[next].locked = false;
				stages[next].justUnlocked = true;
			}

			// 次回セレクト画面での初期選択を「次ステージ」にしておく
			repo.SetStartIndex(next);
		}

		repo.SetStages(stages);
	}
}

void GamePlayScene::InitializeWaves()
{
	waveConfigs_ = currentStageConfig_.waves;

	currentWaveIndex_ = 0;
	allWavesCleared_ = false;

	enemies_.clear();
	boss_.reset();
	bossSpawned_ = false;

	if (!waveConfigs_.empty())
	{
		SpawnWave(currentWaveIndex_);
	}
}

void GamePlayScene::SpawnWave(int waveIndex)
{
	if (waveIndex < 0 || waveIndex >= static_cast<int>(waveConfigs_.size())) return;

	const auto& cfg = waveConfigs_[waveIndex];

	enemies_.clear();

	// ステージ側で決める基準位置
	const Vector3 center = currentStageConfig_.bossSpawnPos;

	for (int i = 0; i < cfg.enemyCount; ++i)
	{
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize();
		enemy->SetPlayerPointer(player_.get());
		enemy->SetLevelObjectManager(levelObjectManager_.get());

		// ステージ依存パラメータを適用
		enemy->ApplyStageParams(
			currentStageConfig_.enemymaxHp,
			currentStageConfig_.enemyWalkSpeed,
			currentStageConfig_.enemyChaseSpeed,
			currentStageConfig_.enemyAttackDamage,
			currentStageConfig_.enemyAttackCooldown,
			currentStageConfig_.enemyDetectRadius
		);

		Vector3 pos;

		// 個別指定があればそれを使う
		if (!cfg.spawnPositions.empty() && i < (int)cfg.spawnPositions.size())
		{
			pos = cfg.spawnPositions[i];
		}
		else
		{
			// なければ「ステージ基準位置」を中心に円形スポーン
			const float angle =
				(2.0f * std::numbers::pi_v<float> / std::max(cfg.enemyCount, 1))
				* i;

			pos = center + Vector3{
				std::cos(angle) * cfg.spawnRadius,
				0.0f,                               // 敵の足元Y
				std::sin(angle) * cfg.spawnRadius
			};
		}

		enemy->SetSpawnPosition(pos);

		enemies_.push_back(std::move(enemy));
	}
}

void GamePlayScene::SpawnBoss()
{
	if (bossSpawned_) return;
	bossSpawned_ = true;

	boss_ = std::make_unique<Enemy>();
	boss_->Initialize();
	boss_->SetPlayerPointer(player_.get());
	boss_->SetLevelObjectManager(levelObjectManager_.get());

	boss_->SetSpawnPosition(currentStageConfig_.bossSpawnPos);
	boss_->SetBoss(true, currentStageConfig_.bossHp);

	// ボスは敵より少し強めにする
	boss_->ApplyStageParams(
		currentStageConfig_.bossHp,
		currentStageConfig_.enemyChaseSpeed * 0.8f,   // 徘徊はあまり速くなくてもOK
		currentStageConfig_.enemyChaseSpeed * 1.2f,   // 追跡はちょい速く
		currentStageConfig_.enemyAttackDamage * 1.5f, // ダメージアップ
		currentStageConfig_.enemyAttackCooldown * 0.8f,
		currentStageConfig_.enemyDetectRadius + 3.0f
	);
}

