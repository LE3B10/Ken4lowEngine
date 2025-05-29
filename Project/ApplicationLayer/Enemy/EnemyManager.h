#pragma once
#include "Enemy.h"
#include "ItemDropTable.h"

#include <memory>
#include <vector>


/// ---------- 前方宣言 ---------- ///
class Player;
class ItemManager;

/// ---------- 列挙型 ---------- ///
enum class EnemySpawnDirection
{
	Left,   // 左からスポーン
	Right,  // 右からスポーン
	Front,  // 前からスポーン
	Back    // 後ろからスポーン
};

/// -------------------------------------------------------------
///				　		エネミーマネージャー
/// -------------------------------------------------------------
class EnemyManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Player* player);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	void RegisterColliders(class CollisionManager* collisionManager);

	// ImGui描画処理
	void DrawImGui();

	// 次のウェーブを開始
	void StartNextWave();

	// ウェーブクリア判定
	bool IsWaveClear() const { return spawnedEnemies_ >= totalEnemiesThisWave_ && enemies_.empty(); }

	// 現在のウェーブ数を取得
	int GetCurrentWave() const { return currentWave_ - 1; }

	// 出現位置を切り替える処理
	void GetSpawnPosition(Vector3& outPosition, EnemySpawnDirection direction);
	
	// スポーンしたエネミーの数を取得
	const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const { return enemies_; }

	void SetItemManager(ItemManager* itemManager) { itemManager_ = itemManager; }

private: /// ---------- メンバ変数 ---------- ///

	ItemDropTable itemDropTable_;

	ItemManager* itemManager_ = nullptr; // 所有はしない

	std::vector<std::unique_ptr<Enemy>> enemies_; // エネミーのリスト
	Player* player_ = nullptr; // プレイヤーへのポインタ

	int currentWave_ = 1;		   // 現在のウェーブ数
	int totalEnemiesThisWave_ = 0; // 現在のウェーブでスポーンするエネミーの数
	int spawnedEnemies_ = 0;	   // スポーンしたエネミーの数
	float spawnInterval_ = 1.0f;   // スポーン間隔
	float spawnTimer_ = 0.0f;	   // スポーンタイマー
};
