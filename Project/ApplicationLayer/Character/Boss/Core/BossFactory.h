#pragma once
#include "BossBase.h"

#include <memory>

/// -------------------------------------------------------------
///					　ボスファクトリー
/// -------------------------------------------------------------
class BossFactory
{
public: /// ---------- 列挙型 ---------- ///

	/// ---------- ボスの種類 ---------- ///
    enum class BossType
    {
        ForestGuardian,
        FlameBeast,
        SandWorm,
        MachineCore,
        IceQueen
    };

public: /// ---------- メンバ関数 ---------- ///

	// ボスタイプに応じた BossBase 派生クラスのインスタンスを生成して返す
    static std::unique_ptr<BossBase> Create(BossType type);
};
