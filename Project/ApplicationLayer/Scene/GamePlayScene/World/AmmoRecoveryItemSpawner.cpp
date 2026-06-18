#define NOMINMAX
#include "AmmoRecoveryItemSpawner.h"

#include "ItemManager.h"
#include "Player.h"
#include "Stage.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace
{
	float DistanceSqXZ(const K4E::Vector3& a, const K4E::Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return dx * dx + dz * dz;
	}

	bool IsUsableFloor(const K4E::AABB& floor)
	{
		const float sizeX = floor.max.x - floor.min.x;
		const float sizeZ = floor.max.z - floor.min.z;
		// 小さすぎる床は壁上や装飾の可能性があるため、スポーン候補から外す。
		return sizeX >= 2.0f && sizeZ >= 2.0f;
	}
}

void AmmoRecoveryItemSpawner::Initialize()
{
	Reset();
}

void AmmoRecoveryItemSpawner::Reset()
{
	// リトライやWorld再生成時に、前回ステージのタイマーと床候補巡回を持ち越さない。
	spawnTimerSec_ = 0.0f;
	nextFloorIndex_ = 0;
	lastSpawnPosition_ = {};
	lastSpawnSucceeded_ = false;
	lastSuppressed_ = false;
}

void AmmoRecoveryItemSpawner::Update(float deltaTime, Player* player, ItemManager& itemManager, const K4E::Stage* stage, bool suppressNewSpawn)
{
	lastSuppressed_ = suppressNewSpawn;
	if (suppressNewSpawn || !player)
	{
		// チュートリアルやボス登場演出など、進行を止めたい場面では新規スポーンタイマーも止める。
		return;
	}

	if (!ShouldSpawnForPlayer(*player))
	{
		// 弾薬が十分ある間はタイマーを戻し、使い切った直後の即時スポーンを避ける。
		spawnTimerSec_ = 0.0f;
		return;
	}

	const int maxActiveCount = std::max(0, settings_.maxActiveCount);
	if (itemManager.GetActiveItemCount(ItemType::AmmoSmall) >= maxActiveCount)
	{
		// 既存AmmoSmall数で上限を見て、時間スポーンが弾薬箱を増やし続けないようにする。
		return;
	}

	spawnTimerSec_ += deltaTime;
	if (spawnTimerSec_ < std::max(0.1f, settings_.spawnIntervalSec))
	{
		return;
	}

	K4E::Vector3 spawnPosition{};
	if (!TryFindSpawnPosition(*player, stage, spawnPosition))
	{
		return;
	}

	const int recoveryAmount = ResolveRecoveryAmount(*player);
	if (recoveryAmount <= 0)
	{
		return;
	}

	// 生成・取得・Collider管理は既存ItemManagerに任せ、スポナーは出現判断だけに集中する。
	itemManager.SpawnAmmoSmall(spawnPosition, recoveryAmount);
	lastSpawnPosition_ = spawnPosition;
	lastSpawnSucceeded_ = true;
	spawnTimerSec_ = 0.0f;
}

bool AmmoRecoveryItemSpawner::ShouldSpawnForPlayer(const Player& player) const
{
	if (!player.CanCurrentWeaponRecoverAmmo())
	{
		return false;
	}

	const int maxReserveAmmo = player.GetCurrentWeaponMaxReserveAmmo();
	if (maxReserveAmmo <= 0)
	{
		return false;
	}

	// 予備弾薬が満タンなら詰み防止の支援は不要なので、スポーンを抑制する。
	return player.GetCurrentWeaponReserveAmmo() < maxReserveAmmo;
}

int AmmoRecoveryItemSpawner::ResolveRecoveryAmount(const Player& player) const
{
	if (settings_.recoveryAmountOverride > 0)
	{
		return settings_.recoveryAmountOverride;
	}

	// 既定値は「現在武器のマガジン1個分」とし、武器ごとの差を自然に反映する。
	return std::max(1, player.GetCurrentWeaponMagazineCapacity());
}

