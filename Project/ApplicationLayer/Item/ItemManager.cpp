#include "ItemManager.h"
#include "Player.h"
#include "CollisionManager.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void ItemManager::Initialize()
{
	// アイテムリストをクリア
	items_.clear();
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void ItemManager::Update(Player* player)
{
	// アイテムの更新とプレイヤーとの衝突判定
	for (auto& item : items_) item->Update();

	// 寿命切れまたは取得済みのアイテムを削除
	items_.erase(std::remove_if(items_.begin(), items_.end(), [](const std::unique_ptr<Item>& item) {
		return item->IsCollected() || item->IsExpired(); }),
		items_.end()
		);
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
void ItemManager::Spawn(ItemType type, const Vector3& position)
{
	auto item = std::make_unique<Item>(); // アイテムを生成
	item->Initialize(type, position);	  // 初期化
	items_.push_back(std::move(item));	  // リストに追加
}
