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
	// デバッグ用：アイテムの種類を文字列に変換する関数
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

/// -------------------------------------------------------------
///						 初期化処理
/// -------------------------------------------------------------
void ItemManager::Initialize()
{
	// アイテムを初期化
	Clear();
}

/// -------------------------------------------------------------
/// 					 更新処理
/// -------------------------------------------------------------
void ItemManager::Update(Player* player)
{
	// アイテムの状態を更新し、プレイヤーとの衝突判定を行う
	for (auto& item : items_)
	{
		// アイテムがnullptrの場合はスキップ
		if (!item) continue;

		// アイテムの状態を更新
		ApplyVisualSettings(*item); // アイテムの見た目の設定を適用
		item->Update();

		// アイテムがアクティブな場合は待機演出を更新
		if (item->IsActive())
		{
			itemVisualEffect_.UpdateIdle(*item);
		}
	}

	// 取得済みまたは寿命切れのアイテムを安全に削除
	RemoveInactiveItems();

	// プレイヤーが存在する場合は、アイテムの取得判定を行う
	if (player) CheckPickup(*player);
}

/// -------------------------------------------------------------
/// 					 描画処理
/// -------------------------------------------------------------
void ItemManager::Draw()
{
	// アイテムを描画
	for (auto& item : items_)
	{
		item->Draw();
	}
}

/// -------------------------------------------------------------
///				CollisionManagerへのコライダー登録
/// -------------------------------------------------------------
void ItemManager::RegisterColliders(CollisionManager* collisionManager)
{
	// CollisionManagerがnullptrの場合は登録を行わない
	if (!collisionManager) return;

	// 既存のCollisionManagerからコライダーを削除
	if (registeredCollisionManager_)
	{
		// 既存のCollisionManagerからコライダーを削除
		for (auto& item : items_)
		{
			if (item)
			{
				registeredCollisionManager_->RemoveCollider(item.get());
			}
		}
	}

	// 新しいCollisionManagerにコライダーを登録
	registeredCollisionManager_ = collisionManager;

	// 新しいCollisionManagerにコライダーを登録
	for (auto& item : items_)
	{
		// アイテムがnullptrでなく、かつアクティブな場合にコライダーを登録
		if (item && item->IsActive())
		{
			registeredCollisionManager_->AddCollider(item.get());
		}
	}
}

/// -------------------------------------------------------------
///					 アイテムスポーン
/// -------------------------------------------------------------
void ItemManager::Spawn(ItemType type, const K4E::Vector3& position)
{
	// アイテムスポーンの個別処理を呼び出す
	SpawnConfigured(type, position);
}

/// -------------------------------------------------------------
/// 			アイテムスポーンの個別処理
/// -------------------------------------------------------------
void ItemManager::SpawnHealSmall(const K4E::Vector3& position)
{
	SpawnConfigured(ItemType::HealSmall, position);
}

/// -------------------------------------------------------------
/// 			アイテムスポーンの個別処理
/// -------------------------------------------------------------
void ItemManager::SpawnAmmoSmall(const K4E::Vector3& position)
{
	// 弾薬アイテムは地面に埋まらないように少し上にスポーンさせる
	K4E::Vector3 spawnPosition = position;
	spawnPosition.y += 0.5f;

	// 弾薬回復スポナー専用に、既存のAmmoSmall見た目と取得処理を使いながら回復量だけ差し替える。
	SpawnConfigured(ItemType::AmmoSmall, spawnPosition);
}

/// -------------------------------------------------------------
/// 			アイテムスポーンの個別処理
/// -------------------------------------------------------------
void ItemManager::SpawnAmmoSmall(const K4E::Vector3& position, int ammoAmount)
{
	K4E::Vector3 spawnPosition = position;
	spawnPosition.y += 0.5f;
	// 弾薬回復スポナー専用に、既存のAmmoSmall見た目と取得処理を使いながら回復量だけ差し替える。
	SpawnConfigured(ItemType::AmmoSmall, spawnPosition, ammoAmount);
}

