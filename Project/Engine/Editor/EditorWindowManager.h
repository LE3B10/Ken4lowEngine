#pragma once

namespace Ken4lowEngine
{

	/// <summary>
	/// UE5風エディタUIの各ウィンドウ表示状態を保持します。
	/// </summary>
	struct EditorWindowState
	{
		bool showToolbar = true;
		bool showMainViewport = true;
		bool showWorldOutliner = true;
		bool showDetails = true;
		bool showContentBrowser = true;
		bool showOutputLog = true;

		bool debugShowCollider = false;
		bool debugShowEnemyInfo = false;
		bool debugShowWaveInfo = false;
		bool debugShowFps = true;
	};

	/// <summary>
	/// UE5風エディタUIのメニュー、ツールバー、各パネルをまとめて描画します。
	/// </summary>
	class EditorWindowManager
	{
	public:
		static EditorWindowManager* GetInstance();

		void Draw();
		void DrawMenuBar();
		void DrawToolbar();
		void DrawMainViewport();
		void DrawWorldOutliner();
		void DrawDetails();
		void DrawContentBrowser();
		void DrawOutputLog();

		EditorWindowState& GetWindowState() { return windowState_; }
		const EditorWindowState& GetWindowState() const { return windowState_; }

	private:
		EditorWindowManager() = default;
		~EditorWindowManager() = default;
		EditorWindowManager(const EditorWindowManager&) = delete;
		EditorWindowManager& operator=(const EditorWindowManager&) = delete;

		EditorWindowState windowState_{};
	};

} // namespace Ken4lowEngine
