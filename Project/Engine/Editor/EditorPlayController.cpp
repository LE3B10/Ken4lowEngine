#include "EditorPlayController.h"
#include "EditorCommandHistory.h"

namespace Ken4lowEngine
{
	EditorPlayController* EditorPlayController::GetInstance()
	{
		static EditorPlayController instance;
		return &instance;
	}

	void EditorPlayController::Play()
	{
		if (playState_ == EditorPlayState::Edit)
		{
			EditorCommandHistory::GetInstance()->Clear(); // Runtime更新でEditor対象が変化する前に履歴を破棄する。
		}
		playState_ = EditorPlayState::Play;
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::Pause()
	{
		playState_ = EditorPlayState::Pause;
		inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorPlayController::Stop()
	{
		playState_ = EditorPlayState::Edit;
		inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorPlayController::ToggleInputCapture()
	{
		if (IsGameCaptured())
		{
			ReleaseGameInput();
			return;
		}
		CaptureGameInput();
	}

	void EditorPlayController::CaptureGameInput() { inputMode_ = EditorInputMode::GameCaptured; }
	void EditorPlayController::ReleaseGameInput() { inputMode_ = EditorInputMode::GameReleased; }
	void EditorPlayController::ForceReleaseToEditor() { ReleaseGameInput(); }
	void EditorPlayController::SetDebugFreezeEnabled(bool enabled) { debugFreezeEnabled_ = enabled; }
	bool EditorPlayController::IsEditing() const { return playState_ == EditorPlayState::Edit; }
	bool EditorPlayController::IsPlaying() const { return playState_ == EditorPlayState::Play; }
	bool EditorPlayController::IsPaused() const { return playState_ == EditorPlayState::Pause; }
	bool EditorPlayController::IsGameCaptured() const { return inputMode_ == EditorInputMode::GameCaptured; }
	bool EditorPlayController::IsGameReleased() const { return inputMode_ == EditorInputMode::GameReleased; }
	bool EditorPlayController::IsEditorInputMode() const { return inputMode_ == EditorInputMode::Editor; }

	const char* EditorPlayController::GetPlayStateText() const
	{
		switch (playState_)
		{
		case EditorPlayState::Play: return "再生中";
		case EditorPlayState::Pause: return "一時停止";
		case EditorPlayState::Edit:
		default: return "編集中";
		}
	}

	const char* EditorPlayController::GetInputModeText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured: return "ゲーム入力取得";
		case EditorInputMode::GameReleased: return "エディターへ解放";
		case EditorInputMode::Editor:
		default: return "エディター入力";
		}
	}

	const char* EditorPlayController::GetInputStatusText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured: return "入力: ゲーム操作";
		case EditorInputMode::GameReleased: return "入力: エディターへ解放";
		case EditorInputMode::Editor:
		default: return "入力: エディター操作";
		}
	}

	const char* EditorPlayController::GetDebugFreezeStatusText() const
	{
		return debugFreezeEnabled_ ? "デバッグ停止: 有効" : "デバッグ停止: 無効";
	}
} // namespace Ken4lowEngine
