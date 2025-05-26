#include "ItemDropTable.h"

ItemDropTable::ItemDropTable()
{
	std::random_device rd;
	rng_ = std::mt19937(rd());
	chanceDist_ = std::uniform_int_distribution<int>(0, 99);
}

void ItemDropTable::AddEntry(ItemType itemType, int weight)
{
	// 無効な重みは無視
	if (weight <= 0) return;

	entries_.push_back({ itemType, weight });
	totalWeight_ += weight;
}

void ItemDropTable::Clear()
{
	entries_.clear();
	totalWeight_ = 0;
}

void ItemDropTable::RemoveEntry(ItemType itemType)
{
	totalWeight_ = 0;
	entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const DropEntry& entry) {
		return entry.itemType == itemType;
		}), entries_.end());
	for (const auto& e : entries_) {
		totalWeight_ += e.weight;
	}
}

bool ItemDropTable::RollForDrop(ItemType& outItemType)
{
	if (chanceDist_(rng_) >= dropChancePercent_) return false;

	std::uniform_int_distribution<int> weightDist(0, totalWeight_ - 1);
	int r = weightDist(rng_);

	int accumulated = 0;
	for (const auto& entry : entries_)
	{
		accumulated += entry.weight;
		if (r < accumulated)
		{
			outItemType = entry.itemType;
			return true;
		}
	}

	return false;
}
