#pragma once
#include "EnemyTacticalDebugPoint.h"

#include <vector>

// 候補点をワイヤーフレームSphereと選択ラインで描画するデバッグ専用
class EnemyTacticalPointDebugDrawer
{
public: /// ---------- 静的メンバ関数 ---------- ///

	// 候補点をワイヤーフレームSphereと選択ラインで描画します。
	static void Draw(const std::vector<EnemyTacticalDebugPoint>& points, const Ken4lowEngine::Vector3& enemyPosition, float sphereRadius);
};