bool AmmoRecoveryItemSpawner::TryFindSpawnPosition(const Player& player, const K4E::Stage* stage, K4E::Vector3& outPosition)
{
	const K4E::Vector3 playerPosition = player.GetCenterPosition();
	if (stage && TryFindFloorSpawnPosition(playerPosition, stage->GetFloorAABBs(), outPosition))
	{
		return true;
	}

	// 床AABBが未設定のステージでも弾切れ救済が止まらないよう、前方固定位置へフォールバックする。
	outPosition = MakeFallbackSpawnPosition(playerPosition);
	return true;
}

bool AmmoRecoveryItemSpawner::TryFindFloorSpawnPosition(const K4E::Vector3& playerPosition, const std::vector<K4E::AABB>& floorAABBs, K4E::Vector3& outPosition)
{
	if (floorAABBs.empty())
	{
		return false;
	}

	const float minDistanceSq = settings_.minDistanceFromPlayer * settings_.minDistanceFromPlayer;
	float farthestDistanceSq = -1.0f;
	K4E::Vector3 farthestPosition{};
	bool hasFallback = false;

	for (std::size_t i = 0; i < floorAABBs.size(); ++i)
	{
		const std::size_t index = (nextFloorIndex_ + i) % floorAABBs.size();
		const K4E::AABB& floor = floorAABBs[index];
		if (!IsUsableFloor(floor))
		{
			continue;
		}

		// 床AABB中央を候補にし、プレイヤー周辺ではなくステージ上の安全な床に置く。
		K4E::Vector3 candidate{
			(floor.min.x + floor.max.x) * 0.5f,
			floor.max.y + settings_.spawnHeightOffset,
			(floor.min.z + floor.max.z) * 0.5f
		};

		const float distanceSq = DistanceSqXZ(candidate, playerPosition);
		if (distanceSq > farthestDistanceSq)
		{
			farthestDistanceSq = distanceSq;
			farthestPosition = candidate;
			hasFallback = true;
		}
		if (distanceSq >= minDistanceSq)
		{
			outPosition = candidate;
			nextFloorIndex_ = (index + 1) % floorAABBs.size();
			return true;
		}
	}

	if (hasFallback)
	{
		// 近距離候補しか無い小ステージでは、最も遠い床を選んで詰み防止を優先する。
		outPosition = farthestPosition;
		return true;
	}

	return false;
}

K4E::Vector3 AmmoRecoveryItemSpawner::MakeFallbackSpawnPosition(const K4E::Vector3& playerPosition) const
{
	// 床情報がない場合でもプレイヤー直近には置かず、少し離れた固定方向に出す。
	return {
		playerPosition.x + settings_.minDistanceFromPlayer,
		playerPosition.y,
		playerPosition.z + settings_.minDistanceFromPlayer
	};
}

void AmmoRecoveryItemSpawner::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Ammo Recovery Spawner", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	ImGui::DragFloat("Spawn Interval Sec", &settings_.spawnIntervalSec, 0.1f, 0.1f, 120.0f, "%.1f");
	ImGui::DragInt("Max Active Ammo Items", &settings_.maxActiveCount, 1.0f, 0, 20);
	ImGui::DragInt("Recovery Override(0=Magazine)", &settings_.recoveryAmountOverride, 1.0f, 0, 999);
	ImGui::DragFloat("Min Distance From Player", &settings_.minDistanceFromPlayer, 0.5f, 0.0f, 80.0f, "%.1f");
	ImGui::Text("Timer: %.2f / %.2f", spawnTimerSec_, settings_.spawnIntervalSec);
	ImGui::Text("Last Suppressed: %s", lastSuppressed_ ? "true" : "false");
	ImGui::Text("Last Spawn: %s (%.2f, %.2f, %.2f)",
		lastSpawnSucceeded_ ? "true" : "false",
		lastSpawnPosition_.x,
		lastSpawnPosition_.y,
		lastSpawnPosition_.z);
#else
	(void)this;
#endif
}
