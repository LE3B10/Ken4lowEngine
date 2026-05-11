#define NOMINMAX
#include "GamePlayScene.h"
#include <Input.h>
#include <SpriteManager.h>
#include <SceneManager.h>
#include <GameTimer.h>
#include <PostEffectManager.h>
#include <Player.h>

#ifdef _DEBUG
#include <DebugCamera.h>
#endif

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
	InitializeSystems();

	loadStep_ = 0;
	isLoadReady_ = false;

	flow_.reset();
	stageContext_.reset();
	world_.reset();
	introDirector_.reset();
	debugTools_.reset();
	frustumCullingDebug_.reset();
	if (hpPostEffectController_)
	{
		hpPostEffectController_->Finalize();
		hpPostEffectController_.reset();
	}

	if (!fadeManager_)
	{
		fadeManager_ = std::make_unique<FadeManager>();
		fadeManager_->Initialize();
	}
}

/// -------------------------------------------------------------
/// エンジン依存システム初期化
/// 
/// - DirectXCommon / Input 取得
/// - カーソルロック
/// - デバッグカメラ初期化
/// -------------------------------------------------------------
void GamePlayScene::InitializeSystems()
{
#ifdef _DEBUG
	K4E::DebugCamera::GetInstance()->Initialize();
#endif
	input_ = K4E::Input::GetInstance();

	if (input_)
	{
		// ゲームプレイ開始時はカーソルをロックして非表示
		input_->SetLockCursor(true);
		input_->SetCursorVisible(false);
	}
}

/// -------------------------------------------------------------
/// ゲームプレイ構成オブジェクト生成
/// -------------------------------------------------------------
void GamePlayScene::InitializeGameplayObjects()
{
	flow_ = std::make_unique<GamePlayFlow>();
	flow_->Initialize();

	stageContext_ = std::make_unique<GamePlayStageContext>();
	stageContext_->InitializeFromRepository();

	world_ = std::make_unique<GamePlayWorld>();
	world_->Initialize(*stageContext_);

	introDirector_ = std::make_unique<GamePlayIntroDirector>();

	debugTools_ = std::make_unique<GamePlayDebugTools>();
	debugTools_->Initialize();

	frustumCullingDebug_ = std::make_unique<FrustumCullingDebugController>();
	frustumCullingDebug_->Initialize();

	InitializeHealthPostEffectController();

	if (!fadeManager_)
	{
		fadeManager_ = std::make_unique<FadeManager>();
		fadeManager_->Initialize();
	}
}


void GamePlayScene::InitializeHealthPostEffectController()
{
	hpPostEffectController_ = std::make_unique<PlayerHealthPostEffectController>();
	hpPostEffectController_->Initialize(K4E::PostEffectManager::GetInstance());

	if (auto* player = world_ ? world_->GetCharacters().GetPlayer() : nullptr)
	{
		player->SetOnDamageTakenCallback([this]()
			{
				if (hpPostEffectController_)
				{
					hpPostEffectController_->NotifyDamageTaken();
				}
			});
	}
}

/// -------------------------------------------------------------
/// 新規ゲーム開始準備
/// 
/// - イントロをリセット
/// - イントロ有無を flow に反映
/// - イントロが無い場合は即 Wave 開始
/// -------------------------------------------------------------
void GamePlayScene::SetupNewGame(bool skipIntro)
{
	if (!introDirector_ || !stageContext_ || !flow_)
	{
		return;
	}

	introDirector_->Reset(*stageContext_, 2.5f);

	const bool hasIntroPoints = introDirector_->HasIntro();
	const bool startIntro = hasIntroPoints && !skipIntro;

	flow_->ResetForNewGame(startIntro);

	if (!startIntro && world_)
	{
		world_->StartWaves();
	}
}

/// -------------------------------------------------------------
/// 更新
/// 
/// ここでは「順番」と「分岐」だけを担当する。
/// 実処理は private 関数へ逃がして見通しを良くする。
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// フレーム開始時に FPSCounter を更新して deltaTime を取得
	const float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	// FadeManager は常に更新
	if (fadeManager_)
	{
		fadeManager_->Update(deltaTime);
	}

	// ロード中は通常ゲームプレイ更新をしない
	if (!isLoadReady_)
	{
		return;
	}

	// デバッグ停止系
	if (HandleDebugFreeze()) return;

	// リトライ遷移中
	if (UpdateRetryTransition()) return;

	// イントロ更新
	if (UpdateIntro(deltaTime))	return;

	// リザルト更新
	if (UpdateResult(deltaTime)) return;

	// ポーズ切り替え
	if (HandlePauseToggle()) return;

	// ポーズ中更新
	if (UpdatePause(deltaTime))	return;

	// 通常時デバッグ更新
	UpdateDebug();

	// 通常ワールド更新
	UpdateWorld(deltaTime);

	// ゲーム終了判定
	CheckGameEnd();
}


