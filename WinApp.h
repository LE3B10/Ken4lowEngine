#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp
{
public:	// メンバ関数

	// シングルトン
	static WinApp* GetInstance();

	// 初期化処理
	void CreateMainWindow();
	// 終了処理
	void Finalize();
	// メッセージ処理
	bool ProcessMessage();

	// getter
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

private: // メンバ関数
	//ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public: // 静的メンバ変数
	//クライアント領域サイズ
	static const uint32_t kClientWidth = 1280;
	static const uint32_t kClientHeight = 720;

private: // メンバ変数
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
