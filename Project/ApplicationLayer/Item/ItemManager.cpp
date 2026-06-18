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
		if (!item) continue;
		ApplyVisualSettings(*item);
		item->Update(deltaTime);
		if (item->IsActive())
		{
			itemVisualEffect_.UpdateIdle(*item, deltaTime);
		}
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

void ItemManager::SpawnAmmoSmall(const K4E::Vector3& position, int ammoAmount)
{
	K4E::Vector3 spawnPosition = position;
	spawnPosition.y += 0.5f;
	// 弾薬回復スポナー専用に、既存のAmmoSmall見た目と取得処理を使いながら回復量だけ差し替える。
	SpawnConfigured(ItemType::AmmoSmall, spawnPosition, ammoAmount);
}

bool ItemManager::TryGetFirstActiveItemPosition(ItemType type, K4E::Vector3& outPosition) const
{
	for (const auto& item : items_)
	{
		if (item && item->IsActive() && item->GetType() == type)
		{
			outPosition = item->GetPosition();
			return true;
		}
	}
	return false;
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
		if (!item || !item->IsActive()) continue;

		if (item->CheckCollisionWithPlayer(playerPos))
		{
			lastItemEffectDebugInfo_ = {};
			lastItemEffectDebugInfo_.pickupDetected = true;
			lastItemEffectDebugInfo_.itemType = item->GetType();
			lastPickedItemType_ = item->GetType();

			const bool effectApplied = ApplyItemEffect(*item, player);
			const bool consumePickedItem = effectApplied || (consumeItemWhenFull_ && lastItemEffectDebugInfo_.noEffectBecauseFull);
			if (consumePickedItem)
			{
				itemVisualEffect_.PlayPickup(lastItemEffectDebugInfo_.itemType, item->GetPosition());
				itemVisualEffect_.StopIdle(*item);
				item->MarkCollected();
				collectedEvents_.push_back(lastItemEffectDebugInfo_.itemType);
			}

			lastKnownMagazineAmmo_ = player.GetCurrentWeaponMagazineAmmo();
			lastKnownReserveAmmo_ = player.GetCurrentWeaponReserveAmmo();
			lastKnownMaxReserveAmmo_ = player.GetCurrentWeaponMaxReserveAmmo();
			lastAmmoSmallReserveRestored_ = (lastItemEffectDebugInfo_.itemType == ItemType::AmmoSmall)
				? std::max(0, lastItemEffectDebugInfo_.reserveAfter - lastItemEffectDebugInfo_.reserveBefore)
				: 0;

			LogItemEffectResult();
		}
	}

	RemoveInactiveItems();
}