/// -------------------------------------------------------------
/// Editor中の更新
///
/// Edit/Pause中はゲーム進行を止め、フェードなど描画確認に必要な軽い更新だけ行う。
/// -------------------------------------------------------------
void GamePlayScene::UpdateEditor(float deltaTime)
{
	// Editor更新では敵/弾/Wave/Player操作/リザルト遷移を進めない。
	if (fadeManager_)
	{
		fadeManager_->Update(deltaTime);
	}
}

/// -------------------------------------------------------------
/// デバッグ停止処理
/// 
/// F10 で完全停止に入る処理。
/// 停止中は通常更新を進めない。
/// -------------------------------------------------------------
bool GamePlayScene::HandleDebugFreeze()
{
#ifdef _DEBUG
	if (debugTools_ && debugTools_->HandleFreezeToggle(input_, flow_.get()))
	{
		return true;
	}

	if (debugTools_ && debugTools_->IsFrozen())
	{
		debugTools_->UpdateFreeze();
		return true;
	}
#endif

	return false;
}

bool GamePlayScene::UpdateRetryTransition()
{
	if (!isRetryTransitionActive_)
	{
		return false;
	}

	// フェードアウト完了待ち
	if (!isRetryRestartDone_)
	{
		if (IsRetryFadeOutFinished())
		{
			RestartGame(true);   // 同じステージをイントロ無しで再構築
			isRetryRestartDone_ = true;

			StartRetryFadeIn();  // ひび割れ→落下で開く
		}

		return true;
	}

	// フェードイン完了待ち
	if (IsRetryFadeInFinished())
	{
		isRetryTransitionActive_ = false;
		isRetryRestartDone_ = false;
	}

	return true;
}

/// -------------------------------------------------------------
/// イントロ更新
/// 
/// イントロ中は通常ゲームプレイ更新を行わない。
/// -------------------------------------------------------------
bool GamePlayScene::UpdateIntro(float deltaTime)
{
	if (!(flow_ && flow_->IsIntro()))
	{
		return false;
	}

	if (introDirector_ && stageContext_ && world_)
	{
		introDirector_->Update(
			deltaTime,
			*flow_,
			*stageContext_,
			*world_,
			input_,
			debugTools_ ? debugTools_->IsDebugCamera() : false);
	}

	return true;
}

/// -------------------------------------------------------------
/// リザルト更新
/// 
/// リザルト中はリザルト専用フローだけ更新する。
/// -------------------------------------------------------------
bool GamePlayScene::UpdateResult(float deltaTime)
{
	if (!(flow_ && flow_->IsResultState()))
	{
		return false;
	}

	GamePlayFlow::ResultUpdateContext ctx{};
	ctx.deltaTime = deltaTime;
	ctx.input = input_;
	ctx.sceneManager = sceneManager_;

	ctx.onRetry = [this]()
		{
			RequestRetryWithFade();
		};

	ctx.onNextStage = [this]()
		{
			if (stageContext_)
			{
				stageContext_->UnlockNextStage();
			}
		};

	flow_->UpdateResult(ctx);
	return true;
}

/// -------------------------------------------------------------
/// ポーズ切り替え処理
/// 
/// ESC 押下で Pause <-> Resume を切り替える。
/// Resume 時のカーソルロックはデバッグカメラ状態を考慮する。
/// -------------------------------------------------------------
bool GamePlayScene::HandlePauseToggle()
{
	if (!(input_ && input_->TriggerKey(DIK_ESCAPE)))
	{
		return false;
	}

	if (flow_ && flow_->IsPaused())
	{
		const bool lockCursorOnResume =
			!(debugTools_ && debugTools_->IsDebugCamera());

		flow_->ExitPause(input_, lockCursorOnResume);
	}
	else if (flow_)
	{
		flow_->EnterPause(input_);
	}

	return true;
}

/// -------------------------------------------------------------
/// ポーズ中更新
/// 
/// HUD / player / scene manager など必要な情報を
/// Context に詰めて flow 側へ渡す。
/// -------------------------------------------------------------
bool GamePlayScene::UpdatePause(float deltaTime)
{
	if (!(flow_ && flow_->IsPaused()))
	{
		return false;
	}

	GamePlayFlow::PausedUpdateContext ctx{};
	ctx.deltaTime = deltaTime;
	ctx.input = input_;
	ctx.hud = world_ ? world_->GetHUDManager() : nullptr;
	ctx.player = world_ ? world_->GetCharacters().GetPlayer() : nullptr;
	ctx.sceneManager = sceneManager_;
	ctx.lockCursorOnResume = !(debugTools_ && debugTools_->IsDebugCamera());

	flow_->UpdatePaused(ctx);
	return true;
}

