#pragma once
#include <cmath>
#include "Input.h"
#include "PlayerInputSnapshot.h"

static void NormalizeClamp2(float& x, float& z)
{
    const float lenSq = x * x + z * z;
    if (lenSq <= 1e-6f) { x = 0.0f; z = 0.0f; return; }
    const float len = std::sqrt(lenSq);
    x /= len; z /= len;
}

// 生成関数（毎フレーム呼ぶ）
inline InputSnapshot BuildInputSnapshot(Ken4lowEngine::Input& input)
{
    using namespace Ken4lowEngine;

    InputSnapshot s{};

    // ---- Move (WASD) ----
    if (input.PushKey(DIK_A)) s.moveX -= 1.0f;
    if (input.PushKey(DIK_D)) s.moveX += 1.0f;
    if (input.PushKey(DIK_W)) s.moveZ += 1.0f;
    if (input.PushKey(DIK_S)) s.moveZ -= 1.0f;

    // ---- Move (Pad Left Stick) ----
    if (input.IsConnect() && !input.LStickInDeadZone())
    {
        const auto ls = input.GetLeftStick(); // -1..+1
        s.moveX += ls.x;
        s.moveZ += ls.y;
    }

    // 斜めが速くならないように正規化（必要なら）
    if ((s.moveX * s.moveX + s.moveZ * s.moveZ) > 1.0f)
    {
        NormalizeClamp2(s.moveX, s.moveZ);
    }

    // ---- Look ----
    s.lookMouseX = static_cast<float>(input.GetMouseMoveX());
    s.lookMouseY = static_cast<float>(input.GetMouseMoveY());

    if (input.IsConnect() && !input.RStickInDeadZone())
    {
        const auto rs = input.GetRightStick();
        s.lookPadX = rs.x;
        s.lookPadY = rs.y;
    }

    // ---- Sprint ----
    s.sprintHeld = input.PushKey(DIK_LSHIFT);
    if (input.IsConnect())
    {
        s.sprintHeld = s.sprintHeld || input.PushButton(XButtons.L_Shoulder);
    }

    // ---- Jump / Dash ----
    s.jumpPressed = input.TriggerKey(DIK_SPACE);
    s.dashPressed = input.TriggerKey(DIK_LCONTROL);

    if (input.IsConnect())
    {
        s.jumpPressed = s.jumpPressed || input.TriggerButton(XButtons.A);
        s.dashPressed = s.dashPressed || input.TriggerButton(XButtons.B);
    }

    // ---- Aim (RMB or LT) ----
    s.aimHeld = input.PushMouse(1);
    s.aimPressed = input.TriggerMouse(1);

    if (input.IsConnect())
    {
        const bool ltHeld = (input.GetLeftTrigger() > 0.2f) || input.PushButton(XButtons.L_Trigger);
        s.aimHeld = s.aimHeld || ltHeld;
        s.aimPressed = s.aimPressed || input.TriggerButton(XButtons.L_Trigger);
    }

    // ---- Fire (LMB or RT) ----
    s.fireHeld = input.PushMouse(0);
    s.firePressed = input.TriggerMouse(0);

    if (input.IsConnect())
    {
        const bool rtHeld = (input.GetRightTrigger() > 0.2f) || input.PushButton(XButtons.R_Trigger);
        s.fireHeld = s.fireHeld || rtHeld;
        s.firePressed = s.firePressed || input.TriggerButton(XButtons.R_Trigger);
    }

    // ---- Reload / Melee ----
    s.reloadPressed = input.TriggerKey(DIK_R);
    s.meleePressed = input.TriggerKey(DIK_F);

    if (input.IsConnect())
    {
        s.reloadPressed = s.reloadPressed || input.TriggerButton(XButtons.X);
        s.meleePressed = s.meleePressed || input.TriggerButton(XButtons.R_Thumbstick);
    }

    // ---- Weapon Switch ----
    const int wheel = input.GetMouseWheel();
    if (wheel > 0) s.weaponSwitch = +1;
    else if (wheel < 0) s.weaponSwitch = -1;

    if (input.IsConnect())
    {
        if (input.TriggerButton(XButtons.DPad_Right)) s.weaponSwitch = +1;
        if (input.TriggerButton(XButtons.DPad_Left))  s.weaponSwitch = -1;
    }

    // ---- Pause ----
    s.pausePressed = input.TriggerKey(DIK_ESCAPE);
    if (input.IsConnect())
    {
        s.pausePressed = s.pausePressed || input.TriggerButton(XButtons.Start);
    }

    return s;
}