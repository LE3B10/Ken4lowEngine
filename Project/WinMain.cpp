#include "GameEngine.h"

// 解放し忘れがないか確認するリークチェッカー
D3DResourceLeakChecker resourceLeakCheck;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// Frameworkの派生クラスであるGameEngineを使用
	std::unique_ptr<Framework> game = std::make_unique<GameEngine>();

	// 実行処理
	game->Run();

	return 0;
}