/// -------------------------------------------------------------
/// 通常時デバッグ更新
/// 
/// 凍結していない通常フレーム中だけ呼ばれる。
/// -------------------------------------------------------------
void GamePlayScene::UpdateDebug()
{
	if (debugTools_)
	{
		debugTools_->UpdateDebugCamera(input_, world_.get());
	}
}

/// -------------------------------------------------------------
/// 通常ワールド更新
/// -------------------------------------------------------------
void GamePlayScene::UpdateWorld(float deltaTime)
{
	if (world_)
	{
		world_->Update(deltaTime);
	}

	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->Update(deltaTime);
	}

	if (hpPostEffectController_)
	{
		hpPostEffectController_->Update(deltaTime, world_ ? world_->GetCharacters().GetPlayer() : nullptr);
	}
}

/// -------------------------------------------------------------
/// クリア / ゲームオーバー判定
/// -------------------------------------------------------------
void GamePlayScene::CheckGameEnd()
{
	if (!world_ || !flow_)
	{
		return;
	}

	// プレイヤー死亡演出が終わったらゲームオーバーへ
	if (auto* player = world_->GetCharacters().GetPlayer())
	{
		if (player->IsDeathSequenceFinished())
		{
			flow_->EnterGameOver(input_);
			return;
		}
	}

	// ステージ固有の失敗条件
	if (world_->IsStageObjectiveFailed())
	{
		flow_->EnterGameOver(input_);
		return;
	}

	// ステージ固有のクリア条件
	if (world_->IsStageObjectiveCleared())
	{
		flow_->EnterGameClear(input_, nullptr);
		return;
	}
}

/// -------------------------------------------------------------
/// 3D描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
	if (world_)
	{
		world_->Draw3D(ShouldHideCharactersDuringIntro());
	}

	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->DrawDebug();
	}
}

/// -------------------------------------------------------------
/// シャドウ描画
/// -------------------------------------------------------------
void GamePlayScene::DrawShadowObjects()
{
	if (world_)
	{
		world_->DrawShadow(ShouldHideCharactersDuringIntro());
	}
}

/// -------------------------------------------------------------
/// 2D描画
/// -------------------------------------------------------------
void GamePlayScene::Draw2DSprites()
{
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();
	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();

	if (world_)
	{
		world_->DrawHUD(ShouldHideCharactersDuringIntro());
	}

	if (flow_)
	{
		flow_->DrawUI();
	}

	if (fadeManager_)
	{
		fadeManager_->Draw2DSprites();
	}
}

/// -------------------------------------------------------------
/// イントロ中のキャラクター非表示判定
/// 
/// Draw3D / DrawShadow / DrawHUD で同じ条件を使うため共通化
/// -------------------------------------------------------------
bool GamePlayScene::ShouldHideCharactersDuringIntro() const
{
	return (flow_ && flow_->IsIntro());
}

void GamePlayScene::RequestRetryWithFade()
{
	if (isRetryTransitionActive_)
	{
		return;
	}

	isRetryTransitionActive_ = true;
	isRetryRestartDone_ = false;

	StartRetryFadeOut();
}

/// -------------------------------------------------------------
/// 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	RestoreCursorState();
	ReleaseGameplayObjects();

	if (fadeManager_)
	{
		fadeManager_->Finalize();
		fadeManager_.reset();
	}

	input_ = nullptr;
}

/// -------------------------------------------------------------
/// カーソル状態を通常へ戻す
/// -------------------------------------------------------------
void GamePlayScene::RestoreCursorState()
{
	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}
}

/// -------------------------------------------------------------
/// 生成済みゲームプレイオブジェクト破棄
/// 
/// 依存が強いものから順に落とす意識で整理。
/// -------------------------------------------------------------
void GamePlayScene::ReleaseGameplayObjects()
{
	if (debugTools_)
	{
		debugTools_->Finalize();
		debugTools_.reset();
	}
	frustumCullingDebug_.reset();

	if (hpPostEffectController_)
	{
		hpPostEffectController_->Finalize();
		hpPostEffectController_.reset();
	}

	introDirector_.reset();

	if (flow_)
	{
		flow_->Finalize();
		flow_.reset();
	}

	if (world_)
	{
		world_->Finalize();
		world_.reset();
	}

	stageContext_.reset();
}

