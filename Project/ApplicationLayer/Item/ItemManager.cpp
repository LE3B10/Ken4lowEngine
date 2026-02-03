#include "ItemManager.h"
#include "Player.h"
#include "CollisionManager.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void ItemManager::Initialize()
{
	// アイテムリストをクリア
	items_.clear();

	// 取得イベントリストをクリア
	collectedEvents_.clear();
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void ItemManager::Update(Player* player, float deltaTime)
{
	(void)player;

	// アイテムの更新とプレイヤーとの衝突判定
	for (auto& item : items_) item->Update(deltaTime);

	// 消える前に「何を拾ったか」記録
	for (auto& item : items_)
	{
		if (item->IsCollected())
		{
			collectedEvents_.push_back(item->GetType()); // 取得イベントを追加
		}
	}

	// 寿命切れまたは取得済みのアイテムを削除
	items_.erase(std::remove_if(items_.begin(), items_.end(), [](const std::unique_ptr<Item>& item) {
		return item->IsCollected() || item->IsExpired(); }), items_.end());
}

/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void ItemManager::Draw()
{
	// アイテムの描画
	for (auto& item : items_) item->Draw();
}

/// -------------------------------------------------------------
///						衝突判定を登録
/// -------------------------------------------------------------
void ItemManager::RegisterColliders(CollisionManager* collisionManager)
{
	// 収集されていないアイテムのコライダーを登録
	for (auto& item : items_)
	{
		// 収集されていないアイテムのみ登録
		if (!item->IsCollected())
		{
			collisionManager->AddCollider(item.get()); // コライダーを登録
		}
	}
}

/// -------------------------------------------------------------
///							スポーン処理
/// -------------------------------------------------------------
void ItemManager::Spawn(ItemType type, const K4E::Vector3& position)
{
	auto item = std::make_unique<Item>(); // アイテムを生成
	item->Initialize(type, position);	  // 初期化
	items_.push_back(std::move(item));	  // リストに追加
}

bool ItemManager::ConsumeCollected(ItemType type)
{
	// 取得イベントリストを検索
	auto it = std::find(collectedEvents_.begin(), collectedEvents_.end(), type);
	if (it != collectedEvents_.end())
	{
		// 見つかった場合、イベントを消費して true を返す
		collectedEvents_.erase(it);
		return true;
	}

	// 見つからなかった場合、false を返す
	return false;
}
