#include "EditorInputCaptureController.h"

namespace Ken4lowEngine
{

	EditorInputCaptureController* EditorInputCaptureController::GetInstance()
	{
		static EditorInputCaptureController instance;
		return &instance;
	}

	void EditorInputCaptureController::ToggleInputCapture()
	{
		// F8はEscと独立したゲーム入力キャプチャ専用トグルにする。
		if (IsGameCaptured())
		{
			ReleaseGameInput();
			return;
		}

		CaptureGameInput();
	}

	void EditorInputCaptureController::CaptureGameInput()
	{
		// Main Viewport上でのみゲーム操作を受け付ける状態へ切り替える。
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorInputCaptureController::ReleaseGameInput()
	{
		// Editor/ImGui操作を優先し、ゲーム側へのマウス入力を止める。
		inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorInputCaptureController::ForceReleaseToEditor()
	{
		// Shift+F8はどの状態からでもEditor操作へ戻す安全な入口にする。
		inputMode_ = EditorInputMode::Editor;
	}

	bool EditorInputCaptureController::IsGameCaptured() const
	{
		return inputMode_ == EditorInputMode::GameCaptured;
	}

	bool EditorInputCaptureController::IsGameReleased() const
	{
		return inputMode_ == EditorInputMode::GameReleased;
	}

	bool EditorInputCaptureController::IsEditorInputMode() const
	{
		return inputMode_ == EditorInputMode::Editor;
	}

	const char* EditorInputCaptureController::GetInputStatusText() const
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

} // namespace Ken4lowEngine
