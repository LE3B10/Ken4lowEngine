#pragma once

#include <Windows.h>

#include <Xinput.h>					// ゲームコントローラーAPI
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

	/// <summary>
	/// キーの押下をチェック
	/// </summary>
	/// <param name="keyNumber">キー番号（ DIK_0 等 ）</param>
	/// <returns></returns>
	bool PushKey(BYTE keyNumber) const;

	/// <summary>
	/// キーのトリガーをチェック（押した瞬間）
	/// </summary>
	/// <param name="keyNumber">キー番号（ DIK_0 等 ）</param>
	/// <returns></returns>
	bool TriggerKey(BYTE keyNumber) const;

private:
	ComPtr<IDirectInput8> directInput;		// DirectInputのインスタンス
	ComPtr<IDirectInputDevice8> keyboard;	// キーボードのデバイス

	BYTE key[256] = {};
	BYTE keyPre[256] = {};
};

