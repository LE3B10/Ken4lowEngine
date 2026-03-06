#pragma once
#include <optional>
#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// EnemyAICommand
///  - FSMが「やりたい事」だけを書き、Enemyが実行する
///  - moveGoal : 目標地点へ向かう（ナビメッシュ導入しても維持）
///  - moveDir  : そのフレームの移動方向（ストレイフ/後退などに便利）
/// -------------------------------------------------------------
struct EnemyAICommand
{
	// 移動目標（A*導入後もここは変えない）
	std::optional<K4E::Vector3> moveGoal;

	// そのフレームの移動方向（XZ想定）
	std::optional<K4E::Vector3> moveDir;

	// moveDir 時の速度上書き
	std::optional<float> moveSpeed;

	// 向きたい方向
	std::optional<K4E::Vector3> lookAt;

	// 射撃したい対象
	std::optional<K4E::Vector3> fireAt;

	// そのフレームは移動を止めたい
	bool stopMove = false;

	// リロードしたい（演出/行動用）
	bool wantReload = false;

	// デバッグ用：どの状態が命令を出したか
	int debugState = -1;

	void Clear() { *this = EnemyAICommand{}; }
};