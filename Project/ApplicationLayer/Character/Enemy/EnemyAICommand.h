#pragma once
#include <optional>
#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

struct EnemyAICommand
{
    // 移動目標（A*導入後もここは変えない）
    std::optional<K4E::Vector3> moveGoal;

    // 向きたい方向
    std::optional<K4E::Vector3> lookAt;

    // 射撃したい対象
    std::optional<K4E::Vector3> fireAt;

    // そのフレームは移動を止めたい
    bool stopMove = false;

    // デバッグ用：どの状態が命令を出したか
    int debugState = -1;

    void Clear() { *this = EnemyAICommand{}; }
};