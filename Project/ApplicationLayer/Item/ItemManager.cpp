#define NOMINMAX
#include "ItemManager.h"
#include "Player.h"
#include "CollisionManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <sstream>
#include <string>

#include <Windows.h>

namespace K4E = ::Ken4lowEngine;

namespace
{
	const char* ToItemTypeName(ItemType type)
	{
		switch (type)
		{
		case ItemType::HealSmall: return "HealSmall";
		case ItemType::AmmoSmall: return "AmmoSmall";
		case ItemType::NextStageKey: return "NextStageKey";
		case ItemType::None:
		default: return "None";
		}
	}
}

void ItemManager::Initialize()
{
	Clear();
}

void ItemManager::Update(float deltaTime)
{
	for (auto& item : items_)
	{
		item->Update(deltaTime);
	}

	RemoveInactiveItems();
}

void ItemManager::Update(Player* player, float deltaTime)
{
	Update(deltaTime);
	if (player)
	{
		CheckPickup(*player);
	}
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
	if (!collisionManager) return;

	if (registeredCollisionManager_)
	{
		for (auto& item : items_)
		{
			if (item)
			{
				registeredCollisionManager_->RemoveCollider(item.get());
			}
		}
	}

	registeredCollisionManager_ = collisionManager;
	for (auto& item : items_)
	{
		if (item && item->IsActive())
		{
			registeredCollisionManager_->AddCollider(item.get());
		}
	}
}

void ItemManager::Spawn(ItemType type, const K4E::Vector3& position)
{
	SpawnConfigured(type, position);
}

void ItemManager::SpawnHealSmall(const K4E::Vector3& position)
{
	SpawnConfigured(ItemType::HealSmall, position);
}

void ItemManager::SpawnAmmoSmall(const K4E::Vector3& position)
{
	SpawnConfigured(ItemType::AmmoSmall, position);
}

void ItemManager::TryDropFromEnemyDeath(const K4E::Vector3& deathPosition)
{
	K4E::Vector3 dropPosition = deathPosition;
	dropPosition.y += 0.5f;

	if (forceEnemyDeathDrop_)
	{
		SpawnHealSmall(dropPosition);
		lastDroppedItemType_ = ItemType::HealSmall;
		return;
	}

	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	const float roll = dist(rng_);

	// ドロップ抽選は Heal → Ammo の順で合計確率を評価し、敵1体につき最大1個だけ出す。
	if (roll < healDropChance_)
	{
		SpawnHealSmall(dropPosition);
		lastDroppedItemType_ = ItemType::HealSmall;
	}
	else if (roll < healDropChance_ + ammoDropChance_)
	{
		SpawnAmmoSmall(dropPosition);
		lastDroppedItemType_ = ItemType::AmmoSmall;
	}
	else
	{
		lastDroppedItemType_ = ItemType::None;
	}
}

void ItemManager::CheckPickup(Player& player)
{
	const K4E::Vector3 playerPos = player.GetCenterPosition();

	for (auto& item : items_)
	{
		if (!item->IsActive()) continue;

		if (item->CheckCollisionWithPlayer(playerPos) && item->OnPickup(player))
		{
			lastPickedItemType_ = item->GetType();
			collectedEvents_.push_back(item->GetType());
		}
	}

	RemoveInactiveItems();
}

void ItemManager::Clear()
{
	if (registeredCollisionManager_)
	{
		for (auto& item : items_)
		{
			if (item)
			{
				registeredCollisionManager_->RemoveCollider(item.get());
			}
		}
	}
	items_.clear();
	collectedEvents_.clear();
	lastPickedItemType_ = ItemType::None;
	lastDroppedItemType_ = ItemType::None;
	registeredCollisionManager_ = nullptr;
}

bool ItemManager::ConsumeCollected(ItemType type)
{
	auto it = std::find(collectedEvents_.begin(), collectedEvents_.end(), type);
	if (it != collectedEvents_.end())
	{
		collectedEvents_.erase(it);
		return true;
	}

	return false;
}

int ItemManager::GetActiveItemCount() const
{
	return static_cast<int>(std::count_if(items_.begin(), items_.end(), [](const std::unique_ptr<Item>& item)
		{
			return item && item->IsActive();
		}));
}

int ItemManager::GetActiveItemCount(ItemType type) const
{
	return static_cast<int>(std::count_if(items_.begin(), items_.end(), [type](const std::unique_ptr<Item>& item)
		{
			return item && item->IsActive() && item->GetType() == type;
		}));
}

void ItemManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("アイテム管理"))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Item数: %d", GetActiveItemCount());
	ImGui::Text("HealSmall数: %d", GetActiveItemCount(ItemType::HealSmall));
	ImGui::Text("AmmoSmall数: %d", GetActiveItemCount(ItemType::AmmoSmall));
	ImGui::Checkbox("敵死亡時ドロップを必ず発生させる", &forceEnemyDeathDrop_);
	float healDropPercent = healDropChance_ * 100.0f;
	float ammoDropPercent = ammoDropChance_ * 100.0f;
	if (ImGui::SliderFloat("Healドロップ確率", &healDropPercent, 0.0f, 100.0f, "%.0f%%"))
	{
		healDropChance_ = healDropPercent / 100.0f;
	}
	if (ImGui::SliderFloat("Ammoドロップ確率", &ammoDropPercent, 0.0f, 100.0f, "%.0f%%"))
	{
		ammoDropChance_ = ammoDropPercent / 100.0f;
	}
	ImGui::DragInt("Heal回復量", &healAmount_, 1, 0, 999);
	ImGui::DragInt("Ammo回復量", &ammoAmount_, 1, 0, 999);
	ImGui::DragFloat("pickupRadius", &pickupRadius_, 0.1f, 0.1f, 20.0f, "%.2f");
	ImGui::Text("最後に取得したItemType: %s", ToItemTypeName(lastPickedItemType_));
	ImGui::Text("最後にドロップしたItemType: %s", ToItemTypeName(lastDroppedItemType_));

	ImGui::End();
#else
	(void)this;
#endif
}

void ItemManager::RemoveInactiveItems()
{
	items_.erase(std::remove_if(items_.begin(), items_.end(), [this](const std::unique_ptr<Item>& item)
		{
			const bool shouldRemove = !item || item->IsCollected() || item->IsExpired();
			if (shouldRemove && item && registeredCollisionManager_)
			{
				registeredCollisionManager_->RemoveCollider(item.get());
			}
			return shouldRemove;
		}), items_.end());
}

void ItemManager::SpawnConfigured(ItemType type, const K4E::Vector3& position)
{
	if (type == ItemType::None) return;

	auto item = std::make_unique<Item>();
	item->Initialize(type, position, healAmount_, ammoAmount_, pickupRadius_);
	if (registeredCollisionManager_)
	{
		registeredCollisionManager_->AddCollider(item.get());
	}
	items_.push_back(std::move(item));

	std::ostringstream oss;
	oss << "DropItem Spawned"
		<< " ItemType=" << ToItemTypeName(type)
		<< " SpawnPosition=(" << position.x << ", " << position.y << ", " << position.z << ")"
		<< " ActiveItemCount=" << GetActiveItemCount()
		<< "\n";
	OutputDebugStringA(oss.str().c_str());
}
