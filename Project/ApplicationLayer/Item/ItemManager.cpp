#include "ItemManager.h"
#include "Player.h"
#include "CollisionManager.h"

void ItemManager::Initialize()
{
	items_.clear();
}

void ItemManager::Update(Player* player)
{
	for (auto& item : items_)
	{
		item->Update();
		if (!item->IsCollected())
		{
			if (item->CheckCollisionWithPlayer(player->GetWorldTransform()->translate_))
			{
				item->ApplyTo(player);
			}
		}
	}

	// 寿命切れまたは取得済みのアイテムを削除
	items_.erase(
		std::remove_if(items_.begin(), items_.end(), [](const std::unique_ptr<Item>& item) {
			return item->IsCollected() || item->IsExpired(); }),
			items_.end()
			);
}

void ItemManager::Draw()
{
	for (auto& item : items_)
	{
		item->Draw();
	}
}

void ItemManager::RegisterColliders(CollisionManager* collisionManager)
{
	for (auto& item : items_)
	{
		if (!item->IsCollected())
		{
			collisionManager->AddCollider(item.get());
		}
	}
}

void ItemManager::Spawn(ItemType type, const Vector3& position)
{
	auto item = std::make_unique<Item>();
	item->Initialize(type, position);
	items_.push_back(std::move(item));
}
