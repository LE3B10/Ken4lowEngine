#pragma once
#include "BehaviorStatus.h"

/// -------------------------------------------------------------
///			ビヘイビアツリーのノードインターフェース
/// -------------------------------------------------------------
class IBTNode
{
public:

	virtual ~IBTNode() = default;

	/// ノードを1回評価し、成功・失敗・実行中のいずれかを返す。
	virtual BehaviorStatus Tick(float deltaTime) = 0;
};
