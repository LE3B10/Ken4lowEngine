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

#include "StageRepository.h"
#include "PauseOverlay.h"

#include "GameClearState.h"
#include "GameLoadState.h"
#include "GameOverState.h"
#include "GamePauseState.h"
#include "GamePlayingState.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#include <array>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

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

	// クロスヘアの初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	reloadCircle_ = std::make_unique<ReloadCircle>();
	reloadCircle_->Initialize("reload-circle.png");
	reloadCircle_->SetVisible(false);

	weaponSlot_ = std::make_unique<WeaponSlot>();
	weaponSlot_->Initialize("slot_frame.png", "slot_frame_selected.png");
	weaponSlot_->InitializeSlotNumbers("numbers02.png", 50.0f, 50.0f, { 8.0f, 8.0f }, 2.0f, 32, 32);

	// キューブアイコン
	// 武器カテゴリ別アイコン（スロット0..5）
	const std::array<std::string, WeaponSlot::kSlotCount> weaponIcons = {
		"icon/primary_icon.png",
		"icon/backup_icon.png",
		"icon/melee_icon.png",
		"icon/special_icon.png",
		"icon/sniper_icon.png",
		"icon/heavy_icon.png"
	};
	weaponSlot_->InitializeIcons(weaponIcons);

	weaponSlot_->InitializeAmmoDelimiter(
		"icon/slash_icon.png",
		{ 20.0f, 20.0f },   // 数字が20x20ならこれがちょうど良い
		{ 0.0f, 0.0f }      // 微調整したいならここでオフセット
	);

	// 弾薬表示初期化
	weaponSlot_->InitializeAmmoNumbers("Number.png",
		50, 50,
		{ 10, 10 },
		-5.0f,   // spacingは小さく
		20.0f, 20.0f); // drawサイズ

	// フェードオーバーレイの初期化
	InitializeFadeOverlay();

	// アイテムの初期化
	InitializeItems();

	// クリアエフェクトスプライトの初期化
	InitializeClearEffectSprites();

	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->Initialize();

	// 最初の状態をセット （Loading）
	ChangeState(std::make_unique<GameLoadState>());
	state_ = State::Loading; // 最初のステート
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

	// ----- 状態更新共通処理 -----
	if (currentState_)
	{
		// ステートクラスに丸投げ
		currentState_->Update(this, deltaTime);
	}

	if (weaponSlot_ && player_)
	{
		weaponSlot_->Update(*player_->GetWeaponManager()); // ← Player側に getter を用意
	}

	// ★リロード円の更新
	if (reloadCircle_ && player_->GetWeaponManager())
	{
		const auto ammo = player_->GetWeaponManager()->GetCurrentAmmoView();

		if (ammo.usesAmmo && ammo.reloading && ammo.reloadSec > 0.0f)
		{
			const float p = ammo.reloadT / ammo.reloadSec; // 0..1
			reloadCircle_->SetReloading(true, p);
		}
		else
		{
			reloadCircle_->SetReloading(false, 0.0f);
		}

		reloadCircle_->Update();
	}

	hudManager_->Update();
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

	// ステートクラスに丸投げ
	if (currentState_) { currentState_->Draw3DObjects(this); }

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

	weaponSlot_->Draw();

	if (reloadCircle_) reloadCircle_->Draw();

	// ステートクラスに丸投げ
	if (currentState_) { currentState_->Draw2DSprites(this); }

	hudManager_->Draw();

#pragma endregion
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	Input::GetInstance()->SetLockCursor(false);
	ShowCursor(true);

	// ステートを抜ける（ステートがシーン内リソースを握ってる可能性がある）
	if (currentState_) {
		currentState_->Exit(this);
	}
	currentState_.reset();

	// 参照が残りやすいものから順に解放（安全寄り）
	pauseOverlay_.reset();

	// 衝突は内部に “生ポインタのリスト” を持ちやすいので先に無効化
	if (collisionManager_) {
		collisionManager_->Reset();
	}
	collisionManager_.reset();

	hudManager_.reset();

	// ゲームオブジェクト
	boss_.reset();
	enemies_.clear();
	enemies_.shrink_to_fit();

	itemManager_.reset();
	levelObjectManager_.reset();

	ballisticEffect_.reset();
	weaponSlot_.reset();
	reloadCircle_.reset();
	crosshair_.reset();
	player_.reset();

	// UI/演出スプライト
	retryButtonSprite_.reset();
	retireButtonSprite_.reset();
	clearPanelSprite_.reset();
	clearTextSprite_.reset();
	for (auto& s : clearStarSprites_) { s.reset(); }
	for (auto& s : clearOptionSprites_) { s.reset(); }
	fadeSprite_.reset();

	// 3D背景など
	skyBox_.reset();

	// 生ポインタ参照は最後に切る
	input_ = nullptr;
	dxCommon_ = nullptr;
}

void GamePlayScene::ChangeState(std::unique_ptr<IGamePlaySceneState> newState)
{
	// 今のステートから抜ける
	if (currentState_) {
		currentState_->Exit(this);
	}

	// ステート差し替え
	currentState_ = std::move(newState);

	// 新しいステートに入る
	if (currentState_) {
		currentState_->Enter(this);
	}
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

	hudManager_->DrawImGui();

#ifdef USE_IMGUI



#endif // USE_IMGUI
}

void GamePlayScene::InitializeFadeOverlay()
{
	// フェード用スプライト（黒の1x1テクスチャを用意しておく）
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize("white.png");
	fadeSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	fadeSprite_->SetPosition({ dxCommon_->GetClientWidth() * 0.5f, dxCommon_->GetClientHeight() * 0.5f });
	fadeSprite_->SetSize({ static_cast<float>(dxCommon_->GetClientWidth()), static_cast<float>(dxCommon_->GetClientHeight()) });   // 画面全体を覆う
	fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f }); // 最初は透明
	fadeAlpha_ = 0.0f;
}

void GamePlayScene::InitializeItems()
{
	// アイテムマネージャーの初期化
	itemManager_ = std::make_unique<ItemManager>();
	itemManager_->Initialize();

	// --- 雑魚のドロップテーブル設定 ---
	normalDropTable_.Clear();
	normalDropTable_.SetDropChance(60);              // 60% の確率で何か落とす
	normalDropTable_.AddEntry(ItemType::HealSmall, 35);	 // 小回復 : 35%
	normalDropTable_.AddEntry(ItemType::AmmoSmall, 45);	 // 弾薬多め : 45%
	normalDropTable_.AddEntry(ItemType::ScoreBonus, 20); // スコアボーナスも追加 : 20%
}

void GamePlayScene::InitializeClearEffectSprites()
{
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
