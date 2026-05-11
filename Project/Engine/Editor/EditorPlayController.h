#pragma once

namespace Ken4lowEngine
{

	enum class EditorPlayState
	{
		Edit,
		Play,
		Pause,
	};

	enum class EditorInputMode
	{
		Editor,
		GameCaptured,
		GameReleased,
	};

	/// <summary>
	/// EditorのPlay状態とMain Viewport上のゲーム入力キャプチャ状態を管理します。
	/// </summary>
	class EditorPlayController
	{
	public:
		static EditorPlayController* GetInstance();

		void Play();
		void Pause();
		void Stop();
		void ToggleInputCapture();

		bool IsEditing() const;
		bool IsPlaying() const;
		bool IsPaused() const;
		bool IsGameCaptured() const;
		bool IsGameReleased() const;
		bool IsEditorInputMode() const;

		EditorPlayState GetPlayState() const { return playState_; }
		EditorInputMode GetInputMode() const { return inputMode_; }
		const char* GetPlayStateText() const;
		const char* GetInputModeText() const;
		const char* GetInputStatusText() const;

	private:
		EditorPlayController() = default;
		~EditorPlayController() = default;
		EditorPlayController(const EditorPlayController&) = delete;
		EditorPlayController& operator=(const EditorPlayController&) = delete;

		EditorPlayState playState_ = EditorPlayState::Edit; // 起動直後はEditor編集状態から開始する。
		EditorInputMode inputMode_ = EditorInputMode::GameReleased; // 起動直後はEditor操作を優先して誤クリックを防ぐ。
	};

} // namespace Ken4lowEngine
