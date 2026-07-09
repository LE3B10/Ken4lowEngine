#pragma once

#include "BulletEnemyCollisionSoA.h"
#include "Vector3.h"

#include <algorithm>
#include <chrono>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	inline void DrawBulletEnemySoABenchmarkPanel()
	{
#ifdef USE_IMGUI
		static BulletEnemyCollisionSoA collisionSystem;
		static int bulletCount = 1000;
		static int enemyCount = 1000;
		static float cellSize = 2.0f;
		static double lastElapsedMs = 0.0;
		static BulletEnemyCollisionSoA::FrameStats lastStats{};

		if (!ImGui::CollapsingHeader("BulletEnemy SoA Benchmark"))
		{
			return;
		}

		// DebugScene内だけで小規模な弾vs敵のSoA判定を実行し、本編の敵や弾へはまだ接続しない。
		ImGui::DragInt("SoA Bullet Count", &bulletCount, 100.0f, 1, 1000000);
		ImGui::DragInt("SoA Enemy Count", &enemyCount, 100.0f, 1, 1000000);
		ImGui::DragFloat("SoA Cell Size", &cellSize, 0.05f, 0.1f, 20.0f, "%.2f");

		if (ImGui::Button("Run BulletEnemy SoA Benchmark"))
		{
			bulletCount = std::clamp(bulletCount, 1, 1000000);
			enemyCount = std::clamp(enemyCount, 1, 1000000);
			cellSize = std::max(cellSize, 0.1f);

			collisionSystem.ClearFrameData();
			collisionSystem.Reserve(static_cast<size_t>(bulletCount), static_cast<size_t>(enemyCount));

			const auto begin = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < enemyCount; ++i)
			{
				const float x = static_cast<float>(i % 1000);
				const float z = static_cast<float>(i / 1000);
				collisionSystem.AddEnemy(Vector3{ x, 0.0f, z }, 0.5f, 1, true);
			}

			for (int i = 0; i < bulletCount; ++i)
			{
				const float x = static_cast<float>(i % 1000) + 0.1f;
				const float z = static_cast<float>(i / 1000);
				collisionSystem.AddBullet(Vector3{ x, 0.0f, z }, 0.25f, 1, true);
			}

			collisionSystem.Execute(cellSize);
			const auto end = std::chrono::high_resolution_clock::now();
			lastElapsedMs = std::chrono::duration<double, std::milli>(end - begin).count();
			lastStats = collisionSystem.GetLastFrameStats();
		}

		const double theoreticalPairs = static_cast<double>(bulletCount) * static_cast<double>(enemyCount);
		ImGui::Separator();
		ImGui::Text("Theoretical Pairs: %.0f", theoreticalPairs);
		ImGui::Text("Collision Checks: %zu", lastStats.collisionChecks);
		ImGui::Text("Hit Count: %zu", lastStats.hitCount);
		ImGui::Text("Active Bullets: %zu", lastStats.activeBulletCount);
		ImGui::Text("Active Enemies: %zu", lastStats.activeEnemyCount);
		ImGui::Text("Elapsed: %.3f ms", lastElapsedMs);
#endif // USE_IMGUI
	}
}
