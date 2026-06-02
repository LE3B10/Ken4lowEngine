#define NOMINMAX
#include "CrystalManager.h"

#include "CharacterWorld.h"
#include "CameraManager.h"
#include "CollisionManager.h"
#include "EnemyBase.h"

#include <algorithm>
#include <iostream>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr float kMinimumSpawnInterval = 0.05f;
}

void CrystalManager::Initialize(const std::vector<CrystalSpawnPoint>& spawnPoints, CollisionManager* collisionManager, const std::vector<Ken4lowEngine::AABB>* floorAABBs, const std::vector<Ken4lowEngine::AABB>* obstacleAABBs)
{
	if (collisionManager_)
	{
		for (EnemySpawnCrystal& crystal : crystals_)
		{
			collisionManager_->RemoveCollider(&crystal);
		}
	}

	crystals_.clear();
	collisionManager_ = collisionManager;
	crystals_.reserve(spawnPoints.size());
	for (const CrystalSpawnPoint& spawnPoint : spawnPoints)
	{
		EnemySpawnCrystal crystal;
		crystal.Initialize(spawnPoint, floorAABBs, obstacleAABBs);
		crystals_.push_back(std::move(crystal));
	}

	if (collisionManager_)
	{
		for (EnemySpawnCrystal& crystal : crystals_)
		{
			collisionManager_->AddCollider(&crystal);
		}
	}

	selectedCrystalIndex_ = 0;
	nextSpawnCrystalIndex_ = 0;
	enableCrystalEnemySpawn_ = true;
	maxTotalCrystalSpawnEnemies_ = 9;
	globalSpawnInterval_ = 5.0f;
	globalSpawnTimer_ = 0.0f;
	maxSpawnPerInterval_ = 1;
	shouldSpawnBoss_ = false;
	hasBossSpawned_ = false;
}

void CrystalManager::Update(CharacterWorld& characters, float deltaTime)
{
	for (EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.Update(characters);
	}

	NotifyBossSpawnIfNeeded();
	if (!enableCrystalEnemySpawn_ || AreAllCrystalsDestroyed() || GetAliveCrystalCount() <= 0 ||
		GetAliveCrystalSpawnEnemyCount() >= maxTotalCrystalSpawnEnemies_)
	{
		globalSpawnTimer_ = 0.0f;
		return;
	}

	// フレーム落ち時にスポーン更新が一気に進まないよう、Crystal更新用deltaTimeを制限する。
	const float safeDeltaTime = std::clamp(deltaTime, 0.0f, kMaxUpdateDeltaTime);
	globalSpawnTimer_ += safeDeltaTime;
	if (globalSpawnTimer_ < globalSpawnInterval_)
	{
		return;
	}

	globalSpawnTimer_ = 0.0f;
	// 敵が増えすぎないよう、クリスタル由来の生存敵数がステージ全体の上限未満のときだけ補充する。
	for (int spawnedCount = 0; spawnedCount < maxSpawnPerInterval_ &&
		GetAliveCrystalSpawnEnemyCount() < maxTotalCrystalSpawnEnemies_; ++spawnedCount)
	{
		EnemySpawnCrystal* crystal = FindNextSpawnableCrystal();
		if (!crystal)
		{
			break;
		}
		crystal->SpawnEnemy(characters);
	}
}

void CrystalManager::Draw() const
{
	for (const EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.Draw();
	}
}

int CrystalManager::GetAliveCrystalCount() const
{
	return static_cast<int>(std::count_if(crystals_.begin(), crystals_.end(),
		[](const EnemySpawnCrystal& crystal) { return crystal.IsAlive(); }));
}

int CrystalManager::GetAliveCrystalSpawnEnemyCount() const
{
	int totalCount = 0;
	for (const EnemySpawnCrystal& crystal : crystals_)
	{
		totalCount += crystal.GetAliveSpawnedEnemyCount();
	}
	return totalCount;
}

bool CrystalManager::AreAllCrystalsDestroyed() const
{
	return !crystals_.empty() && GetAliveCrystalCount() == 0;
}

void CrystalManager::NotifyBossSpawnIfNeeded()
{
	if (!AreAllCrystalsDestroyed() || hasBossSpawned_)
	{
		return;
	}

	shouldSpawnBoss_ = true;
	hasBossSpawned_ = true;
	// 既存ボス進行へ安全に接続するまでは、全破壊時に一度だけ仮通知する。
	std::clog << "[CrystalManager] すべてのクリスタルが破壊されました。ボスを出現させます。\n";
}

EnemySpawnCrystal* CrystalManager::GetSelectedCrystal()
{
	return selectedCrystalIndex_ < crystals_.size() ? &crystals_[selectedCrystalIndex_] : nullptr;
}

const EnemySpawnCrystal* CrystalManager::GetSelectedCrystal() const
{
	return selectedCrystalIndex_ < crystals_.size() ? &crystals_[selectedCrystalIndex_] : nullptr;
}

EnemySpawnCrystal* CrystalManager::FindNextSpawnableCrystal()
{
	for (size_t offset = 0; offset < crystals_.size(); ++offset)
	{
		const size_t crystalIndex = (nextSpawnCrystalIndex_ + offset) % crystals_.size();
		if (crystals_[crystalIndex].CanSpawnEnemy())
		{
			nextSpawnCrystalIndex_ = (crystalIndex + 1) % crystals_.size();
			return &crystals_[crystalIndex];
		}
	}
	return nullptr;
}

void CrystalManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("クリスタル デバッグ"))
	{
		return;
	}

	ImGui::Checkbox("クリスタル敵スポーン有効", &enableCrystalEnemySpawn_);
	ImGui::DragInt("クリスタル由来の最大敵数", &maxTotalCrystalSpawnEnemies_, 1.0f, 0, 100);
	maxTotalCrystalSpawnEnemies_ = std::max(0, maxTotalCrystalSpawnEnemies_);
	ImGui::Text("現在のクリスタル由来敵数: %d", GetAliveCrystalSpawnEnemyCount());
	ImGui::DragFloat("スポーン間隔", &globalSpawnInterval_, 0.1f, kMinimumSpawnInterval, 60.0f, "%.2f 秒");
	globalSpawnInterval_ = std::max(kMinimumSpawnInterval, globalSpawnInterval_);
	ImGui::Text("スポーンタイマー: %.2f / %.2f 秒", globalSpawnTimer_, globalSpawnInterval_);
	ImGui::DragInt("1回の最大スポーン数", &maxSpawnPerInterval_, 1.0f, 1, 9);
	maxSpawnPerInterval_ = std::max(1, maxSpawnPerInterval_);
	ImGui::Text("クリスタル数: %d", GetCrystalCount());
	ImGui::Text("生存クリスタル数: %d", GetAliveCrystalCount());
	ImGui::Text("全クリスタル破壊済み: %s", AreAllCrystalsDestroyed() ? "はい" : "いいえ");
	ImGui::Text("ボス出現済み: %s", HasBossSpawned() ? "はい" : "いいえ");
	ImGui::Text("更新用deltaTime上限: %.4f 秒", kMaxUpdateDeltaTime);
	ImGui::Text("敵Ground Snap有効: %s", EnemyBase::IsGroundSnapEnabled() ? "はい" : "いいえ");
	ImGui::Text("クリスタルGround Snap有効: %s", EnemySpawnCrystal::IsGroundSnapEnabled() ? "はい" : "いいえ");
	ImGui::Text("最大押し出し量: %.2f", EnemyBase::GetMaxPushOutPerFrame());
	ImGui::Text("敵の場外制限有効: %s", EnemyBase::IsWorldBoundsEnabled() ? "はい" : "いいえ");

	if (crystals_.empty())
	{
		ImGui::Text("クリスタル設定がありません。");
		return;
	}

	int selectedIndex = static_cast<int>(selectedCrystalIndex_);
	if (ImGui::SliderInt("選択中クリスタル", &selectedIndex, 0, static_cast<int>(crystals_.size()) - 1))
	{
		selectedCrystalIndex_ = static_cast<size_t>(selectedIndex);
	}

	EnemySpawnCrystal* crystal = GetSelectedCrystal();
	if (!crystal)
	{
		return;
	}

	bool enableInfiniteSpawn = crystal->IsInfiniteSpawnEnabled();
	if (ImGui::Checkbox("選択中クリスタルの敵スポーン有効", &enableInfiniteSpawn))
	{
		crystal->SetInfiniteSpawnEnabled(enableInfiniteSpawn);
	}

	constexpr const char* enemyTypeLabels[] = { "旧Enemy", "近接雑魚敵", "中距離雑魚敵" };
	int enemyTypeIndex = static_cast<int>(crystal->GetSpawnEnemyType());
	if (ImGui::Combo("出現敵タイプ", &enemyTypeIndex, enemyTypeLabels, IM_ARRAYSIZE(enemyTypeLabels)))
	{
		crystal->SetSpawnEnemyType(static_cast<EnemyType>(enemyTypeIndex));
	}

	int maxAliveEnemies = crystal->GetMaxAliveEnemies();
	if (ImGui::DragInt("選択中クリスタルの同時出現上限", &maxAliveEnemies, 1.0f, 0, 100))
	{
		crystal->SetMaxAliveEnemies(maxAliveEnemies);
	}

	const Ken4lowEngine::Vector3& crystalPosition = crystal->GetPosition();
	const Ken4lowEngine::Vector3& crystalScale = crystal->GetScale();
	const Ken4lowEngine::Vector3 cameraPosition = Ken4lowEngine::CameraManager::GetInstance()->GetActiveCameraPosition();
	ImGui::Text("選択中クリスタル座標: (%.2f, %.2f, %.2f)", crystalPosition.x, crystalPosition.y, crystalPosition.z);
	ImGui::Text("クリスタルScale: (%.2f, %.2f, %.2f)", crystalScale.x, crystalScale.y, crystalScale.z);
	ImGui::Text("選択中クリスタルHP: %d", crystal->GetHp());
	ImGui::Text("生存状態: %s", crystal->IsAlive() ? "生存" : "破壊済み");
	ImGui::Text("クリスタル破壊済み: %s", crystal->IsDestroyed() ? "はい" : "いいえ");
	ImGui::Text("クリスタルCollider有効: %s", crystal->IsColliderEnabled() ? "はい" : "いいえ");
	ImGui::Text("クリスタル被弾回数: %d", crystal->GetHitCount());
	ImGui::Text("カメラ座標: (%.2f, %.2f, %.2f)", cameraPosition.x, cameraPosition.y, cameraPosition.z);
	ImGui::Text("選択中クリスタル由来の生存敵数: %d", crystal->GetAliveSpawnedEnemyCount());
	ImGui::Text("選択中クリスタルの合計スポーン数: %d", crystal->GetTotalSpawnedCount());
	if (ImGui::Button("選択中クリスタルに10ダメージ"))
	{
		crystal->ApplyDamage(10);
	}
	if (ImGui::Button("全クリスタルに10ダメージ"))
	{
		for (EnemySpawnCrystal& target : crystals_)
		{
			target.ApplyDamage(10);
		}
	}
#endif
}
