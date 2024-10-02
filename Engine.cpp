#include "Engine.h"

#include "WinApp.h"


///----- 静的メンバ -----///
WinApp* Engine::winApp_ = nullptr;


void Engine::Initialize(uint32_t width, uint32_t height)
{
	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	winApp_->GetHInstance();


	//----- メインウィンドウ作成 -----///
	winApp_->CreateMainWindow(width, height);
}

bool Engine::ProcessMessage()
{
	if (winApp_->ProcessMessage())
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void Engine::Finalize()
{
	CloseWindow(winApp_->GetHwnd());
	//COMの終了処理
	CoUninitialize();
}
