#pragma once
#include "Vector2.h"

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
	/// Main Viewport内で実際にゲーム画面を表示している矩形を保持します。
	/// </summary>
	struct EditorViewportRect
	{
		Vector2 screenMin = { 0.0f, 0.0f };
		Vector2 screenMax = { 0.0f, 0.0f };
		Vector2 imageSize = { 0.0f, 0.0f };
		bool isHovered = false; // Main Viewportのゲーム画像上だけをゲーム入力の対象にする
		bool valid = false;
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

		/// <summary>
		/// スクリーン座標をMain Viewport左上基準へ変換する入口です。
		/// </summary>
		Vector2 ConvertScreenToMainViewportPosition(const Vector2& screenPosition) const;

		/// <summary>
		/// ImGuiのMain Viewport内マウス座標をゲーム基準解像度へ変換します。
		/// </summary>
		bool GetMousePositionInGameViewport(Vector2& outMouse) const;

		const EditorViewportRect& GetMainViewportRect() const { return mainViewportRect_; }
		const Vector2& GetMainViewportScreenPosition() const { return mainViewportScreenPosition_; }
		const Vector2& GetMainViewportSize() const { return mainViewportSize_; }

		EditorWindowState& GetWindowState() { return windowState_; }
		const EditorWindowState& GetWindowState() const { return windowState_; }

	private:
		EditorWindowManager() = default;
		~EditorWindowManager() = default;
		EditorWindowManager(const EditorWindowManager&) = delete;
		EditorWindowManager& operator=(const EditorWindowManager&) = delete;

		EditorWindowState windowState_{};
		EditorViewportRect mainViewportRect_{}; // Main Viewport内で実際にゲーム画面を表示している矩形
		Vector2 mainViewportScreenPosition_ = { 0.0f, 0.0f }; // マウス座標をMain Viewport基準へ変換するための左上座標
		Vector2 mainViewportSize_ = { 0.0f, 0.0f }; // Main Viewport内でGameRenderTargetを表示しているサイズ
	};

} // namespace Ken4lowEngine
