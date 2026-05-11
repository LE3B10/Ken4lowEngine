#pragma once

namespace Ken4lowEngine
{

	/// <summary>
	/// UE5風エディタUIの各ウィンドウ表示状態を保持します。
	/// </summary>
	struct EditorWindowState
	{
		// WindowメニューのCommonカテゴリで切り替える標準エディタウィンドウです。
		bool showToolbar = true;
		bool showMainViewport = true;
		bool showContentBrowser = true;
		bool showWorldOutliner = true;
		bool showDetails = true;
		bool showOutputLog = true;

		// WindowメニューのRenderingカテゴリで切り替える描画調整ウィンドウです。
		bool showParameters = true;
		bool showDisplay = true;
		bool showPostEffectSettings = true;
		bool showLightEditor = true;

		// WindowメニューのScene Debugカテゴリで切り替えるシーン依存デバッグウィンドウです。
		bool showTitleDebug = true;
		bool showStageSelectDebug = true;
		bool showGameDebug = true;
		bool showWeaponMasterDebug = true;
		bool showTileFadeDebug = true;

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
