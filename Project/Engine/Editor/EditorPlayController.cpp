#include "EditorPlayController.h"

namespace Ken4lowEngine
{

	EditorPlayController* EditorPlayController::GetInstance()
	{
		static EditorPlayController instance;
		return &instance;
	}

	void EditorPlayController::Play()
	{
		// Play開始時はゲーム操作へ即入れるように入力キャプチャも同時に有効化する。
		playState_ = EditorPlayState::Play;
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::Pause()
	{
		// Pauseは入力キャプチャ状態を変えず、Play状態だけを一時停止へ遷移させる。
		playState_ = EditorPlayState::Pause;
	}

	void EditorPlayController::Stop()
	{
		// Stop時は編集へ戻し、ゲーム入力を解放して各SceneのEsc処理とは独立させる。
		playState_ = EditorPlayState::Edit;
		inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorPlayController::ToggleInputCapture()
	{
		// F8はEscと独立したゲーム入力キャプチャ専用トグルにする。
		if (IsGameCaptured())
		{
			ReleaseGameInput();
			return;
		}

		CaptureGameInput();
	}

	void EditorPlayController::CaptureGameInput()
	{
		// GameCapturedはゲーム更新を止めずMain Viewport上の操作だけをゲームへ戻す。
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::ReleaseGameInput()
	{
		// GameReleasedはゲーム更新を継続したまま入力だけEditor/ImGuiへ解放する。
		inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorPlayController::ForceReleaseToEditor()
	{
		// Shift+F8は現在状態に関係なくEditor操作へ復帰できる非常口にする。
		ReleaseGameInput();
	}

	void EditorPlayController::SetDebugFreezeEnabled(bool enabled)
	{
		// Debug Freezeは入力キャプチャとは別機能としてToolbar表示だけ同期する。
		debugFreezeEnabled_ = enabled;
	}

	bool EditorPlayController::IsEditing() const
	{
		return playState_ == EditorPlayState::Edit;
	}

	bool EditorPlayController::IsPlaying() const
	{
		return playState_ == EditorPlayState::Play;
	}

	bool EditorPlayController::IsPaused() const
	{
		return playState_ == EditorPlayState::Pause;
	}

	bool EditorPlayController::IsGameCaptured() const
	{
		return inputMode_ == EditorInputMode::GameCaptured;
	}

	bool EditorPlayController::IsGameReleased() const
	{
		return inputMode_ == EditorInputMode::GameReleased;
	}

	bool EditorPlayController::IsEditorInputMode() const
	{
		return inputMode_ == EditorInputMode::Editor;
	}

	const char* EditorPlayController::GetPlayStateText() const
	{
		switch (playState_)
		{
		case EditorPlayState::Play:
			return "Play";
		case EditorPlayState::Pause:
			return "Pause";
		case EditorPlayState::Edit:
		default:
			return "Edit";
		}
	}

	const char* EditorPlayController::GetInputModeText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured:
			return "GameCaptured";
		case EditorInputMode::GameReleased:
			return "GameReleased";
		case EditorInputMode::Editor:
		default:
			return "Editor";
		}
	}

	const char* EditorPlayController::GetInputStatusText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured:
			return "Input: Game Captured";
		case EditorInputMode::GameReleased:
			return "Input: Editor Released";
		case EditorInputMode::Editor:
		default:
			return "Input: Editor";
		}
	}

	const char* EditorPlayController::GetDebugFreezeStatusText() const
	{
		return debugFreezeEnabled_ ? "Debug Freeze: ON" : "Debug Freeze: OFF";
	}

} // namespace Ken4lowEngine