/// -------------------------------------------------------------
///				アイテムスポーンの共通処理
/// -------------------------------------------------------------
bool ItemManager::TryGetFirstActiveItemPosition(ItemType type, K4E::Vector3& outPosition) const
{
	// 指定されたアイテムタイプの最初のアクティブなアイテムの位置を取得する
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

/// -------------------------------------------------------------
///				敵の死亡位置にアイテムを落とす試行
/// -------------------------------------------------------------
void ItemManager::TryDropFromEnemyDeath(const K4E::Vector3& deathPosition)
{
	// 敵の死亡位置にアイテムを落とす試行。ドロップ条件を満たす場合、アイテムをスポーンさせる。
	TryDropEnemyItem(deathPosition);
}

/// -------------------------------------------------------------
/// 			敵の死亡位置にアイテムを落とす試行
/// -------------------------------------------------------------
void ItemManager::TryDropEnemyItem(const K4E::Vector3& deathPosition)
{
	// 敵の死亡位置にアイテムを落とす試行。ドロップ条件を満たす場合、アイテムをスポーンさせる。
	K4E::Vector3 dropPosition = deathPosition;
	dropPosition.y += 0.5f;

	// ドロップ位置を記録
	lastDropPosition_ = dropPosition;

	// ドロップが無効化されている場合は何もせずに終了
	if (!enemyDeathDropEnabled_)
	{
		lastDroppedItemType_ = ItemType::None;

		// ドロップ結果をログに出力
		LogDropRollResult(lastDroppedItemType_, dropPosition);
		return;
	}

	// ドロップの抽選を行い、アイテムをスポーンさせる
	lastDroppedItemType_ = RollEnemyDrop();

	// ドロップが決定した場合のみスポーン処理を行う
	if (lastDroppedItemType_ != ItemType::None)
	{
		SpawnDropItem(lastDroppedItemType_, dropPosition);
	}

	// ドロップ結果をログに出力
	LogDropRollResult(lastDroppedItemType_, dropPosition);
}

/// -------------------------------------------------------------
/// 					敵のドロップ抽選
/// -------------------------------------------------------------
ItemType ItemManager::RollEnemyDrop()
{
	// ドロップ率を正規化して、HealSmall、AmmoSmall、None のいずれかを返す
	const float healRate = std::max(0.0f, healDropChance_);
	const float ammoRate = std::max(0.0f, ammoDropChance_);

	// ドロップ率の合計を計算
	float totalDropRate = healRate + ammoRate;

	// ドロップ率が0以下の場合は、強制ドロップフラグに応じてHealSmallまたはNoneを返す
	if (totalDropRate <= 0.0f)
	{
		return forceEnemyDeathDrop_ ? ItemType::HealSmall : ItemType::None;
	}

	float effectiveHealRate = healRate;
	float effectiveAmmoRate = ammoRate;

	// ドロップ率が1を超える場合は、HealSmallとAmmoSmallのドロップ率を正規化する
	if (forceEnemyDeathDrop_ || totalDropRate > 1.0f)
	{
		effectiveHealRate = healRate / totalDropRate;
		effectiveAmmoRate = ammoRate / totalDropRate;
	}

	// 乱数を生成して、HealSmall、AmmoSmall、None のいずれかを返す
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	const float roll = dist(rng_);

	// Heal/Ammo/None の抽選を ItemManager に集約し、シーン側に確率分岐を置かない。
	if (roll < effectiveHealRate)
	{
		return ItemType::HealSmall; // HealSmallがドロップする場合
	}

	// AmmoSmallがドロップする場合
	if (roll < effectiveHealRate + effectiveAmmoRate)
	{
		return ItemType::AmmoSmall; // AmmoSmallがドロップする場合
	}

	// どちらもドロップしない場合はNoneを返す
	return ItemType::None;
}

/// -------------------------------------------------------------
/// 				アイテムの取得チェック
/// -------------------------------------------------------------
void ItemManager::CheckPickup(Player& player)
{
	// プレイヤーの中心座標を取得
	const K4E::Vector3 playerPos = player.GetCenterPosition();

	// 各アイテムに対して、プレイヤーとの衝突判定を行い、取得可能な場合は効果を適用する
	for (auto& item : items_)
	{
		// アイテムがnullptrまたは非アクティブの場合はスキップ
		if (!item || !item->IsActive()) continue;

		// プレイヤーとの衝突判定を行い、取得可能な場合は効果を適用する
		if (item->CheckCollisionWithPlayer(playerPos))
		{
			lastItemEffectDebugInfo_ = {};						 // 取得判定のデバッグ情報を初期化
			lastItemEffectDebugInfo_.pickupDetected = true;		 // 取得判定が発生したことを記録
			lastItemEffectDebugInfo_.itemType = item->GetType(); // 取得判定が発生したアイテムの種類を記録
			lastPickedItemType_ = item->GetType();				 // 取得判定が発生したアイテムの種類を記録

			// アイテムの効果をプレイヤーに適用し、効果が適用されたかどうかを記録する
			const bool effectApplied = ApplyItemEffect(*item, player);

			// アイテムの効果が適用された場合、またはアイテムが満タンで消費される設定の場合は、アイテムを消費する
			const bool consumePickedItem = effectApplied || (consumeItemWhenFull_ && lastItemEffectDebugInfo_.noEffectBecauseFull);

			// 取得判定が発生した場合、アイテムの取得演出を再生し、アイテムを消費する
			if (consumePickedItem)
			{
				// 取得演出を再生し、アイテムを消費する
				itemVisualEffect_.PlayPickup(lastItemEffectDebugInfo_.itemType, item->GetPosition());

				// アイテムの待機演出を停止し、アイテムを消費する
				itemVisualEffect_.StopIdle(*item);

				// アイテムを消費済みにマークし、取得イベントを記録する
				item->MarkCollected();

				// 取得イベントを記録する
				collectedEvents_.push_back(lastItemEffectDebugInfo_.itemType);
			}

			// 取得判定が発生した場合、プレイヤーの弾薬情報を記録する
			lastKnownMagazineAmmo_ = player.GetCurrentWeaponMagazineAmmo();

			// 取得判定が発生した場合、プレイヤーの予備弾薬情報を記録する
			lastKnownReserveAmmo_ = player.GetCurrentWeaponReserveAmmo();

			// 取得判定が発生した場合、プレイヤーの最大予備弾薬情報を記録する
			lastKnownMaxReserveAmmo_ = player.GetCurrentWeaponMaxReserveAmmo();

			// 取得判定が発生した場合、AmmoSmallの予備弾薬回復量を記録する
			lastAmmoSmallReserveRestored_ = (lastItemEffectDebugInfo_.itemType == ItemType::AmmoSmall)
				? std::max(0, lastItemEffectDebugInfo_.reserveAfter - lastItemEffectDebugInfo_.reserveBefore) : 0;

			// 取得判定が発生した場合、アイテム効果適用のデバッグ情報をログに出力する
			LogItemEffectResult();
		}
	}

	// 取得済みまたは寿命切れのアイテムを安全に削除
	RemoveInactiveItems();
}

/// -------------------------------------------------------------
/// 				アイテム効果の適用
/// -------------------------------------------------------------
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

	// 取得判定が発生した場合、プレイヤーのHPと予備弾薬情報を更新する
	if (info.hpAfter == info.hpBefore)
	{
		info.hpAfter = player.GetHP();
	}

	// 取得判定が発生した場合、プレイヤーの予備弾薬情報を更新する
	if (info.reserveAfter == info.reserveBefore)
	{
		info.reserveAfter = player.GetCurrentWeaponReserveAmmo();
	}

	info.maxHp = player.GetMaxHP();
	info.maxReserve = player.GetCurrentWeaponMaxReserveAmmo();
	info.magazineAmmo = player.GetCurrentWeaponMagazineAmmo();

	WeaponSlot::HudSnapshot hud{};

	// プレイヤーのHUD情報を取得し、選択中のスロットの弾薬情報を記録する
	if (player.GetWeaponSlotHUD(hud) && hud.selectedIndex >= 0 && hud.selectedIndex < WeaponSlot::kSlotCount)
	{
		const auto& selected = hud.slotStates[hud.selectedIndex];
		info.hudMagazineAmmo = selected.ammoInfo.currentAmmo;
		info.hudReserveAmmo = selected.ammoInfo.reserveAmmo;
	}

	// 効果が適用された場合、失敗理由を「なし」に設定する
	if (info.effectApplied)
	{
		info.failReason = "なし";
	}

	// 効果が適用されたかどうかを返す
	return info.effectApplied;
}

