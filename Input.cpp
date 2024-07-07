#include "Input.h"
#include <cassert>
#include <cstring> // memcpyを使うために追加

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

void Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT result{};

	// DirectInputのインスタンス生成
	result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));
	// キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));
	// 入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(result));
	// 排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	// キー配列の初期化
	memset(key, 0, sizeof(key));
	memset(keyPre, 0, sizeof(keyPre));
}

void Input::Update()
{
	HRESULT result{};

	// 前回のキー入力を保存
	memcpy(keyPre, key, sizeof(key));

	// キーボード情報の取得開始
	result = keyboard->Acquire();
	if (FAILED(result))
	{
		// デバイスの取得が失敗した場合はここで処理を終了する
		return;
	}

	// 全キーの入力情報を取得する
	result = keyboard->GetDeviceState(sizeof(key), key);
	if (FAILED(result))
	{
		// 取得が失敗した場合はキー配列をリセット
		memset(key, 0, sizeof(key));
	}
}

bool Input::PushKey(BYTE keyNumber)
{
	return key[keyNumber] != 0;
}

bool Input::TriggerKey(BYTE keyNumber)
{
	return key[keyNumber] != 0 && keyPre[keyNumber] == 0;
}
