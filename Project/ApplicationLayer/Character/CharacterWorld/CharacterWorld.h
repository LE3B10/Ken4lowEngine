#pragma once
#include <memory>
#include <vector>
#include <string>

#include "Player.h"
#include "Enemy.h"
#include "EnemyArchetype.h"
#include "EnemyParticleEffectSystem.h"
#include "EnemyTuningEditor.h"

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
public:
	void Initialize(GameContext& ctx);
	void Finalize();

	void Update(float dt);
	void Draw();
	void DrawImGui();

	void DrawShadow();

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	Player* GetPlayer() { return player_.get(); }
	const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const { return enemies_; }

	// 生成
	Enemy& SpawnEnemy(const K4E::Vector3& pos);
	Enemy& SpawnEnemy(EnemyArchetype type, const K4E::Vector3& pos);

	// 全消し
	void ClearEnemies();

	int GetEnemyCount() const { return static_cast<int>(enemies_.size()); }

public: /// ---------- デバッグ用 ---------- ///

	void SetDebug(bool on) { isDebug_ = on; }
	bool IsDebug() const { return isDebug_; }

private: /// ---------- 内部処理 ---------- ///

	void InjectPlayerDeps(Player& p);
	void InjectEnemyDeps(Enemy& e);

	// --------------------------------------------------------
	// Repositoryの内容を全Enemyへ再反映する
	// - Save / Delete / Reload 後に呼ぶ
	// - 各Enemyは自分の archetype を持っているので、
	//   それを使って SetArchetype() し直せば最新値になる
	// --------------------------------------------------------
	void ReapplyEnemyTunings();

private: /// ---------- メンバ変数 ---------- ///

	GameContext ctx_{}; // ポインタ保持しない（Scene側ローカルctxの寿命問題を避ける）

	std::unique_ptr<Player> player_;
	std::vector<std::unique_ptr<Enemy>> enemies_;

	// 敵の被弾エフェクトシステム
	EnemyParticleEffectSystem enemyParticleEffectSystem_;

	// --------------------------------------------------------
	// EnemyTuningEditor は CharacterWorld に 1個だけ持たせる
	// - 敵個体ではなく、敵種別データを編集するためのUI
	// --------------------------------------------------------
	EnemyTuningEditor tuningEditor_{};

private: /// ---------- デバッグ用 ---------- ///

	bool isDebug_ = false;

};