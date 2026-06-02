#pragma once

#include "EnemySpawnCrystal.h"

#include <cstddef>
#include <vector>

class CharacterWorld;

/// 複数クリスタルの進行、雑魚敵生成要求、ボス通知を集約する。
class CrystalManager
{
public:
	void Initialize(const std::vector<CrystalSpawnPoint>& spawnPoints);
	void Update(CharacterWorld& characters, float deltaTime);
	void Draw() const;
	void DrawImGui();

	int GetCrystalCount() const { return static_cast<int>(crystals_.size()); }
	int GetAliveCrystalCount() const;
	bool AreAllCrystalsDestroyed() const;
	bool ShouldSpawnBoss() const { return shouldSpawnBoss_; }
	bool HasBossSpawned() const { return hasBossSpawned_; }

private:
	EnemySpawnCrystal* GetSelectedCrystal();
	const EnemySpawnCrystal* GetSelectedCrystal() const;
	void NotifyBossSpawnIfNeeded();

private:
	std::vector<EnemySpawnCrystal> crystals_;
	size_t selectedCrystalIndex_ = 0;
	bool shouldSpawnBoss_ = false;
	bool hasBossSpawned_ = false;
};
