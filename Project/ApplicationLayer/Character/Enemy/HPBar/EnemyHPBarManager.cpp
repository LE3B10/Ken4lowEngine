#define NOMINMAX
#include "EnemyHPBarManager.h"
#include "EnemyBase.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void EnemyHPBarManager::Initialize()
{
	entries_.clear();
}

EnemyHPBarManager::Entry* EnemyHPBarManager::FindEntry(EnemyBase* enemy)
{
	for (auto& entry : entries_)
	{
		if (entry.enemy == enemy) return &entry;
	}
	return nullptr;
}

EnemyHPBarManager::Entry& EnemyHPBarManager::FindOrCreateEntry(EnemyBase* enemy)
{
	if (Entry* found = FindEntry(enemy)) return *found;

	Entry entry{};
	entry.enemy = enemy;
	entry.usesWorldGauge = dynamic_cast<K4E::EnemyActor*>(enemy) != nullptr;
	entry.bar = std::make_unique<EnemyHPBar>();
	entry.bar->Initialize();
	if (enemy) entry.cachedWorldPos = enemy->GetHpBarWorldPosition();
	entries_.push_back(std::move(entry));
	return entries_.back();
}

void EnemyHPBarManager::RemoveDeadEntries()
{
	entries_.erase(
		std::remove_if(entries_.begin(), entries_.end(), [](const Entry& entry) { return entry.removeRequested; }),
		entries_.end());
}

void EnemyHPBarManager::Update(
	const std::vector<EnemyBase*>& enemies,
	const K4E::Matrix4x4& viewMatrix,
	const K4E::Matrix4x4& projMatrix,
	float screenWidth,
	float screenHeight,
	float deltaTime,
	const EnemyBase* aimedEnemy,
	bool showOnlyWhenAimed,
	float visibleHoldTime)
{
	for (auto& entry : entries_)
	{
		entry.updatedThisFrame = false;
		entry.visibleThisFrame = false;
	}

	for (EnemyBase* enemy : enemies)
	{
		if (!enemy) continue;

		Entry& entry = FindOrCreateEntry(enemy);
		entry.updatedThisFrame = true;
		entry.cachedWorldPos = enemy->GetHpBarWorldPosition();

		const HpBarProjectResult proj = ProjectWorldToScreen(
			entry.cachedWorldPos,
			viewMatrix,
			projMatrix,
			screenWidth,
			screenHeight);
		if (proj.inFront && proj.inScreen)
		{
			entry.cachedScreenPos = proj.screenPos;
			entry.cachedScreenPos.y -= 18.0f;
			entry.visibleThisFrame = true;
		}

		float hpRate = 0.0f;
		if (!enemy->IsDead() && enemy->GetMaxHp() > 0)
		{
			hpRate = static_cast<float>(enemy->GetHp()) / static_cast<float>(enemy->GetMaxHp());
		}
		else
		{
			entry.deathStarted = true;
		}

		if (enemy == aimedEnemy) entry.aimVisibleTimer = std::max(0.0f, visibleHoldTime);
		else if (entry.aimVisibleTimer > 0.0f) entry.aimVisibleTimer = std::max(0.0f, entry.aimVisibleTimer - deltaTime);

		const bool aimVisible = !showOnlyWhenAimed || enemy == aimedEnemy || entry.aimVisibleTimer > 0.0f;
		if (entry.usesWorldGauge)
		{
			static_cast<K4E::EnemyActor*>(enemy)->SetHealthBarVisible(!enemy->IsDead() && aimVisible);
			entry.bar->SetVisible(false); // EnemyActorはWorldGaugeComponentへ描画を統一し、旧Sprite HPバーを二重表示しない。
			continue;
		}

		const bool visible = (entry.visibleThisFrame || entry.deathStarted) && aimVisible;
		entry.bar->Update(entry.cachedScreenPos, hpRate, visible, deltaTime, 72.0f, 8.0f);
	}

	for (auto& entry : entries_)
	{
		if (entry.updatedThisFrame) continue;
		if (!entry.bar)
		{
			entry.removeRequested = true;
			continue;
		}
		if (entry.usesWorldGauge)
		{
			entry.bar->SetVisible(false);
			entry.removeRequested = true; // 実体消滅後は保存済み方式だけを見てEntryを捨て、Enemyポインタへ再アクセスしない。
			continue;
		}
		if (entry.deathStarted)
		{
			entry.bar->Update(entry.cachedScreenPos, 0.0f, !showOnlyWhenAimed || entry.aimVisibleTimer > 0.0f, deltaTime, 72.0f, 8.0f);
		}
		else
		{
			entry.bar->SetVisible(false);
			entry.removeRequested = true;
		}
	}

	for (auto& entry : entries_)
	{
		if (!entry.bar)
		{
			entry.removeRequested = true;
			continue;
		}
		if (entry.deathStarted)
		{
			entry.bar->SetVisible(false);
			entry.removeRequested = true;
		}
	}

	RemoveDeadEntries();
}

void EnemyHPBarManager::Draw()
{
	for (auto& entry : entries_) if (entry.bar) entry.bar->Draw();
}

void EnemyHPBarManager::DrawImGuiContent() const
{
#ifdef USE_IMGUI
	ImGui::Text("HPBar Entries: %d", static_cast<int>(entries_.size()));
	int visibleCount = 0;
	int deathStartedCount = 0;
	for (const auto& entry : entries_)
	{
		if (entry.visibleThisFrame) ++visibleCount;
		if (entry.deathStarted) ++deathStartedCount;
	}
	ImGui::Text("Visible This Frame: %d", visibleCount);
	ImGui::Text("Death Animations: %d", deathStartedCount);
#endif
}
