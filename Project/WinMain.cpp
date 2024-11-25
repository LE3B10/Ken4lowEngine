#include "GameEngine.h"

// 解放し忘れがないか確認するリークチェッカー
D3DResourceLeakChecker resourceLeakCheck;

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	std::unique_ptr gameEngine = std::make_unique<GameEngine>();

	// 初期化処理
	gameEngine->Initialize(kClientWidth, kClientHeight);

	// 更新処理
	gameEngine->Update();

	// 終了処理
	gameEngine->Finalize();

	return 0;
}