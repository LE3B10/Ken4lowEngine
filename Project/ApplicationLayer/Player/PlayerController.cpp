#include "PlayerController.h"
#include <Input.h>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void PlayerController::Initialize()
{
	input_ = Input::GetInstance();
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void PlayerController::Update()
{
	move_ = { 0.0f, 0.0f, 0.0f };
	jump_ = false;

	// --- キーボード入力 ---
	if (input_->PushKey(DIK_W)) move_.z += 2.0f;
	if (input_->PushKey(DIK_S)) move_.z -= 2.0f;
	if (input_->PushKey(DIK_A)) move_.x -= 2.0f;
	if (input_->PushKey(DIK_D)) move_.x += 2.0f;
	if (input_->TriggerKey(DIK_SPACE)) jump_ = true;

	// --- ゲームパッド入力（左スティック） ---
	if (!input_->LStickInDeadZone())
	{
		auto stick = input_->GetLeftStick();
		move_.x += stick.x;
		move_.z += stick.y;
	}

	// --- ゲームパッドのAボタンでジャンプ ---
	if (input_->TriggerButton(XButtons.A)) jump_ = true;

	// 正規化してスピードを一定に
	if (Vector3::Length(move_) > 1.0f) move_ = Vector3::Normalize(move_);
}
