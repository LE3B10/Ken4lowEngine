#pragma once
#include "Enemy.h"

#include <memory>
#include <vector>

class Player;

class EnemySpawner
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

	// 次のウェーブを開始
	void StartNextWave();

	// ウェーブクリア判定
	bool IsWaveClear() const { return (spawnedEnemies_ >= totalEnemiesThisWave_) && enemies_.empty(); }

	// 現在のウェーブ数を取得
	int GetCurrentWave() const { return currentWave_; }

	// スポーンしたエネミーの数を取得
	const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const { return enemies_; }

	void SetTargetPlayer(Player* player) { player_ = player; }

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // プレイヤーへのポインタ

	int currentWave_ = 1;
	int totalEnemiesThisWave_ = 0;
	int spawnedEnemies_ = 0;
	float spawnInterval_ = 1.0f;
	float spawnTimer_ = 0.0f;

	std::vector<std::unique_ptr<Enemy>> enemies_;
};
