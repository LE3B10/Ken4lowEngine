#include "GamePlayFlow.h"

#include <Input.h>
#include <SceneManager.h>

#include "HUDManager.h"
#include "Player.h"

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayFlow::Initialize()
{
	// ポーズメニューの初期化
	pauseMenu_ = std::make_unique<PauseMenu>();
	pauseMenu_->Initialize();

	// 結果画面の初期化
	resultMenu_ = std::make_unique<ResultMenu>();
	resultMenu_->Initialize();

	isPaused_ = false;				// 初期状態は非ポーズ
	resultInputCooldown_ = 0.0f;	// クールタイムは0で初期化
	state_ = State::Playing;		// デフォルトはPlaying状態
}

/// -------------------------------------------------------------
///				　			　終了処理
/// -------------------------------------------------------------
void GamePlayFlow::Finalize()
{
	pauseMenu_.reset();
	resultMenu_.reset();

	isPaused_ = false;
	resultInputCooldown_ = 0.0f;
	state_ = State::Playing;
}

/// -------------------------------------------------------------
///			新しいゲームの開始に向けて状態をリセットする
/// -------------------------------------------------------------
void GamePlayFlow::ResetForNewGame(bool startIntro)
{
	// ポーズキャンセル
	CancelPause();

	// クールタイムリセット
	resultInputCooldown_ = 0.0f;

	// 状態を判定
	state_ = startIntro ? State::Intro : State::Playing;
}

/// -------------------------------------------------------------
///				　			　ゲームプレイ開始
/// -------------------------------------------------------------
void GamePlayFlow::StartPlaying()
{
	// ポーズキャンセル
	CancelPause();

	// クールタイムリセット
	resultInputCooldown_ = 0.0f;

	// 状態を Playing にする
	state_ = State::Playing;
}

/// -------------------------------------------------------------
///				　			　ポーズ開始
/// -------------------------------------------------------------
void GamePlayFlow::EnterPause(Ken4lowEngine::Input* input)
{
	if (isPaused_) { return; }
	if (state_ != State::Playing) { return; }

	// ポーズ状態にする
	isPaused_ = true;

	// ポーズメニューを開く
	if (pauseMenu_)	pauseMenu_->Open();

	if (input)
	{
		input->SetLockCursor(false);	// カーソルロック解除
		input->SetCursorVisible(true);	// カーソル表示
	}
}

/// -------------------------------------------------------------
///				　			　ポーズ解除
/// -------------------------------------------------------------
void GamePlayFlow::ExitPause(Ken4lowEngine::Input* input, bool lockCursorOnResume)
{
	if (!isPaused_) { return; }

	// 非ポーズ状態にする
	isPaused_ = false;

	// ポーズメニューを閉じる
	if (pauseMenu_) pauseMenu_->Close();

	if (input)
	{
		input->SetLockCursor(lockCursorOnResume);	  // カーソルロック
		input->SetCursorVisible(!lockCursorOnResume); // カーソルはロックするなら非表示、ロックしないなら表示
	}
}

/// -------------------------------------------------------------
///				　			　ポーズキャンセル
/// -------------------------------------------------------------
void GamePlayFlow::CancelPause()
{
	if (!isPaused_) { return; }

	// 非ポーズ状態にする
	isPaused_ = false;

	// ポーズを閉じる
	if (pauseMenu_)	pauseMenu_->Close();
}

/// -------------------------------------------------------------
///				　			ゲームクリア開始
/// -------------------------------------------------------------
void GamePlayFlow::EnterGameClear(Ken4lowEngine::Input* input, const std::function<void()>& onUnlockNextStage)
{
	if (state_ == State::GameClear) { return; }

	if (onUnlockNextStage)
	{
		onUnlockNextStage();
	}

	state_ = State::GameClear;
	isPaused_ = false;
	resultInputCooldown_ = 0.25f;

	if (pauseMenu_)
	{
		pauseMenu_->Close();
	}

	if (resultMenu_)
	{
		resultMenu_->Open(ResultMenuMode::GameClear);
	}

	if (input)
	{
		input->SetLockCursor(false);	// カーソルロックを解除
		input->SetCursorVisible(true);  // カーソルを表示
	}
}

