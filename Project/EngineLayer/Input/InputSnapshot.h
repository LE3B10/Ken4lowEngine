#pragma once
#include <cmath>
#include "Input.h"
#include "PlayerInputSnapshot.h"

static void NormalizeClamp2(float& x, float& z)
{
	const float lenSq = x * x + z * z;
	if (lenSq <= 1e-6f)
	{
		x = 0.0f;
		z = 0.0f;
		return;
	}

	const float len = std::sqrt(lenSq);
	x /= len;
	z /= len;
}

// 毎フレーム呼ぶ入力スナップショット生成
static InputSnapshot BuildInputSnapshot(K4E::Input& input)
{
	InputSnapshot snap{};

	//// --------------------------------------------------
	// 移動
	// --------------------------------------------------
	if (input.PushKey(DIK_A)) { snap.moveX -= 1.0f; }
	if (input.PushKey(DIK_D)) { snap.moveX += 1.0f; }
	if (input.PushKey(DIK_S)) { snap.moveZ -= 1.0f; }
	if (input.PushKey(DIK_W)) { snap.moveZ += 1.0f; }

	// 斜め移動で速度が不自然に速くならないよう正規化
	NormalizeClamp2(snap.moveX, snap.moveZ);

	// --------------------------------------------------
	// 視点
	// --------------------------------------------------
	snap.lookMouseX = static_cast<float>(input.GetMouseMoveX());
	snap.lookMouseY = static_cast<float>(input.GetMouseMoveY());

	// --------------------------------------------------
	// アクション
	// --------------------------------------------------
	snap.sprintHeld = input.PushKey(DIK_LSHIFT);
	snap.jumpHeld = input.PushKey(DIK_SPACE);
	snap.jumpPressed = input.TriggerKey(DIK_SPACE);
	snap.blinkPressed = input.TriggerKey(DIK_LCONTROL);

	snap.aimHeld = input.PushMouse(1);   // 右クリック
	snap.aimPressed = input.TriggerMouse(1);

	snap.fireHeld = input.PushMouse(0);   // 左クリック
	snap.firePressed = input.TriggerMouse(0);

	// リロードは R のみ
	snap.reloadPressed = input.TriggerKey(DIK_R);

	// 近接
	snap.meleePressed = input.TriggerKey(DIK_F);

	// --------------------------------------------------
	// 武器切替
	// --------------------------------------------------
	// 0 = 切替なし
	// +1 = 次の武器
	// -1 = 前の武器
	snap.weaponSwitch = 0;

	// マウスホイール差分を取得
	// 実装によっては ±120 などで返るので、符号だけ見て 1 ステップ入力へ変換する
	const int wheelDelta = input.GetMouseWheel();

	if (wheelDelta > 0)
	{
		snap.weaponSwitch = +1;
	}
	else if (wheelDelta < 0)
	{
		snap.weaponSwitch = -1;
	}

	// --------------------------------------------------
	// 数字キー武器スロット
	// --------------------------------------------------
	snap.weaponSlotPressed = 0;

	if (input.TriggerKey(DIK_1)) { snap.weaponSlotPressed = 1; }
	else if (input.TriggerKey(DIK_2)) { snap.weaponSlotPressed = 2; }
	else if (input.TriggerKey(DIK_3)) { snap.weaponSlotPressed = 3; }
	else if (input.TriggerKey(DIK_4)) { snap.weaponSlotPressed = 4; }
	else if (input.TriggerKey(DIK_5)) { snap.weaponSlotPressed = 5; }
	else if (input.TriggerKey(DIK_6)) { snap.weaponSlotPressed = 6; }

	// --------------------------------------------------
	// その他
	// --------------------------------------------------
	snap.toggleFireModePressed = input.TriggerKey(DIK_V);
	snap.pausePressed = input.TriggerKey(DIK_ESCAPE);

	return snap;
}