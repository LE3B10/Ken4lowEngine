#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

#include "Player.h"
#include "Enemy.h"
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

	void DrawShadow();

	// 大量敵の負荷原因を切り分けるため、更新・描画・デバッグ描画・影を個別に切り替える。
	void SetEnemyUpdateEnabled(bool enabled) { enableEnemyUpdate_ = enabled; }
	void SetEnemyDrawEnabled(bool enabled) { enableEnemyDraw_ = enabled; }
	void SetEnemyDebugDrawEnabled(bool enabled) { enableEnemyDebugDraw_ = enabled; }
	void SetEnemyShadowEnabled(bool enabled) { enableEnemyShadow_ = enabled; }
	bool IsEnemyUpdateEnabled() const { return enableEnemyUpdate_; }
	bool IsEnemyDrawEnabled() const { return enableEnemyDraw_; }
	bool IsEnemyDebugDrawEnabled() const { return enableEnemyDebugDraw_; }
	bool IsEnemyShadowEnabled() const { return enableEnemyShadow_; }
	int GetMeleeEnemyCount() const { return 0; }
	int GetMidRangeEnemyCount() const { return static_cast<int>(enemies_.size()); }
	int GetLastEnemyUpdateCount() const { return lastEnemyUpdateCount_; }
	int GetLastEnemyDrawCount() const { return lastEnemyDrawCount_; }
	int GetLastEnemyDebugDrawCount() const { return lastEnemyDebugDrawCount_; }

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	Player* GetPlayer() { return player_.get(); }
	const Player* GetPlayer() const { return player_.get(); }
	const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const { return enemies_; }

	// HPバーやロックオンみたいな「EnemyBase* 配列」が欲しい処理用
	std::vector<EnemyBase*> GetEnemyRawList() const;

	// 生成
	Enemy& SpawnEnemy(const EnemySpawnRequest& request);
	Enemy& SpawnEnemyAt(const K4E::Vector3& position);

	// 全消し
	void ClearEnemies();
	void SetEnemyKilledCallback(std::function<void(const K4E::Vector3&)> callback) { onEnemyKilled_ = std::move(callback); }

	int GetEnemyCount() const { return static_cast<int>(enemies_.size()); }

public: /// ---------- デバッグ用 ---------- ///

	void SetDebug(bool on) { isDebug_ = on; }
	bool IsDebug() const { return isDebug_; }

private: /// ---------- 内部処理 ---------- ///

	void InjectPlayerDeps(Player& p);
	void InjectEnemyDeps(Enemy& e);

private: /// ---------- メンバ変数 ---------- ///

	GameContext ctx_{}; // ポインタ保持しない（Scene側ローカルctxの寿命問題を避ける）

	std::unique_ptr<Player> player_;
	std::vector<std::unique_ptr<Enemy>> enemies_;

	// 敵の被弾エフェクトシステム
	EnemyParticleEffectSystem enemyParticleEffectSystem_;
	std::function<void(const K4E::Vector3&)> onEnemyKilled_{};
	std::unordered_set<const Enemy*> notifiedKilledEnemies_;

private: /// ---------- デバッグ用 ---------- ///

	bool isDebug_ = false;
	bool enableEnemyUpdate_ = true;
	bool enableEnemyDraw_ = true;
	bool enableEnemyDebugDraw_ = true;
	bool enableEnemyShadow_ = true;
	int lastEnemyUpdateCount_ = 0;
	int lastEnemyDrawCount_ = 0;
	int lastEnemyDebugDrawCount_ = 0;

};
