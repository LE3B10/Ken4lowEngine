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
	K4E::Vector3 spawnPosition = position;
	spawnPosition.y += 0.5f;
	SpawnConfigured(ItemType::AmmoSmall, spawnPosition);
}

void ItemManager::TryDropFromEnemyDeath(const K4E::Vector3& deathPosition)
{
	TryDropEnemyItem(deathPosition);
}

void ItemManager::TryDropEnemyItem(const K4E::Vector3& deathPosition)
{
	K4E::Vector3 dropPosition = deathPosition;
	dropPosition.y += 0.5f;
	lastDropPosition_ = dropPosition;

	if (!enemyDeathDropEnabled_)
	{
		lastDroppedItemType_ = ItemType::None;
		LogDropRollResult(lastDroppedItemType_, dropPosition);
		return;
	}

	lastDroppedItemType_ = RollEnemyDrop();
	if (lastDroppedItemType_ != ItemType::None)
	{
		SpawnDropItem(lastDroppedItemType_, dropPosition);
	}

	LogDropRollResult(lastDroppedItemType_, dropPosition);
}

ItemType ItemManager::RollEnemyDrop()
{
	const float healRate = std::max(0.0f, healDropChance_);
	const float ammoRate = std::max(0.0f, ammoDropChance_);
	float totalDropRate = healRate + ammoRate;

	if (totalDropRate <= 0.0f)
	{
		return forceEnemyDeathDrop_ ? ItemType::HealSmall : ItemType::None;
	}

	float effectiveHealRate = healRate;
	float effectiveAmmoRate = ammoRate;
	if (forceEnemyDeathDrop_ || totalDropRate > 1.0f)
	{
		effectiveHealRate = healRate / totalDropRate;
		effectiveAmmoRate = ammoRate / totalDropRate;
	}

	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	const float roll = dist(rng_);

	// Heal/Ammo/None の抽選を ItemManager に集約し、シーン側に確率分岐を置かない。
	if (roll < effectiveHealRate)
	{
		return ItemType::HealSmall;
	}
	if (roll < effectiveHealRate + effectiveAmmoRate)
	{
		return ItemType::AmmoSmall;
	}

	return ItemType::None;
}

void ItemManager::CheckPickup(Player& player)
{
	const K4E::Vector3 playerPos = player.GetCenterPosition();

	for (auto& item : items_)
	{
		if (!item->IsActive()) continue;

		if (item->CheckCollisionWithPlayer(playerPos))
		{
			const ItemType pickupType = item->GetType();
			const int reserveBefore = player.GetCurrentWeaponReserveAmmo();

			if (item->OnPickup(player))
			{
				lastPickedItemType_ = pickupType;
				lastKnownMagazineAmmo_ = player.GetCurrentWeaponMagazineAmmo();
				lastKnownReserveAmmo_ = player.GetCurrentWeaponReserveAmmo();
				lastKnownMaxReserveAmmo_ = player.GetCurrentWeaponMaxReserveAmmo();
				lastAmmoSmallReserveRestored_ = (pickupType == ItemType::AmmoSmall)
					? std::max(0, lastKnownReserveAmmo_ - reserveBefore)
					: 0;
				collectedEvents_.push_back(pickupType);

				std::ostringstream oss;
				oss << "Item Picked"
					<< " ItemType=" << ToItemTypeName(pickupType)
					<< " MagazineAmmo=" << lastKnownMagazineAmmo_
					<< " ReserveAmmo=" << lastKnownReserveAmmo_
					<< " MaxReserveAmmo=" << lastKnownMaxReserveAmmo_
					<< " AmmoSmallAmount=" << ammoAmount_
					<< " AmmoSmallReserveRestored=" << lastAmmoSmallReserveRestored_
					<< "\n";
				OutputDebugStringA(oss.str().c_str());
			}
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
	lastDropPosition_ = {};
	lastKnownMagazineAmmo_ = 0;
	lastKnownReserveAmmo_ = 0;
	lastKnownMaxReserveAmmo_ = 0;
	lastAmmoSmallReserveRestored_ = 0;
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

float ItemManager::GetNoneDropChance() const
{
	return std::max(0.0f, 1.0f - std::max(0.0f, healDropChance_) - std::max(0.0f, ammoDropChance_));
}

void ItemManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("アイテム管理"))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Active Item数: %d", GetActiveItemCount());
	ImGui::Text("HealSmall数: %d", GetActiveItemCount(ItemType::HealSmall));
	ImGui::Text("AmmoSmall数: %d", GetActiveItemCount(ItemType::AmmoSmall));
	ImGui::Checkbox("敵死亡時ドロップ有効", &enemyDeathDropEnabled_);
	ImGui::Checkbox("必ず何か落とす", &forceEnemyDeathDrop_);
	float healDropPercent = healDropChance_ * 100.0f;
	float ammoDropPercent = ammoDropChance_ * 100.0f;
	if (ImGui::SliderFloat("HealSmallドロップ確率", &healDropPercent, 0.0f, 100.0f, "%.0f%%"))
	{
		healDropChance_ = healDropPercent / 100.0f;
	}
	if (ImGui::SliderFloat("AmmoSmallドロップ確率", &ammoDropPercent, 0.0f, 100.0f, "%.0f%%"))
	{
		ammoDropChance_ = ammoDropPercent / 100.0f;
	}
	ImGui::Text("None確率表示: %.0f%%", GetNoneDropChance() * 100.0f);
	if (healDropChance_ + ammoDropChance_ > 1.0f)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "Heal+Ammoが100%%超過: 内部で正規化します");
	}
	if (forceEnemyDeathDrop_)
	{
		ImGui::Text("必ず何か落とすON: Noneは選ばれません");
	}
	ImGui::DragInt("Heal回復量", &healAmount_, 1, 0, 999);
	ImGui::DragInt("Ammo回復量", &ammoAmount_, 1, 0, 999);
	ImGui::DragFloat("pickupRadius", &pickupRadius_, 0.1f, 0.1f, 20.0f, "%.2f");
	ImGui::Text("現在マガジン弾数: %d", lastKnownMagazineAmmo_);
	ImGui::Text("予備弾薬: %d", lastKnownReserveAmmo_);
	ImGui::Text("最大予備弾薬: %d", lastKnownMaxReserveAmmo_);
	ImGui::Text("AmmoSmall回復量: %d", ammoAmount_);
	ImGui::Text("AmmoSmall取得時に回復した予備弾薬量: %d", lastAmmoSmallReserveRestored_);
	ImGui::Text("最後に取得したItemType: %s", ToItemTypeName(lastPickedItemType_));
	ImGui::Text("最後にドロップしたItemType: %s", ToItemTypeName(lastDroppedItemType_));
	ImGui::Text("最後のドロップ位置: (%.2f, %.2f, %.2f)", lastDropPosition_.x, lastDropPosition_.y, lastDropPosition_.z);

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

void ItemManager::SpawnDropItem(ItemType type, const K4E::Vector3& position)
{
	SpawnConfigured(type, position);
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

void ItemManager::LogDropRollResult(ItemType type, const K4E::Vector3& position) const
{
	std::ostringstream oss;
	oss << "Drop Roll Result"
		<< " ItemType=" << ToItemTypeName(type)
		<< " SpawnPosition=(" << position.x << ", " << position.y << ", " << position.z << ")"
		<< " ActiveItemCount=" << GetActiveItemCount()
		<< "\n";
	OutputDebugStringA(oss.str().c_str());
}
