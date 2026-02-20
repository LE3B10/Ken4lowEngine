#pragma once
#include <memory>
#include <vector>
#include <string>

#include "Player.h"
#include "Enemy.h"
#include "EnemyArchetype.h"

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

    Player* GetPlayer() { return player_.get(); }
    const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const { return enemies_; }

    // 生成
    Enemy& SpawnEnemy(const K4E::Vector3& pos, const std::string& modelPath = "cube.gltf");
    Enemy& SpawnEnemy(EnemyArchetype type, const K4E::Vector3& pos, const std::string& modelPath = "cube.gltf");

    // 全消し
    void ClearEnemies();

	int GetEnemyCount() const { return static_cast<int>(enemies_.size()); }

public: /// ---------- デバッグ用 ---------- ///

	void SetDebug(bool on) { isDebug_ = on; }
	bool IsDebug() const { return isDebug_; }

private:
    void InjectPlayerDeps(Player& p);
    void InjectEnemyDeps(Enemy& e);

private:
    GameContext ctx_{}; // ポインタ保持しない（Scene側ローカルctxの寿命問題を避ける）

    std::unique_ptr<Player> player_;
    std::vector<std::unique_ptr<Enemy>> enemies_;

private: /// ---------- デバッグ用 ---------- ///

    bool isDebug_ = false;

};