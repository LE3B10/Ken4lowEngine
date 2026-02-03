#include "GameApplication.h"
#include "D3DResourceLeakChecker.h"

using namespace Ken4lowEngine;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#ifdef _DEBUG
	// 解放し忘れがないか確認するリークチェッカー
	D3DResourceLeakChecker resourceLeakCheck;
#endif // _DEBUG

	// Frameworkの派生クラスであるGameEngineを使用
	std::unique_ptr<Framework> game = std::make_unique<GameApplication>();

	// 実行処理
	game->Run();

	return 0;
}