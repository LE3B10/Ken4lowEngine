#pragma once

/// ---------- 前方宣言 ---------- ///
class Enemy;

/// ---------- ノードの実行結果 ---------- ///
enum class BehaviorStatus
{
	Success, // 成功
	Failure, // 失敗
	Running, // 実行中
};

/// ---------------------------------------------
///		 ビヘイビアノードインターフェース
/// ---------------------------------------------
class IBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~IBehaviorNode() = default;

	// ノードの実行
	virtual BehaviorStatus Tick(Enemy& enemy, float deltaTime) = 0;
};

