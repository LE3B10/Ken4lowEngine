#pragma once
#include <BehaviorStatus.h>

/// -------------------------------------------------------------
///			ビヘイビアツリーのノードインターフェース
/// -------------------------------------------------------------
class IBTNode
{
public: /// ---------- 基本構造 ---------- ///

	// デストラクタは仮想関数にしておく
    virtual ~IBTNode() = default;

	// ビヘイビアツリーの更新
    virtual BehaviorStatus Tick(float deltaTime) = 0;
};