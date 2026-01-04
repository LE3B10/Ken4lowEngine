#include "WinApp.h"
#include <stdexcept>
#include <vector>

#ifdef USE_IMGUI
#include <imgui_impl_win32.h>
/// ---------- ImGuiのウィンドウプロシージャ ---------- ///
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // USE_IMGUI

namespace
{
	struct MonitorEnumCtx
	{
		int target = 0;
		int index = 0;
		RECT rect{};
		bool found = false;
	};

	BOOL CALLBACK EnumMonProc(HMONITOR hMon, HDC, LPRECT, LPARAM lp)
	{
		auto* ctx = reinterpret_cast<MonitorEnumCtx*>(lp);
		if (ctx->index == ctx->target)
		{
			MONITORINFO mi{};
			mi.cbSize = sizeof(mi);
			GetMonitorInfo(hMon, &mi);
			ctx->rect = mi.rcMonitor;
			ctx->found = true;
			return FALSE; // stop
		}
		ctx->index++;
		return TRUE;
	}

	RECT GetMonitorRectByIndex(int monitorIndex)
	{
		struct Ctx { int target; int i; RECT rc; bool found; } ctx{ monitorIndex, 0, {}, false };

		auto cb = [](HMONITOR hMon, HDC, LPRECT, LPARAM lp)->BOOL
			{
				auto* c = reinterpret_cast<Ctx*>(lp);
				if (c->i == c->target)
				{
					MONITORINFO mi{}; mi.cbSize = sizeof(mi);
					GetMonitorInfo(hMon, &mi);
					c->rc = mi.rcMonitor;
					c->found = true;
					return FALSE;
				}
				c->i++;
				return TRUE;
			};

		EnumDisplayMonitors(nullptr, nullptr, cb, (LPARAM)&ctx);

		if (ctx.found) return ctx.rc;

		// fallback primary
		HMONITOR h = MonitorFromPoint(POINT{ 0,0 }, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO mi{}; mi.cbSize = sizeof(mi);
		GetMonitorInfo(h, &mi);
		return mi.rcMonitor;
	}
}

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
void WinApp::CreateMainWindow(const DisplaySettings& settings)
{
	// COM初期化など「既存のCreateMainWindow」と同じなら、そこは共通化してもOK
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hr)) { throw std::runtime_error("Failed to initialize COM library"); }

	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	WindowMode mode = settings.mode;
	if (mode == WindowMode::ExculusiveFullscreen) {
		// まずは排他は後回し：初期はボーダレスとして扱うのが安全
		mode = WindowMode::BorderlessFullscreen;
	}

	DWORD style = 0;
	DWORD exStyle = 0;
	RECT wrc{};

	if (mode == WindowMode::BorderlessFullscreen)
	{
		RECT mon = GetMonitorRectByIndex(settings.monitorIndex);
		style = WS_POPUP;
		exStyle = WS_EX_APPWINDOW;
		wrc = mon; // そのままウィンドウ矩形として使う
	}
	else // Windowed
	{
		style = WS_OVERLAPPEDWINDOW;
		exStyle = 0;
		wrc = { 0,0,(LONG)settings.width,(LONG)settings.height };
		AdjustWindowRectEx(&wrc, style, FALSE, exStyle);
	}

	const int winW = wrc.right - wrc.left;
	const int winH = wrc.bottom - wrc.top;

	int x = CW_USEDEFAULT;
	int y = CW_USEDEFAULT;

	if (mode == WindowMode::BorderlessFullscreen)
	{
		x = wrc.left;
		y = wrc.top;
	}

	hwnd = CreateWindowEx(
		exStyle,
		wc.lpszClassName,
		L"Ken4lowEngine",
		style,
		x, y,
		winW, winH,
		nullptr, nullptr,
		wc.hInstance,
		nullptr
	);

	ShowWindow(hwnd, (mode == WindowMode::Windowed && settings.maximize) ? SW_MAXIMIZE : SW_SHOW);

	// クライアントサイズ保存（DX初期化に使う）
	RECT crc{};
	GetClientRect(hwnd, &crc);
	clientWidth_ = (uint32_t)(crc.right - crc.left);
	clientHeight_ = (uint32_t)(crc.bottom - crc.top);
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void WinApp::Finalize()
{
	// ウィンドウが存在していれば破棄する
	if (hwnd)
	{
		DestroyWindow(hwnd); // ウィンドウの破棄
		hwnd = nullptr;   // ハンドルをクリア
	}

	// ウィンドウクラスの登録解除
	UnregisterClass(wc.lpszClassName, wc.hInstance);

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

bool WinApp::ConsumeResize(uint32_t& outWidth, uint32_t& outHeight)
{
	if (!resizePending_) return false;
	resizePending_ = false;
	outWidth = clientWidth_;
	outHeight = clientHeight_;
	return true;
}

void WinApp::RequestDisplaySettings(const DisplaySettings& settings)
{
	pendingDisplaySettings_ = settings;
	displaySettingsPending_ = true;
}

bool WinApp::ConsumeDisplaySettings(DisplaySettings& out)
{
	if (!displaySettingsPending_) return false;
	displaySettingsPending_ = false;
	out = pendingDisplaySettings_;
	return true;
}

bool WinApp::ApplyDisplaySettings(const DisplaySettings& settings)
{
	if (!hwnd) return false;

	// 今は排他は後回し：来たらボーダレス扱い
	WindowMode mode = settings.mode;
	if (mode == WindowMode::ExculusiveFullscreen)
	{
		mode = WindowMode::BorderlessFullscreen;
	}

	const RECT mon = GetMonitorRectByIndex(settings.monitorIndex);
	const int monW = mon.right - mon.left;
	const int monH = mon.bottom - mon.top;

	if (mode == WindowMode::BorderlessFullscreen)
	{
		// Windowed状態を保存（復元用）
		if (!hasSavedWindowed_)
		{
			GetWindowRect(hwnd, &savedWindowRect_);
			savedStyle_ = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
			savedExStyle_ = (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE);
			hasSavedWindowed_ = true;
		}

		DWORD style = WS_POPUP;
		DWORD exStyle = WS_EX_APPWINDOW;

		SetWindowLongPtr(hwnd, GWL_STYLE, style);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

		SetWindowPos(hwnd, nullptr,
			mon.left, mon.top, monW, monH,
			SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

		ShowWindow(hwnd, SW_SHOW);
	}
	else // Windowed
	{
		DWORD style = WS_OVERLAPPEDWINDOW;
		DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;

		SetWindowLongPtr(hwnd, GWL_STYLE, style);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

		// 指定クライアントサイズ → ウィンドウサイズに変換
		RECT rc{ 0,0, (LONG)settings.width, (LONG)settings.height };
		AdjustWindowRectEx(&rc, style, FALSE, exStyle);

		const int winW = rc.right - rc.left;
		const int winH = rc.bottom - rc.top;

		// モニター中央へ
		const int x = mon.left + (monW - winW) / 2;
		const int y = mon.top + (monH - winH) / 2;

		SetWindowPos(hwnd, nullptr, x, y, winW, winH,
			SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

		ShowWindow(hwnd, settings.maximize ? SW_MAXIMIZE : SW_SHOW);
	}

	// クライアントサイズ更新（WM_SIZEでも更新されるけど保険で）
	RECT crc{};
	GetClientRect(hwnd, &crc);
	clientWidth_ = (uint32_t)(crc.right - crc.left);
	clientHeight_ = (uint32_t)(crc.bottom - crc.top);
	resizePending_ = true;

	// 現在設定を更新
	currentDisplaySettings_ = settings;
	currentDisplaySettings_.mode = mode;
	if (mode == WindowMode::BorderlessFullscreen)
	{
		currentDisplaySettings_.width = clientWidth_;
		currentDisplaySettings_.height = clientHeight_;
	}

	return true;
}

void WinApp::RequestToggleFullscreen()
{
	toggleFullscreenPending_ = true;
}


bool WinApp::ConsumeToggleFullscreen()
{
	if (!toggleFullscreenPending_) return false;
	toggleFullscreenPending_ = false;
	return true;
}

void WinApp::RememberWindowedSettings(const DisplaySettings& s)
{
	lastWindowedSettings_ = s;
	hasLastWindowedSettings_ = true;
}

DisplaySettings WinApp::GetLastWindowedSettingsOrDefault() const
{
	if (hasLastWindowedSettings_) return lastWindowedSettings_;
	return DisplaySettings{};
}

void WinApp::DrawDisplaySettingsImGui()
{
#ifdef USE_IMGUI
	WinApp* win = WinApp::GetInstance();
	static DisplaySettings edit = win->GetCurrentDisplaySettings();

	if (ImGui::Begin("Display"))
	{
		// モード
		const char* modeItems[] = { "Windowed", "Borderless" };
		int mode = (edit.mode == WindowMode::Windowed) ? 0 : 1;
		if (ImGui::Combo("Mode", &mode, modeItems, IM_ARRAYSIZE(modeItems)))
		{
			edit.mode = (mode == 0) ? WindowMode::Windowed : WindowMode::BorderlessFullscreen;
		}

		// 解像度（Windowedのときだけ）
		if (edit.mode == WindowMode::Windowed)
		{
			const char* resItems[] = { "1280x720", "1920x1080" };
			int res = (edit.width == 1920 && edit.height == 1080) ? 1 : 0;
			if (ImGui::Combo("Resolution", &res, resItems, IM_ARRAYSIZE(resItems)))
			{
				if (res == 0) { edit.width = 1280; edit.height = 720; }
				else { edit.width = 1920; edit.height = 1080; }
			}

			ImGui::Checkbox("Maximize", &edit.maximize);
		}

		// 適用ボタン
		if (ImGui::Button("Apply"))
		{
			win->RequestDisplaySettings(edit);
		}

		// 現在のクライアントサイズ表示（デバッグ用）
		ImGui::Text("Client: %u x %u", win->GetClientWidth(), win->GetClientHeight());
	}
	ImGui::End();
#endif // USE_IMGUI

}

/// -------------------------------------------------------------
///					ウィンドウプロシージャ
/// -------------------------------------------------------------
LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}
#endif // USE_IMGUI

	auto* winApp = WinApp::GetInstance();

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

	case WM_ENTERSIZEMOVE:
		winApp->inSizeMove_ = true;
		return 0;

	case WM_EXITSIZEMOVE:
	{
		winApp->inSizeMove_ = false;
		// 最後に1回だけ確定リサイズさせる
		RECT rc{};
		GetClientRect(hwnd, &rc);
		winApp->clientWidth_ = (uint32_t)(rc.right - rc.left);
		winApp->clientHeight_ = (uint32_t)(rc.bottom - rc.top);
		winApp->resizePending_ = true;
		return 0;
	}
	case WM_SIZE:
	{
		if (wparam == SIZE_MINIMIZED) return 0;

		// ドラッグ中は毎フレームResizeBuffersすると重いので、確定時（EXITSIZEMOVE）にまとめるのが安定
		if (!winApp->inSizeMove_) {
			uint32_t w = LOWORD(lparam);
			uint32_t h = HIWORD(lparam);
			if (w > 0 && h > 0) {
				winApp->clientWidth_ = w;
				winApp->clientHeight_ = h;
				winApp->resizePending_ = true;
			}
		}
		return 0;
	}
	case WM_SYSKEYDOWN:
	{
		// Alt+Enter
		const bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
		const bool firstPress = (lparam & (static_cast<long long>(1) << 30)) == 0;

		if (wparam == VK_RETURN && altDown && firstPress)
		{
			WinApp::GetInstance()->RequestToggleFullscreen();
			return 0;
		}
		break;
	}

	// 「ﾋﾟﾛﾝ♪」を消す（Altキー系のシステム音回避）
	case WM_SYSCHAR:
	{
		const bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
		if (wparam == VK_RETURN && altDown) return 0;
		break;
	}

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}