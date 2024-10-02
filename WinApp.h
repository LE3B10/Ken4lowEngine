#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp
{
public:	// メンバ関数
	// getter
	HWND GetHwnd() const { return hwnd; }
	WNDCLASS GetHInstance() const { return wc; }

	// 初期化処理
	void CreateMainWindow(uint32_t width, uint32_t height);
	// メッセージ処理
	bool ProcessMessage();

	// シングルトンインスタンス
	static WinApp* GetInstance();

private: // メンバ関数
	//ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private: // メンバ変数
	// ウィンドウハンドル
	HWND hwnd = nullptr;
	// ウィンドウクラスの設定
	WNDCLASS wc{};

	WinApp() = default;
	~WinApp() = default;

	// コピー禁止
	WinApp(const WinApp&) = delete;
	const WinApp& operator = (const WinApp&) = delete;

};
