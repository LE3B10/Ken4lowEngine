#include "Input.h"
#include "WinApp.h"
#include <cassert>
#include <cstring> // memcpyを使うために追加

#pragma comment(lib, "XInput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

XButtonIDs XButtons;

/// -------------------------------------------------------------
///				　XButtonIDsのコンストラクタ
/// -------------------------------------------------------------
XButtonIDs::XButtonIDs()
{
	// アクションボタン
	A = 0;
	B = 1;
	X = 2;
	Y = 3;

	// DPADのボタン
	DPad_Up = 4;
	DPad_Down = 5;
	DPad_Left = 6;
	DPad_Right = 7;

	// Shoulderボタン
	L_Shoulder = 8;
	R_Shoulder = 9;

	// Thumbstick
	L_Thumbstick = 10;
	R_Thumbstick = 11;

	Start = 12; // 'START' ボタン
	Back = 13;  // 'BACK' ボタン
}


/// -------------------------------------------------------------
///					　　シングルトンインスタンス
/// -------------------------------------------------------------
Input* Input::GetInstance()
{
	static Input instance;
	return &instance;
}


/// -------------------------------------------------------------
///					　　	初期化処理
/// -------------------------------------------------------------
void Input::Initialize(WinApp* winApp)
{
	// 借りてきたWinAppのインスタンスを記録
	winApp_ = winApp;

	HRESULT result{};

	// DirectInputのインスタンス生成
	result = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	// キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	// 入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	// キー配列の初期化
	memset(key, 0, sizeof(key));
	memset(keyPre, 0, sizeof(keyPre));

	// マウスデバイスの生成
	result = directInput->CreateDevice(GUID_SysMouse, mouseDevice_.GetAddressOf(), NULL);
	assert(SUCCEEDED(result));

	// マウスデバイスのフォーマット設定
	result = mouseDevice_->SetDataFormat(&c_dfDIMouse);
	assert(SUCCEEDED(result));

	// マウスデバイスの協調レベル設定
	result = mouseDevice_->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));

	// マウスの座標を取得
	POINT point;
	GetCursorPos(&point);
	ScreenToClient(winApp_->GetHwnd(), &point);
	mousePosition_.x = point.x;
	mousePosition_.y = point.y;

	// ゲームパッドの初期化
	for (int i = 0; i < GAMEPAD_BUTTON_NUM; i++)
	{
		prevButtonStates_[i] = false;
		buttonStates_[i] = false;
		buttonsTriger_[i] = false;
	}
}


/// -------------------------------------------------------------
///					　　	更新処理
/// -------------------------------------------------------------
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

	// マウスの情報の所得
	mouseDevice_->Acquire();
	mouseDevice_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);

	// マウスの座標を取得
	UpdateMousePosition();

	// ゲームパッドの状態を取得
	state_ = GetGamePadState();

	// ゲームパッドのボタンの状態を更新
	for (int i = 0; i < GAMEPAD_BUTTON_NUM; i++)
	{
		buttonStates_[i] = (state_.Gamepad.wButtons & XINPUT_Buttons[i]) == XINPUT_Buttons[i];
		buttonsTriger_[i] = !prevButtonStates_[i] && buttonStates_[i]; // トリガー判定
	}

	// **ここで前回の状態を更新**
	memcpy(prevButtonStates_, buttonStates_, sizeof(prevButtonStates_));
}


/// -------------------------------------------------------------
///					　　キーの押下処理
/// -------------------------------------------------------------
bool Input::PushKey(BYTE keyNumber) const
{
	return key[keyNumber] != 0;
}


/// -------------------------------------------------------------
///				　キーの押下処理（押した瞬間）
/// -------------------------------------------------------------
bool Input::TriggerKey(BYTE keyNumber) const
{
	return key[keyNumber] != 0 && keyPre[keyNumber] == 0;
}


/// -------------------------------------------------------------
///				　		マウスの座標を更新
/// -------------------------------------------------------------
void Input::UpdateMousePosition()
{
	// マウスの座標を取得
	POINT point;
	GetCursorPos(&point);
	ScreenToClient(winApp_->GetHwnd(), &point);
	mousePosition_.x = point.x;
	mousePosition_.y = point.y;
}


/// -------------------------------------------------------------
///				　	マウスの押下状態を取得
/// -------------------------------------------------------------
bool Input::PushMouse(int button) const
{
	if (mouseState_.rgbButtons[button])
	{
		return true;
	}

	return false;
}


/// -------------------------------------------------------------
///				　	マウスのトリガー状態を取得
/// -------------------------------------------------------------
bool Input::TriggerMouse(int button) const
{
	if (mouseState_.rgbButtons[button] && !prevMouseState_.rgbButtons[button])
	{
		return true;
	}

	return false;
}


/// -------------------------------------------------------------
///				　	マウスのリリース状態を取得
/// -------------------------------------------------------------
bool Input::ReleaseMouse(int button) const
{
	if (!mouseState_.rgbButtons[button] && prevMouseState_.rgbButtons[button])
	{
		return true;
	}

	return false;
}


/// -------------------------------------------------------------
///				　		マウスの座標を設定
/// -------------------------------------------------------------
void Input::SetMousePosition(int x, int y)
{
	POINT point;
	point.x = x;
	point.y = y;
	ClientToScreen(winApp_->GetHwnd(), &point);
	SetCursorPos(point.x, point.y);
}