/// -------------------------------------------------------------
/// ImGui描画
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
	if (debugTools_)
	{
		debugTools_->DrawImGui(world_.get());
	}

	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->DrawImGui();
	}

	if (hpPostEffectController_)
	{
		hpPostEffectController_->DrawImGui();
	}
#endif // USE_IMGUI
}

void GamePlayScene::StartLoad()
{
	loadStep_ = 0;
	isLoadReady_ = false;
}

void GamePlayScene::UpdateLoad()
{
	switch (loadStep_)
	{
	case 0:
		flow_ = std::make_unique<GamePlayFlow>();
		flow_->Initialize();
		++loadStep_;
		break;

	case 1:
		stageContext_ = std::make_unique<GamePlayStageContext>();
		stageContext_->InitializeFromRepository();
		++loadStep_;
		break;

	case 2:
		world_ = std::make_unique<GamePlayWorld>();
		world_->Initialize(*stageContext_);
		++loadStep_;
		break;

	case 3:
		introDirector_ = std::make_unique<GamePlayIntroDirector>();
		++loadStep_;
		break;

	case 4:
		debugTools_ = std::make_unique<GamePlayDebugTools>();
		debugTools_->Initialize();
		frustumCullingDebug_ = std::make_unique<FrustumCullingDebugController>();
		frustumCullingDebug_->Initialize();

		InitializeHealthPostEffectController();
		++loadStep_;
		break;

	case 5:
		SetupNewGame(false); // 初回はイントロあり
		isLoadReady_ = true;
		++loadStep_;
		break;

	default:
		break;
	}
}

void GamePlayScene::StartUnload()
{
	unloadStep_ = 0;
	isUnloadReady_ = false;
}

void GamePlayScene::UpdateUnload()
{
	switch (unloadStep_)
	{
	case 0:
		// まず入力系を軽く戻す
		RestoreCursorState();
		++unloadStep_;
		break;

	case 1:
		// デバッグツール解放
		if (debugTools_)
		{
			debugTools_->Finalize();
			debugTools_.reset();
		}
		frustumCullingDebug_.reset();
		if (hpPostEffectController_)
		{
			hpPostEffectController_->Finalize();
			hpPostEffectController_.reset();
		}
		++unloadStep_;
		break;

	case 2:
		// フローとイントロ系を解放
		introDirector_.reset();

		if (flow_)
		{
			flow_->Finalize();
			flow_.reset();
		}
		++unloadStep_;
		break;

	case 3:
		// 一番重そうな world を単独フレームで解放
		if (world_)
		{
			world_->Finalize();
			world_.reset();
		}
		++unloadStep_;
		break;

	case 4:
		// ステージ文脈を最後に解放
		stageContext_.reset();
		++unloadStep_;
		break;

	case 5:
		// FadeManager は SceneManager 側のフェードがあるので、ここで消してもよい
		if (fadeManager_)
		{
			fadeManager_->Finalize();
			fadeManager_.reset();
		}

		input_ = nullptr;

		isUnloadReady_ = true;
		++unloadStep_;
		break;

	default:
		break;
	}
}

bool GamePlayScene::IsReadyToStartUncover() const
{
	return isLoadReady_;
}

bool GamePlayScene::IsReadyToSwapOut() const
{
	return isUnloadReady_;
}

/// -------------------------------------------------------------
/// リスタート
/// 
/// まず完全に終了してから初期化し直す。
/// 現状はこの形が最も分かりやすい。
/// -------------------------------------------------------------
void GamePlayScene::RestartGame(bool skipIntro)
{
	// カーソルをゲームプレイ向けに戻す
	if (input_)
	{
		input_->SetLockCursor(true);
		input_->SetCursorVisible(false);
	}

	// フェードは残したまま、ゲームプレイ構成だけ再生成
	ReleaseGameplayObjects();
	InitializeGameplayObjects();
	SetupNewGame(skipIntro);
}

void GamePlayScene::StartRetryFadeOut()
{
	if (fadeManager_)
	{
		fadeManager_->StartCover();
	}
}

bool GamePlayScene::IsRetryFadeOutFinished() const
{
	return fadeManager_ && fadeManager_->IsFullyCovered();
}

void GamePlayScene::StartRetryFadeIn()
{
	if (fadeManager_)
	{
		fadeManager_->StartCrack();
	}
}

bool GamePlayScene::IsRetryFadeInFinished() const
{
	return fadeManager_ && fadeManager_->IsDropDone();
}