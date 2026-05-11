#include "EnemyHPBarManager.h"
#include "EnemyBase.h"

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
		if (entry.enemy == enemy)
		{
			return &entry;
		}
	}
	return nullptr;
}

EnemyHPBarManager::Entry& EnemyHPBarManager::FindOrCreateEntry(EnemyBase* enemy)
{
	if (Entry* found = FindEntry(enemy))
	{
		return *found;
	}

	Entry entry{};
	entry.enemy = enemy;
	entry.bar = std::make_unique<EnemyHPBar>();
	entry.bar->Initialize();

	if (enemy)
	{
		entry.cachedWorldPos = enemy->GetHpBarWorldPosition();
	}

	entries_.push_back(std::move(entry));
	return entries_.back();
}

void EnemyHPBarManager::RemoveDeadEntries()
{
	entries_.erase(
		std::remove_if(
			entries_.begin(),
			entries_.end(),
			[](const Entry& entry)
			{
				return entry.removeRequested;
			}),
		entries_.end());
}

void EnemyHPBarManager::Update(
	const std::vector<EnemyBase*>& enemies,
	const K4E::Matrix4x4& viewMatrix,
	const K4E::Matrix4x4& projMatrix,
	float screenWidth,
	float screenHeight,
	float deltaTime)
{
	// まず全Entryを未更新にする
	for (auto& entry : entries_)
	{
		entry.updatedThisFrame = false;
		entry.visibleThisFrame = false;
	}

	// 今いる敵を更新
	for (EnemyBase* enemy : enemies)
	{
		if (enemy == nullptr)
		{
			continue;
		}

		Entry& entry = FindOrCreateEntry(enemy);
		entry.updatedThisFrame = true;

		// 最後の位置を保存しておく
		entry.cachedWorldPos = enemy->GetHpBarWorldPosition();

		// 射影
		HpBarProjectResult proj = ProjectWorldToScreen(
			entry.cachedWorldPos,
			viewMatrix,
			projMatrix,
			screenWidth,
			screenHeight);

		// スクリーン位置は見えている時だけ更新
		if (proj.inFront && proj.inScreen)
		{
			entry.cachedScreenPos = proj.screenPos;
			entry.cachedScreenPos.y -= 18.0f;
			entry.visibleThisFrame = true;
		}

		// 生存中は通常HP、死亡したら 0 に向けて減らす
		float hpRate = 0.0f;
		if (!enemy->IsDead() && enemy->GetMaxHp() > 0)
		{
			hpRate = static_cast<float>(enemy->GetHp()) /
				static_cast<float>(enemy->GetMaxHp());
		}
		else
		{
			entry.deathStarted = true;
			hpRate = 0.0f;
		}

		// 画面内にいる間だけ表示更新
		// 死亡後も最後のスクリーン位置で描き続ける
		const bool visible = entry.visibleThisFrame || entry.deathStarted;

		entry.bar->Update(
			entry.cachedScreenPos,
			hpRate,
			visible,
			deltaTime,
			72.0f,
			8.0f);
	}

	// 今フレーム敵一覧にいなかったEntryの処理
	for (auto& entry : entries_)
	{
		if (entry.updatedThisFrame)
		{
			continue;
		}

		if (!entry.bar)
		{
			entry.removeRequested = true;
			continue;
		}

		// 敵本体はもう一覧から消えたが、死亡済みならバーだけ続行
		if (entry.deathStarted)
		{
			entry.bar->Update(
				entry.cachedScreenPos,
				0.0f,
				true,
				deltaTime,
				72.0f,
				8.0f);
		}
		else
		{
			// 生存中なのに突然消えたなら消してよい
			entry.bar->SetVisible(false);
			entry.removeRequested = true;
		}
	}

	// アニメーションが終わったEntryを消す
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
	for (auto& entry : entries_)
	{
		if (entry.bar)
		{
			entry.bar->Draw();
		}
	}
}

void EnemyHPBarManager::DrawImGuiContent() const
{
#ifdef USE_IMGUI
	// Enemy Debug内でHPBar管理状態を確認できるよう軽量な統計だけ表示する。
	ImGui::Text("HPBar Entries: %d", static_cast<int>(entries_.size()));
	int visibleCount = 0;
	int deathStartedCount = 0;
	for (const auto& entry : entries_)
	{
		if (entry.visibleThisFrame) { ++visibleCount; }
		if (entry.deathStarted) { ++deathStartedCount; }
	}
	ImGui::Text("Visible This Frame: %d", visibleCount);
	ImGui::Text("Death Animations: %d", deathStartedCount);
#endif
}