bool ItemManager::ApplyItemEffect(Item& item, Player& player)
{
	ItemEffectDebugInfo& info = lastItemEffectDebugInfo_;
	info.itemType = item.GetType();
	info.effectApplied = false;
	info.failReason.clear();
	info.noEffectBecauseFull = false;
	info.hpBefore = player.GetHP();
	info.hpAfter = info.hpBefore;
	info.maxHp = player.GetMaxHP();
	info.reserveBefore = player.GetCurrentWeaponReserveAmmo();
	info.reserveAfter = info.reserveBefore;
	info.maxReserve = player.GetCurrentWeaponMaxReserveAmmo();
	info.magazineAmmo = player.GetCurrentWeaponMagazineAmmo();
	info.currentWeaponName = player.GetCurrentWeaponName();
	info.currentWeaponAmmoRecoverable = player.CanCurrentWeaponRecoverAmmo();
	info.hudMagazineAmmo = 0;
	info.hudReserveAmmo = 0;

	// 取得判定と効果適用を分離し、増加量を比較して成功/失敗理由を残す。
	switch (item.GetType())
	{
	case ItemType::HealSmall:
		if (item.GetHealAmount() <= 0)
		{
			info.failReason = "HealSmall回復量が0以下です";
			break;
		}
		if (info.hpBefore >= info.maxHp)
		{
			info.failReason = "HPが最大値のため回復しませんでした";
			info.noEffectBecauseFull = true;
			break;
		}
		player.Heal(static_cast<float>(item.GetHealAmount()));
		info.hpAfter = player.GetHP();
		info.effectApplied = info.hpAfter > info.hpBefore;
		if (!info.effectApplied)
		{
			info.failReason = "Heal処理後もHPが増えませんでした";
		}
		break;

	case ItemType::AmmoSmall:
		if (item.GetAmmoAmount() <= 0)
		{
			info.failReason = "AmmoSmall回復量が0以下です";
			break;
		}
		if (!info.currentWeaponAmmoRecoverable)
		{
			info.failReason = "現在装備中武器は弾薬回復対象ではありません";
			break;
		}
		if (info.maxReserve <= 0)
		{
			info.failReason = "最大予備弾薬が0以下です";
			break;
		}
		if (info.reserveBefore >= info.maxReserve)
		{
			info.failReason = "予備弾薬が最大値のため回復しませんでした";
			info.noEffectBecauseFull = true;
			break;
		}
		(void)player.AddReserveAmmo(item.GetAmmoAmount());
		info.reserveAfter = player.GetCurrentWeaponReserveAmmo();
		info.magazineAmmo = player.GetCurrentWeaponMagazineAmmo();
		info.maxReserve = player.GetCurrentWeaponMaxReserveAmmo();
		info.effectApplied = info.reserveAfter > info.reserveBefore;
		if (!info.effectApplied)
		{
			info.failReason = "AddReserveAmmo後も予備弾薬が増えませんでした";
		}
		break;

	case ItemType::NextStageKey:
		info.effectApplied = true;
		break;

	case ItemType::None:
	default:
		info.failReason = "未対応のItemTypeです";
		break;
	}

	if (info.hpAfter == info.hpBefore)
	{
		info.hpAfter = player.GetHP();
	}
	if (info.reserveAfter == info.reserveBefore)
	{
		info.reserveAfter = player.GetCurrentWeaponReserveAmmo();
	}
	info.maxHp = player.GetMaxHP();
	info.maxReserve = player.GetCurrentWeaponMaxReserveAmmo();
	info.magazineAmmo = player.GetCurrentWeaponMagazineAmmo();

	WeaponSlot::HudSnapshot hud{};
	if (player.GetWeaponSlotHUD(hud) && hud.selectedIndex >= 0 && hud.selectedIndex < WeaponSlot::kSlotCount)
	{
		const auto& selected = hud.slotStates[hud.selectedIndex];
		info.hudMagazineAmmo = selected.ammoInfo.currentAmmo;
		info.hudReserveAmmo = selected.ammoInfo.reserveAmmo;
	}

	if (info.effectApplied)
	{
		info.failReason = "なし";
	}

	return info.effectApplied;
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
	lastItemEffectDebugInfo_ = {};
	itemVisualEffect_.Clear();
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
	ImGui::Checkbox("満タン時もItemを消す", &consumeItemWhenFull_);
	ImGui::SeparatorText("最後のItem取得診断");
	ImGui::Text("最後に拾ったItemType: %s", ToItemTypeName(lastItemEffectDebugInfo_.itemType));
	ImGui::Text("Item取得判定: %s", lastItemEffectDebugInfo_.pickupDetected ? "true" : "false");
	ImGui::Text("効果適用成功: %s", lastItemEffectDebugInfo_.effectApplied ? "true" : "false");
	ImGui::TextWrapped("効果適用失敗理由: %s", lastItemEffectDebugInfo_.failReason.c_str());
	ImGui::Text("取得前HP: %.1f", lastItemEffectDebugInfo_.hpBefore);
	ImGui::Text("取得後HP: %.1f", lastItemEffectDebugInfo_.hpAfter);
	ImGui::Text("最大HP: %.1f", lastItemEffectDebugInfo_.maxHp);
	ImGui::Text("取得前予備弾薬: %d", lastItemEffectDebugInfo_.reserveBefore);
	ImGui::Text("取得後予備弾薬: %d", lastItemEffectDebugInfo_.reserveAfter);
	ImGui::Text("最大予備弾薬: %d", lastItemEffectDebugInfo_.maxReserve);
	ImGui::Text("現在武器名: %s", lastItemEffectDebugInfo_.currentWeaponName.c_str());
	ImGui::Text("現在武器が弾薬回復可能か: %s", lastItemEffectDebugInfo_.currentWeaponAmmoRecoverable ? "true" : "false");
	ImGui::Text("HUD表示弾薬: %d / %d", lastItemEffectDebugInfo_.hudMagazineAmmo, lastItemEffectDebugInfo_.hudReserveAmmo);
	ImGui::Text("最後に取得したItemType: %s", ToItemTypeName(lastPickedItemType_));
	ImGui::Text("最後にドロップしたItemType: %s", ToItemTypeName(lastDroppedItemType_));
	ImGui::Text("最後のドロップ位置: (%.2f, %.2f, %.2f)", lastDropPosition_.x, lastDropPosition_.y, lastDropPosition_.z);
	itemVisualEffect_.DrawImGui();

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
			if (shouldRemove && item)
			{
				itemVisualEffect_.StopIdle(*item);
				if (registeredCollisionManager_)
				{
					registeredCollisionManager_->RemoveCollider(item.get());
				}
			}
			return shouldRemove;
		}), items_.end());
}

