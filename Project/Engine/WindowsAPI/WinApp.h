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
	void CreateMainWindow(uint32_t Width, uint32_t Height);

	// 終了処理
	void Finalize();

	// メッセージ処理
	bool ProcessMessage();

	// ゲッター
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

private: /// ---------- メンバ関数 ---------- ///
	
	// ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private: /// ---------- メンバ関数 ---------- ///

	// ウィンドウハンドル
	HWND hwnd = nullptr;
	
	// ウィンドウクラスの設定
	WNDCLASS wc{};

	WinApp() = default;
	~WinApp() = default;

	// コピー禁止
	WinApp(const WinApp&) = delete;
	const WinApp& operator=(const WinApp&) = delete;
};