/// -------------------------------------------------------------
///				　		  ゲームオーバー開始
/// -------------------------------------------------------------
void GamePlayFlow::EnterGameOver(Ken4lowEngine::Input* input)
{
	if (state_ == State::GameOver) { return; }

	state_ = State::GameOver;
	isPaused_ = false;
	resultInputCooldown_ = 0.25f;

	if (pauseMenu_)	pauseMenu_->Close();

	if (resultMenu_) resultMenu_->Open(ResultMenuMode::GameOver);

	if (input)
	{
		input->SetLockCursor(false);
		input->SetCursorVisible(true);
	}
}

/// -------------------------------------------------------------
///				　			ポーズ中の更新
/// -------------------------------------------------------------
void GamePlayFlow::UpdatePaused(const PausedUpdateContext& ctx)
{
	if (ctx.hud && ctx.player)
	{
		ctx.hud->SetHP(ctx.player->GetHP(), ctx.player->GetMaxHP());
		ctx.hud->Update(ctx.deltaTime);
	}

	if (!pauseMenu_ || !ctx.input)
	{
		return;
	}

	const PauseMenuCommand cmd = pauseMenu_->Update(ctx.input);

	switch (cmd)
	{
	case PauseMenuCommand::Resume:
		ExitPause(ctx.input, ctx.lockCursorOnResume);
		break;

	case PauseMenuCommand::ToStageSelect:
		if (ctx.input)
		{
			ctx.input->SetLockCursor(false);
			ctx.input->SetCursorVisible(true);
		}
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("StageSelectScene");
		}
		break;

	case PauseMenuCommand::ToTitle:
		if (ctx.input)
		{
			ctx.input->SetLockCursor(false);
			ctx.input->SetCursorVisible(true);
		}
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("TitleScene");
		}
		break;

	case PauseMenuCommand::None:
	default:
		break;
	}
}

/// -------------------------------------------------------------
///				　			結果画面の更新
/// -------------------------------------------------------------
void GamePlayFlow::UpdateResult(const ResultUpdateContext& ctx)
{
	if (resultInputCooldown_ > 0.0f)
	{
		resultInputCooldown_ -= ctx.deltaTime;
		if (resultInputCooldown_ < 0.0f)
		{
			resultInputCooldown_ = 0.0f;
		}
	}

	if (!ctx.input || resultInputCooldown_ > 0.0f)
	{
		return;
	}

	ResultMenuCommand cmd = ResultMenuCommand::None;
	if (resultMenu_)
	{
		cmd = resultMenu_->Update(ctx.input);
	}

	switch (cmd)
	{
	case ResultMenuCommand::NextStage:
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("StageSelectScene");
		}
		return;

	case ResultMenuCommand::Retry:
		if (ctx.onRetry)
		{
			ctx.onRetry();
		}
		return;

	case ResultMenuCommand::ToTitle:
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("TitleScene");
		}
		return;

	case ResultMenuCommand::None:
	default:
		break;
	}

	if (ctx.input->TriggerKey(DIK_R))
	{
		if (ctx.onRetry)
		{
			ctx.onRetry();
		}
		return;
	}

	if (ctx.input->TriggerKey(DIK_T))
	{
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("TitleScene");
		}
		return;
	}
}

/// -------------------------------------------------------------
///				　			　UIの描画
/// -------------------------------------------------------------
void GamePlayFlow::DrawUI()
{
	if (isPaused_ && pauseMenu_)
	{
		pauseMenu_->Draw();
	}

	if (IsResultState() && resultMenu_)
	{
		resultMenu_->Update();
		resultMenu_->Draw();
	}
}
