#pragma once

#include <vector>
#include <memory>
#include <algorithm>

#include "EnemyHPBar.h"
#include "EnemyHPBarProjector.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

class EnemyBase;

namespace K4E = ::Ken4lowEngine;

// 複数の敵HPバーをまとめて管理するクラス
class EnemyHPBarManager
{
public:
	void Initialize();

	void Update(
		const std::vector<EnemyBase*>& enemies,
		const K4E::Matrix4x4& viewMatrix,
		const K4E::Matrix4x4& projMatrix,
		float screenWidth,
		float screenHeight,
		float deltaTime);

	void Draw();
	void DrawImGuiContent() const;

private:
	struct Entry
	{
		EnemyBase* enemy = nullptr;
		std::unique_ptr<EnemyHPBar> bar;

		K4E::Vector3 cachedWorldPos{ 0.0f, 0.0f, 0.0f };
		K4E::Vector2 cachedScreenPos{ 0.0f, 0.0f };

		bool visibleThisFrame = false;
		bool updatedThisFrame = false;
		bool deathStarted = false;
		bool removeRequested = false;
	};

private:
	Entry* FindEntry(EnemyBase* enemy);
	Entry& FindOrCreateEntry(EnemyBase* enemy);
	void RemoveDeadEntries();

private:
	std::vector<Entry> entries_;
};