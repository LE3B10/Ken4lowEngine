#pragma once
#include <Windows.h>
#include <cstdint>

/// -------------------------------------------------------------
///				WIndowsAPI - ウィンドウズ作成クラス
/// -------------------------------------------------------------
class WinApp
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトン
	static WinApp* GetInstance();

	// 初期化処理
	void CreateMainWindow(uint32_t Width = kClientWidth, uint32_t Height = kClientHeight);

	// 終了処理
	void Finalize();

	// メッセージ処理
	bool ProcessMessage();

	// ウィンドウハンドルとインスタンスハンドルを取得
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// クライアント領域サイズ
	static inline const UINT32 kClientWidth = 1280;
	static inline const UINT32 kClientHeight = 720;

private: /// ---------- メンバ関数 ---------- ///

	// ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private: /// ---------- メンバ変数 ---------- ///

	// ウィンドウハンドル
	HWND hwnd = nullptr;

	// ウィンドウクラスの設定
	WNDCLASS wc{};

private: /// ---------- コピー禁止 ---------- ///
	
	WinApp() = default;
	~WinApp() = default;
	WinApp(const WinApp&) = delete;
	const WinApp& operator=(const WinApp&) = delete;
};
