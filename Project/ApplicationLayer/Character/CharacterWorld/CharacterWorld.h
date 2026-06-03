#pragma once
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

#include "Player.h"
#include "EnemyBase.h"
#include "EnemyType.h"
#include "EnemyParticleEffectSystem.h"

// 前方宣言
class CollisionManager;
class BulletManager;

namespace K4E = ::Ken4lowEngine;

struct GameContext
{
	CollisionManager* collisionManager_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
};

/// -------------------------------------------------------------
/// キャラクター（Player/Enemy）だけを保持・生成・更新するワールド
/// -------------------------------------------------------------
class CharacterWorld
{
private: /// ---------- 構造体 ---------- ///

	struct EnemySpawnRequest
	{
		K4E::Vector3 position = {};
		float yawRad = 0.0f;
		float maxHp = 240.0f;
		EnemyType enemyType = EnemyType::Legacy;
	};

public: /// ---------- メンバ関数 ---------- ///

	void Initialize(GameContext& ctx);
	void Finalize();

	void Update(float dt);
	void UpdatePlayerOnly(float dt);
	void WarmupStartGameplayVisuals();
	void SetStartGameplayVisualsVisible(bool visible);
	void Draw();
	void DrawImGui();
	void DrawPlayerDebugImGui();
	void DrawEnemyDebugImGui();
	void DrawEnemyTuningImGui();
	void DrawMeleeEnemyTuningImGuiContent();
	void DrawMidRangeEnemyTuningImGuiContent();

	void DrawShadow();

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	Player* GetPlayer() { return player_.get(); }
	const Player* GetPlayer() const { return player_.get(); }
	const std::vector<std::unique_ptr<EnemyBase>>& GetEnemies() const { return enemies_; }

	// HPバーやロックオンみたいな「EnemyBase* 配列」が欲しい処理用
	std::vector<EnemyBase*> GetEnemyRawList() const;

	// 生成
	EnemyBase& SpawnEnemy(const EnemySpawnRequest& request);
	EnemyBase& SpawnEnemyAt(const K4E::Vector3& position, EnemyType enemyType = EnemyType::Legacy);

	// 全消し
	void ClearEnemies();
	void SetEnemyKilledCallback(std::function<void(const K4E::Vector3&)> callback) { onEnemyKilled_ = std::move(callback); }

	int GetEnemyCount() const { return static_cast<int>(enemies_.size()); }
	int GetAliveNormalEnemyCount() const;

public: /// ---------- デバッグ用 ---------- ///

	void SetDebug(bool on) { isDebug_ = on; }
	bool IsDebug() const { return isDebug_; }

private: /// ---------- 内部処理 ---------- ///

	void InjectPlayerDeps(Player& p);
	void InjectEnemyDeps(EnemyBase& e);

private: /// ---------- メンバ変数 ---------- ///

	GameContext ctx_{}; // ポインタ保持しない（Scene側ローカルctxの寿命問題を避ける）

	std::unique_ptr<Player> player_;
	std::vector<std::unique_ptr<EnemyBase>> enemies_;

	// 敵の被弾エフェクトシステム
	EnemyParticleEffectSystem enemyParticleEffectSystem_;
	std::function<void(const K4E::Vector3&)> onEnemyKilled_{};
	std::unordered_set<const EnemyBase*> notifiedKilledEnemies_;
	std::array<int, 3> spawnedEnemyCounts_{}; // 通常ゲーム中に生成した敵種別ごとの累計

private: /// ---------- デバッグ用 ---------- ///

	bool isDebug_ = false;
	EnemyType debugSpawnEnemyType_ = EnemyType::Legacy;

};