/// -------------------------------------------------------------
///				　	マウスの座標を取得
/// -------------------------------------------------------------
Vector2 Input::GetMousePosition()
{
	return Vector2(float(mousePosition_.x), float(mousePosition_.y));
}


/// -------------------------------------------------------------
///				　ゲームパッドの状態を取得
/// -------------------------------------------------------------
XINPUT_STATE Input::GetGamePadState()
{
	XINPUT_STATE state;
	ZeroMemory(&state, sizeof(XINPUT_STATE));
	XInputGetState(0, &state); // ゲームパッドの状態を取得
	return state;
}


/// -------------------------------------------------------------
///				　ゲームパッドの状態を更新
/// -------------------------------------------------------------
void Input::UpdateGamePadState()
{
	memcpy(prevButtonStates_, buttonStates_, sizeof(prevButtonStates_));
}


/// -------------------------------------------------------------
///				　ゲームパッドの接続状態を取得
/// -------------------------------------------------------------
bool Input::IsConnect()
{
	ZeroMemory(&state_, sizeof(XINPUT_STATE));
	DWORD result = XInputGetState(0, &state_); // ゲームパッドの状態を取得
	return result == ERROR_SUCCESS;
}


/// -------------------------------------------------------------
///				ゲームパッドのリリース状態を取得
/// -------------------------------------------------------------
bool Input::ReleaseButton(int button) const
{
	if (!buttonStates_[button] && prevButtonStates_[button])
	{
		return true;
	}

	return false;
}


/// -------------------------------------------------------------
///				ゲームパッドの押下状態を取得
/// -------------------------------------------------------------
bool Input::PushButton(int button) const
{
	if (state_.Gamepad.wButtons & XINPUT_Buttons[button])
	{
		return true;
	}

	return false;
}


/// -------------------------------------------------------------
///				ゲームパッドのトリガー状態を取得
/// -------------------------------------------------------------
bool Input::TriggerButton(int button) const
{
	return buttonsTriger_[button];
}


/// -------------------------------------------------------------
///					左スティックのデッドゾーン
/// -------------------------------------------------------------
bool Input::LStickInDeadZone() const
{
	// 左スティックの値を取得
	short x = state_.Gamepad.sThumbLX;
	short y = state_.Gamepad.sThumbLY;

	// デッドゾーンの設定
	if (x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	{
		return false;
	}

	if (y > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || y < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	{
		return false;
	}

	return true;
}


/// -------------------------------------------------------------
///					右スティックのデッドゾーン
/// -------------------------------------------------------------
bool Input::RStickInDeadZone() const
{
	// 右スティックの値を取得
	short x = state_.Gamepad.sThumbRX;
	short y = state_.Gamepad.sThumbRY;

	// デッドゾーンの設定
	if (x > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || x < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
	{
		return false;
	}

	if (y > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || y < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
	{
		return false;
	}

	return true;
}


/// -------------------------------------------------------------
///					左スティックの値を取得
/// -------------------------------------------------------------
Vector2 Input::GetLeftStick()
{
	// 左スティックの値を取得
	short x = state_.Gamepad.sThumbLX;
	short y = state_.Gamepad.sThumbLY;

	return Vector2(static_cast<float>(x) / 32768.0f, static_cast<float>(y) / 32768.0f);
}


/// -------------------------------------------------------------
///					右スティックの値を取得
/// -------------------------------------------------------------
Vector2 Input::GetRightStick()
{
	// 右スティックの値を取得
	short x = state_.Gamepad.sThumbRX;
	short y = state_.Gamepad.sThumbRY;

	return Vector2(static_cast<float>(x) / 32768.0f, static_cast<float>(y) / 32768.0f);
}


/// -------------------------------------------------------------
///					左トリガーの値を取得
/// -------------------------------------------------------------
float Input::GetLeftTrigger()
{
	// 左トリガーの値を取得
	BYTE trigger = state_.Gamepad.bLeftTrigger;

	if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
	{
		return static_cast<float>(trigger) / 255.0f;
	}

	return 0.0f;
}


/// -------------------------------------------------------------
///					右トリガーの値を取得
/// -------------------------------------------------------------
float Input::GetRightTrigger()
{
	// 左トリガーの値を取得
	BYTE trigger = state_.Gamepad.bRightTrigger;

	if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
	{
		return static_cast<float>(trigger) / 255.0f;
	}

	return 0.0f;
}


/// -------------------------------------------------------------
///						ゲームパッドの振動
/// -------------------------------------------------------------
void Input::SetVibration(float leftMotor, float rightMotor)
{
	// モーターの振動設定
	XINPUT_VIBRATION vibration;
	ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

	vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
	vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);

	XInputSetState(0, &vibration);
}


/// -------------------------------------------------------------
///					ゲームパッドの振動を停止
/// -------------------------------------------------------------
void Input::StopVibration()
{
	// モーターの振動の停止
	XINPUT_VIBRATION vibration;
	ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

	vibration.wLeftMotorSpeed = 0;
	vibration.wRightMotorSpeed = 0;

	XInputSetState(0, &vibration);
}
