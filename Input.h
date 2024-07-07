#pragma once

#include <Windows.h>

#define DIRECTINPUT_VERSION		0x0800	// DirectInputのバージョン指定
#include <dinput.h>


#include <wrl.h>

// 入力クラス
class Input
{
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public: // メンバ変数
	void Initialize(HINSTANCE hInstance, HWND hwnd);	// 初期化処理
	void Update();		// 更新処理

private:
	ComPtr<IDirectInput8> directInput;
	ComPtr<IDirectInputDevice8> keyboard;	// キーボードのデバイス
};

