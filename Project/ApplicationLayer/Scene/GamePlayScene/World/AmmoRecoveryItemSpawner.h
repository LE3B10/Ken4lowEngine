#pragma once

#include "AABB.h"
#include "Vector3.h"

#include <cstddef>
#include <vector>

class ItemManager;
class Player;
namespace Ken4lowEngine
{
	class Stage;
}

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// 弾薬回復アイテムの時間スポーンを管理するクラス。
///
/// GamePlayWorldからタイマーや出現条件を切り離し、既存ItemManagerの
/// AmmoSmall生成・取得・Collider削除処理をそのまま利用する。
/// -------------------------------------------------------------
class AmmoRecoveryItemSpawner
{
public:
	/// <summary>
	/// スポーン条件と回復量を調整しやすくまとめた設定値。
	/// recoveryAmountOverride が0以下の場合は現在武器のマガジン容量を使う。
	/// </summary>
	struct Settings
	{
		float spawnIntervalSec = 15.0f;
		int maxActiveCount = 3;
		int recoveryAmountOverride = 0;
		float minDistanceFromPlayer = 12.0f;
		float spawnHeightOffset = 0.15f;
	};

	/// <summary>
	/// ステージ床情報を参照し、スポーン候補の巡回状態を初期化する。
	/// </summary>
	void Initialize();

	/// <summary>
	/// 弾薬不足かつ進行演出中でない場合だけタイマーを進め、AmmoSmallを生成する。
	/// </summary>
	void Update(float deltaTime, Player* player, ItemManager& itemManager, const K4E::Stage* stage, bool suppressNewSpawn);

	/// <summary>
	/// デバッグ用にスポーン間隔・最大数・回復量などを調整する。
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// タイマーと候補巡回位置を初期状態に戻す。
	/// </summary>
	void Reset();

private:
	bool ShouldSpawnForPlayer(const Player& player) const;
	int ResolveRecoveryAmount(const Player& player) const;
	bool TryFindSpawnPosition(const Player& player, const K4E::Stage* stage, K4E::Vector3& outPosition);
	bool TryFindFloorSpawnPosition(const K4E::Vector3& playerPosition, const std::vector<K4E::AABB>& floorAABBs, K4E::Vector3& outPosition);
	K4E::Vector3 MakeFallbackSpawnPosition(const K4E::Vector3& playerPosition) const;

private:
	Settings settings_{};
	float spawnTimerSec_ = 0.0f;
	std::size_t nextFloorIndex_ = 0;
	K4E::Vector3 lastSpawnPosition_{};
	bool lastSpawnSucceeded_ = false;
	bool lastSuppressed_ = false;
};
