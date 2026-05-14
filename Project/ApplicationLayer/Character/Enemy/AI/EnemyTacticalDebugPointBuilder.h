#pragma once
#include <EnemyTacticalDebugPoint.h>

#include <vector>

class EnemyTacticalDebugPointBuilder
{
public: /// ---------- ネストした構造体 ---------- ///

	struct Config
	{
		bool enabled = true;
		float pointRadius = 0.35f;
		float strafeDistance = 3.0f;
		float retreatDistance = 4.5f;
		float approachDistance = 3.0f;
	};

	struct Input
	{
		Ken4lowEngine::Vector3 enemyPosition{ 0.0f, 0.0f, 0.0f };
		Ken4lowEngine::Vector3 targetPosition{ 0.0f, 0.0f, 0.0f };
		float idealCombatRange = 15.0f;
		float minCombatRange = 8.0f;
		float maxCombatRange = 24.0f;
	};

public: /// ---------- 静的メンバ関数 ---------- ///

	[[nodiscard]] static std::vector<EnemyTacticalDebugPoint> Build(const Input& input, const Config& config);
};

