#pragma once

#include <Windows.h>
#include <Xinput.h>					// ゲームコントローラーAPI
#define DIRECTINPUT_VERSION		0x0800	// DirectInputのバージョン指定
#include <dinput.h>
#include <wrl.h>
#include "WinApp.h"

// 入力クラス
class Input
{
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public: /// ---------- メンバ関数 ---------- ///

	// シングルトン
	static Input* GetInstance();

	// 初期化処理
	void Initialize(WinApp* winApp);

	// 更新処理
	void Update();

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

	// マウス入力
	bool IsMouseButtonPressed(int button) const;
	DIMOUSESTATE GetMouseState() const;

private: // メンバ変数
	// WindowsAPI
	WinApp* winApp_ = nullptr;

	ComPtr<IDirectInput8> directInput;		// DirectInputのインスタンス
	ComPtr<IDirectInputDevice8> keyboard;	// キーボードのデバイス
	ComPtr<IDirectInputDevice8> mouse;		// マウスデバイス

	BYTE key[256] = {};
	BYTE keyPre[256] = {};
	DIMOUSESTATE mouseState = {};

	Input() = default;
	~Input() = default;

	// コピー禁止
	Input(const Input&) = delete;
	const Input& operator=(const Input&) = delete;
};