void ItemManager::SpawnDropItem(ItemType type, const K4E::Vector3& position)
{
	SpawnConfigured(type, position);
}

void ItemManager::SpawnConfigured(ItemType type, const K4E::Vector3& position, int overrideAmmoAmount)
{
	if (type == ItemType::None) return;

	auto item = std::make_unique<Item>();
	// 通常ドロップは既定値、時間スポーン弾薬だけは指定回復量でItemを初期化する。
	const int ammoAmount = (overrideAmmoAmount >= 0) ? overrideAmmoAmount : ammoAmount_;
	item->Initialize(type, position, healAmount_, ammoAmount, pickupRadius_);
	ApplyVisualSettings(*item);
	itemVisualEffect_.StartIdle(*item);
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

void ItemManager::ApplyVisualSettings(Item& item)
{
	const auto& settings = itemVisualEffect_.GetSettings();
	item.SetVisualAnimationSettings(settings.itemFloatHeight, settings.itemFloatSpeed, settings.itemRotationSpeed);
	item.SetVisualColor(itemVisualEffect_.GetEffectColor(item.GetType()));
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


void ItemManager::LogItemEffectResult() const
{
	const ItemEffectDebugInfo& info = lastItemEffectDebugInfo_;
	std::ostringstream oss;
	oss << "Item取得診断"
		<< " 最後に拾ったItemType=" << ToItemTypeName(info.itemType)
		<< " Item取得判定=" << (info.pickupDetected ? "true" : "false")
		<< " 効果適用成功=" << (info.effectApplied ? "true" : "false")
		<< " 効果適用失敗理由=" << info.failReason
		<< " 取得前HP=" << info.hpBefore
		<< " 取得後HP=" << info.hpAfter
		<< " 最大HP=" << info.maxHp
		<< " 取得前予備弾薬=" << info.reserveBefore
		<< " 取得後予備弾薬=" << info.reserveAfter
		<< " 最大予備弾薬=" << info.maxReserve
		<< " 現在武器名=" << info.currentWeaponName
		<< " 現在武器が弾薬回復可能か=" << (info.currentWeaponAmmoRecoverable ? "true" : "false")
		<< " HUD表示弾薬=" << info.hudMagazineAmmo << "/" << info.hudReserveAmmo
		<< " 満タン時もItemを消す=" << (consumeItemWhenFull_ ? "true" : "false")
		<< "\n";
	OutputDebugStringA(oss.str().c_str());
}
