#define NOMINMAX
#include "GamePlayScene.h"

#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include <SceneManager.h>

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
	InitializeGameplayObjects();
	SetupNewGame();
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

	dxCommon_ = K4E::DirectXCommon::GetInstance();
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
}

/// -------------------------------------------------------------
/// 新規ゲーム開始準備
/// 
/// - イントロをリセット
/// - イントロ有無を flow に反映
/// - イントロが無い場合は即 Wave 開始
/// -------------------------------------------------------------
void GamePlayScene::SetupNewGame()
{
	if (!introDirector_ || !stageContext_ || !flow_)
	{
		return;
	}

	introDirector_->Reset(*stageContext_, 2.5f);

	const bool hasIntroPoints = introDirector_->HasIntro();
	flow_->ResetForNewGame(hasIntroPoints);

	if (!hasIntroPoints && world_)
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
	// 依存システムが揃っていないなら更新できないので早期リターン
	if (!dxCommon_)	return;

	// フレーム開始時に FPSCounter を更新して deltaTime を取得
	const float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

	// デバッグ停止系
	if (HandleDebugFreeze()) return;

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
/// デバッグ停止処理
/// 
/// F1 などで完全停止に入る処理。
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
			RestartGame();
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

	// プレイヤー死亡
	if (world_->IsPlayerDead())
	{
		flow_->EnterGameOver(input_);
		return;
	}

	// 全Waveクリア
	if (world_->IsAllWavesCleared())
	{
		flow_->EnterGameClear(
			input_,
			[this]()
			{
				if (stageContext_)
				{
					stageContext_->UnlockNextStage();
				}
			});
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

/// -------------------------------------------------------------
/// 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	RestoreCursorState();
	ReleaseGameplayObjects();

	input_ = nullptr;
	dxCommon_ = nullptr;
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
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
/// リスタート
/// 
/// まず完全に終了してから初期化し直す。
/// 現状はこの形が最も分かりやすい。
/// -------------------------------------------------------------
void GamePlayScene::RestartGame()
{
	Finalize();
	Initialize();
}