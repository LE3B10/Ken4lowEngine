#include "WinApp.h"
#include "Input.h"

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// ポインタ
	WinApp* winApp = WinApp::GetInstance();
	Input* input = Input::GetInstance();

	// WindowsAPIの初期化
	winApp->CreateMainWindow();

	// 入力の初期化
	input->Initialize(winApp);

	while (!winApp->ProcessMessage())
	{
		// 入力の更新
		input->Update();

		if (input->TriggerKey(DIK_0))
		{
			OutputDebugStringA("Hit 0 \n");
		}
	}

	winApp->Finalize();

	return 0;
}
