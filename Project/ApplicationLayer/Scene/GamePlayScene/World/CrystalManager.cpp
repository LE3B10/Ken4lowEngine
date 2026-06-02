#include "CrystalManager.h"

#include "CharacterWorld.h"

#include <algorithm>
#include <iostream>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void CrystalManager::Initialize(const std::vector<CrystalSpawnPoint>& spawnPoints)
{
	crystals_.clear();
	crystals_.reserve(spawnPoints.size());
	for (const CrystalSpawnPoint& spawnPoint : spawnPoints)
	{
		EnemySpawnCrystal crystal;
		crystal.Initialize(spawnPoint);
		crystals_.push_back(std::move(crystal));
	}

	selectedCrystalIndex_ = 0;
	shouldSpawnBoss_ = false;
	hasBossSpawned_ = false;
}

void CrystalManager::Update(CharacterWorld& characters, float deltaTime)
{
	for (EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.Update(characters, deltaTime);
	}
	NotifyBossSpawnIfNeeded();
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

void CrystalManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("クリスタル デバッグ"))
	{
		return;
	}

	ImGui::Text("クリスタル数: %d", GetCrystalCount());
	ImGui::Text("生存クリスタル数: %d", GetAliveCrystalCount());
	ImGui::Text("全クリスタル破壊済み: %s", AreAllCrystalsDestroyed() ? "はい" : "いいえ");
	ImGui::Text("ボス出現済み: %s", HasBossSpawned() ? "はい" : "いいえ");

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
	if (ImGui::Checkbox("クリスタル敵スポーン有効", &enableInfiniteSpawn))
	{
		crystal->SetInfiniteSpawnEnabled(enableInfiniteSpawn);
	}

	constexpr const char* enemyTypeLabels[] = { "旧Enemy", "近接雑魚敵", "中距離雑魚敵" };
	int enemyTypeIndex = static_cast<int>(crystal->GetSpawnEnemyType());
	if (ImGui::Combo("出現敵タイプ", &enemyTypeIndex, enemyTypeLabels, IM_ARRAYSIZE(enemyTypeLabels)))
	{
		crystal->SetSpawnEnemyType(static_cast<EnemyType>(enemyTypeIndex));
	}

	float spawnInterval = crystal->GetSpawnInterval();
	if (ImGui::DragFloat("敵出現間隔", &spawnInterval, 0.1f, 0.05f, 60.0f, "%.2f 秒"))
	{
		crystal->SetSpawnInterval(spawnInterval);
	}

	int maxAliveEnemies = crystal->GetMaxAliveEnemies();
	if (ImGui::DragInt("同時出現上限", &maxAliveEnemies, 1.0f, 0, 100))
	{
		crystal->SetMaxAliveEnemies(maxAliveEnemies);
	}

	ImGui::Text("現在の生存スポーン敵数: %d", crystal->GetAliveSpawnedEnemyCount());
	ImGui::Text("合計スポーン数: %d", crystal->GetTotalSpawnedCount());
	ImGui::Text("選択中クリスタルHP: %d", crystal->GetHp());
	if (ImGui::Button("選択中クリスタルへダメージ"))
	{
		crystal->TakeDamage(25);
	}
#endif
}
