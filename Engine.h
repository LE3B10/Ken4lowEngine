#pragma once
#include <Windows.h>

#include <cstdint>

// 前方宣言
class WinApp;

///----- メインシステムの初期化処理 -----///
class Engine
{
public:
	///----- メンバ関数 -----///

	// 各システムの初期化処理
	static void Initialize(uint32_t width, uint32_t height);

	// メッセージの受け渡し
	static bool ProcessMessage();

	// システムの終了処理
	static void Finalize();

private:
	///----- 静的メンバ変数 -----///
	static WinApp* winApp_;


};