/// -------------------------------------------------------------
///			取得済みまたは寿命切れのアイテムを安全に削除
/// -------------------------------------------------------------
void ItemManager::Clear()
{
	// CollisionManagerに登録されているコライダーを削除
	if (registeredCollisionManager_)
	{
		// CollisionManagerに登録されているコライダーを削除
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

/// -------------------------------------------------------------
///					取得済みのアイテムを消費する
/// -------------------------------------------------------------
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

/// -------------------------------------------------------------
///				現在アクティブなアイテムの数を返す
/// -------------------------------------------------------------
int ItemManager::GetActiveItemCount() const
{
	return static_cast<int>(std::count_if(items_.begin(), items_.end(), [](const std::unique_ptr<Item>& item) {
		return item && item->IsActive();
		}));
}

/// -------------------------------------------------------------
///			現在アクティブな指定種類のアイテムの数を返す
/// -------------------------------------------------------------
int ItemManager::GetActiveItemCount(ItemType type) const
{
	return static_cast<int>(std::count_if(items_.begin(), items_.end(), [type](const std::unique_ptr<Item>& item) {
		return item && item->IsActive() && item->GetType() == type;
		}));
}

/// -------------------------------------------------------------
///					Noneドロップ確率を返す
/// -------------------------------------------------------------
float ItemManager::GetNoneDropChance() const
{
	return std::max(0.0f, 1.0f - std::max(0.0f, healDropChance_) - std::max(0.0f, ammoDropChance_));
}

/// -------------------------------------------------------------
/// 		アイテム管理のデバッグ情報をImGuiで表示する
/// -------------------------------------------------------------
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

/// -------------------------------------------------------------
/// 		取得済みまたは寿命切れのアイテムを安全に削除
/// -------------------------------------------------------------
void ItemManager::RemoveInactiveItems()
{
	items_.erase(std::remove_if(items_.begin(), items_.end(), [this](const std::unique_ptr<Item>& item) {
		const bool shouldRemove = !item || item->IsCollected() || item->IsExpired();

		// 取得済みまたは寿命切れのアイテムを削除する前に、待機演出を停止し、CollisionManagerからコライダーを削除する
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

/// -------------------------------------------------------------
/// 				敵の死亡位置にアイテムを落とす
/// -------------------------------------------------------------
void ItemManager::SpawnDropItem(ItemType type, const K4E::Vector3& position)
{
	SpawnConfigured(type, position);
}

/// -------------------------------------------------------------
/// 				アイテムスポーンの共通処理
/// -------------------------------------------------------------
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

/// -------------------------------------------------------------
/// 					視覚設定を適用する
/// -------------------------------------------------------------
void ItemManager::ApplyVisualSettings(Item& item)
{
	const auto& settings = itemVisualEffect_.GetSettings();
	item.SetVisualAnimationSettings(settings.itemFloatHeight, settings.itemFloatSpeed, settings.itemRotationSpeed);
	item.SetVisualColor(itemVisualEffect_.GetEffectColor(item.GetType()));
}

/// -------------------------------------------------------------
/// 				ドロップ抽選結果をログに出力
/// -------------------------------------------------------------
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
