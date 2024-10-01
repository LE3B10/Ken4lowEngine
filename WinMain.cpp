#include <Windows.h>
#include "WinApp.h"
#include "Input.h"
#include "LogString.h"
#include "DirectXCommon.h"

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// WindowsAPIの初期化
	WinApp* winApp = new WinApp();
	winApp->Initialize();

	// 入力の初期化
	Input* input = new Input();
	input->Initialize(winApp);

	// DirectXの初期化
	DirectXCommon* dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	while (!winApp->ProcessMessage())
	{
		// 入力の更新
		input->Update();

		if (input->TriggerKey(DIK_0))
		{
			OutputDebugStringA("Hit 0 \n");
		}

		// 描画前処理
		dxCommon->PreDraw();

		// 描画後処理
		dxCommon->PostDraw();
	}

	winApp->Finalize();

	delete winApp;	 // WindowsAPI解放
	delete input;	 // 入力の解放
	delete dxCommon; // DirectXの解放
}