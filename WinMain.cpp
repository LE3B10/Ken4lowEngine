#include "WinApp.h"
#include "DirectXCommon.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// ポインタ置き場
	WinApp* winApp = nullptr;
	DirectXCommon* dxCommon = nullptr;

	// ゲームウィンドウの作成
	winApp = new WinApp();
	winApp->Initialize();

	// DirectXの初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	// 汎用機能初期化

	// シーンの初期化

	while (!winApp->ProcessMessage()) // ゲームループ
	{
		// 描画前処理
		dxCommon->PreDraw();

		// 描画後処理
		dxCommon->PostDraw();
	}

	// WindowsAPIの終了処理
	winApp->Finalize();

	// 解放処理
	delete winApp;
	delete dxCommon;

	return 0;
}