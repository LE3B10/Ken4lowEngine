#pragma once

namespace Ken4lowEngine
{

	enum class EditorInputMode
	{
		Editor,
		GameCaptured,
		GameReleased,
	};

	/// <summary>
	/// EditorとMain Viewport上のゲーム入力キャプチャ状態を管理します。
	/// </summary>
	class EditorInputCaptureController
	{
	public:
		static EditorInputCaptureController* GetInstance();

		void ToggleInputCapture();
		void CaptureGameInput();
		void ReleaseGameInput();
		void ForceReleaseToEditor();

		bool IsGameCaptured() const;
		bool IsGameReleased() const;
		bool IsEditorInputMode() const;

		EditorInputMode GetInputMode() const { return inputMode_; }
		const char* GetInputStatusText() const;

	private:
		EditorInputCaptureController() = default;
		~EditorInputCaptureController() = default;
		EditorInputCaptureController(const EditorInputCaptureController&) = delete;
		EditorInputCaptureController& operator=(const EditorInputCaptureController&) = delete;

		EditorInputMode inputMode_ = EditorInputMode::GameReleased; // 起動直後はEditor操作を優先して誤クリックを防ぐ
	};

} // namespace Ken4lowEngine
