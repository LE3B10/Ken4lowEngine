#pragma once
#include <memory>
#include <vector>
#include <string>

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
	void Draw();
	void DrawImGui();

	void DrawShadow();

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	Player* GetPlayer() { return player_.get(); }
	const Player* GetPlayer() const { return player_.get(); }
	const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const { return enemies_; }

	// HPバーやロックオンみたいな「EnemyBase* 配列」が欲しい処理用
	std::vector<EnemyBase*> GetEnemyRawList() const;

	// 生成
	Enemy& SpawnEnemy(const EnemySpawnRequest& reqest);

	// 全消し
	void ClearEnemies();

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

private: /// ---------- デバッグ用 ---------- ///

	bool isDebug_ = false;

};