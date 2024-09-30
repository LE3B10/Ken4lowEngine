#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp
{
public:	// メンバ関数
	// getter
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// 初期化処理
	void Initialize();
	// 更新処理
	void Update();
	// 終了処理
	void Finalize();
	// メッセージ処理
	bool ProcessMessage();

private: // メンバ関数
	//ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public: // 静的メンバ変数
	//クライアント領域サイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private: // メンバ変数
	// ウィンドウハンドル
	HWND hwnd = nullptr;
	// ウィンドウクラスの設定
	WNDCLASS wc{};

};
