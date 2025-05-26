#pragma once
#include <vector>
#include <memory>
#include "Item.h"
#include "CollisionManager.h"

class Player;

class ItemManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update(Player* player);

	// 描画処理
	void Draw();

	// 衝突判定を登録
	void RegisterColliders(CollisionManager* collisionManager);

	// スポーン処理
	void Spawn(ItemType type, const Vector3& position);

private:
	std::vector<std::unique_ptr<Item>> items_;
};
