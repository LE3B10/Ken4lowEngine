#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "WinApp.h"

// DirectXの基盤
class DirectXCommon
{
public: // メンバ関数
	// 初期化処理
	void Initialize(WinApp* winApp);
	// 更新処理
	void Update();

private: // メンバ変数
	// WindowsAPI
	WinApp* winApp = nullptr;
};
