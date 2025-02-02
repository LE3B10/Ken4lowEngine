#include "WinApp.h"

#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
WinApp* WinApp::GetInstance()
{
	static WinApp instance;
	return &instance;
}



/// -------------------------------------------------------------
///					メインウィンドウの作成
/// -------------------------------------------------------------
void WinApp::CreateMainWindow(uint32_t Width, uint32_t Height)
{
	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// ウィンドウクラスを登録する
	wc.lpfnWndProc = WindowProc;				 // ウィンドウプロシージャ
	wc.lpszClassName = L"CG2WindowClass";		 // ウィンドウクラス名（なんでもいい）
	wc.hInstance = GetModuleHandle(nullptr);	 // インスタンスハンドル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // カーソル
	// ウィンドウクラスを登録する
	RegisterClass(&wc);

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = {};
	wrc.right = Width;
	wrc.bottom = Height;

	// クライアント領域をmとに実際のサイズにwrcに変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの生成
	hwnd = CreateWindow(
		wc.lpszClassName,		// 利用するクラス名
		L"GE3",					// タイトルバーの文字
		WS_OVERLAPPEDWINDOW,	// よく見るウィンドウスタイル
		CW_USEDEFAULT,			// 表示X座標
		CW_USEDEFAULT,			// 表示Y座標
		wrc.right - wrc.left,	// ウィンドウ横幅
		wrc.bottom - wrc.top,	// ウィンドウ縦幅
		nullptr,				// 親ウィンドウハンドル
		nullptr,				// メニューハンドル
		wc.hInstance,			// インスタンスハンドル
		nullptr);				// オプション

	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);
}



/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void WinApp::Finalize()
{
	CloseWindow(hwnd);
	// COMの終了処理
	CoUninitialize();
}



/// -------------------------------------------------------------
///						メッセージ処理
/// -------------------------------------------------------------
bool WinApp::ProcessMessage()
{
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		// WM_QUITを確認
		if (msg.message == WM_QUIT)
		{
			return true; // 終了リクエストを検知
		}
	}
	return false;  // 実行継続
}



/// -------------------------------------------------------------
///					ウィンドウプロシージャ
/// -------------------------------------------------------------
LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifndef _Debug
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}
#endif // !_Debug

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

		// ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}