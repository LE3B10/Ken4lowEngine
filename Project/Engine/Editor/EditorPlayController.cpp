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
		// 再生開始時はゲーム操作へ即入れるように入力キャプチャも同時に有効化する。
		playState_ = EditorPlayState::Play;
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::Pause()
	{
		playState_ = EditorPlayState::Pause;
		inputMode_ = EditorInputMode::GameReleased; // 一時停止後すぐにPause/Stopを操作できるようカーソルをEditorへ返す。
	}

	void EditorPlayController::Stop()
	{
		// 停止時は編集へ戻し、ゲーム入力を解放して各SceneのEsc処理とは独立させる。
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
		// ゲーム入力取得中もゲーム更新は止めず、Main Viewport上の操作だけをゲームへ渡す。
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::ReleaseGameInput()
	{
		// 入力解放中はゲーム更新を継続したまま、操作だけをEditorとImGuiへ戻す。
		inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorPlayController::ForceReleaseToEditor()
	{
		// Shift+F8は現在状態に関係なくEditor操作へ復帰できる非常口にする。
		ReleaseGameInput();
	}

	void EditorPlayController::SetDebugFreezeEnabled(bool enabled)
	{
		// デバッグ停止は入力キャプチャとは別機能としてToolbar表示だけ同期する。
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
			return "再生中";
		case EditorPlayState::Pause:
			return "一時停止";
		case EditorPlayState::Edit:
		default:
			return "編集中";
		}
	}

	const char* EditorPlayController::GetInputModeText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured:
			return "ゲーム入力取得";
		case EditorInputMode::GameReleased:
			return "エディターへ解放";
		case EditorInputMode::Editor:
		default:
			return "エディター入力";
		}
	}

	const char* EditorPlayController::GetInputStatusText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured:
			return "入力: ゲーム操作";
		case EditorInputMode::GameReleased:
			return "入力: エディターへ解放";
		case EditorInputMode::Editor:
		default:
			return "入力: エディター操作";
		}
	}

	const char* EditorPlayController::GetDebugFreezeStatusText() const
	{
		return debugFreezeEnabled_ ? "デバッグ停止: 有効" : "デバッグ停止: 無効";
	}

} // namespace Ken4lowEngine
