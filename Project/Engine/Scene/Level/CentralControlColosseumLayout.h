#pragma once

#include "LevelData.h"

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// ローマ式コロッセウムの外観と制御塔の機械設備を組み合わせた、Stage 5専用の円形Boss Arena。
	class CentralControlColosseumLayout final
	{
	public:
		static std::vector<ObjectData> Build(const LevelData& levelData)
		{
			if (!IsTargetLevel(levelData)) return {};

			std::vector<ObjectData> objects;
			objects.reserve(340u);

			BuildArenaFloor(objects);
			BuildOuterWall(objects);
			BuildSpectatorTiers(objects);
			BuildColumnArcades(objects);
			BuildControlRing(objects);
			BuildImperialGate(objects);
			return objects; // 床・外壁・観客席・列柱・制御装置を同一Cubeモデルへ集約し、1つのInstance Batchで描画する。
		}

	private:
		static constexpr float kPi = 3.14159265358979323846f;

		static bool IsTargetLevel(const LevelData& levelData)
		{
			for (const ObjectData& data : levelData.objects)
			{
				if (data.name == "CentralControlColosseum_InstancedOnlyMarker") return true;
			}
			return false;
		}

		static Vector3 PolarPosition(float radius, float angle, float y)
		{
			return { std::sin(angle) * radius, y, std::cos(angle) * radius };
		}

		static void BuildArenaFloor(std::vector<ObjectData>& objects)
		{
			AddBox(objects, "ControlArenaFloor_Center", { 0.0f, -0.45f, 0.0f }, { 19.0f, 0.45f, 19.0f }, "Floor");

			constexpr int kFloorSegments = 24;
			for (int index = 0; index < kFloorSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kFloorSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlArenaFloor_Outer_" + std::to_string(index + 1),
					PolarPosition(24.0f, angle, -0.45f),
					{ 3.45f, 0.45f, 6.2f },
					"Floor",
					{ 0.0f, angle, 0.0f });
			}

			AddBox(objects, "ControlArenaDais_Center", { 0.0f, 0.18f, 0.0f }, { 6.8f, 0.18f, 6.8f }, "Floor");
			constexpr int kDaisSegments = 16;
			for (int index = 0; index < kDaisSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kDaisSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlGlow_DaisRing_" + std::to_string(index + 1),
					PolarPosition(8.1f, angle, 0.08f),
					{ 1.55f, 0.04f, 0.28f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}

			constexpr int kArenaGuideSegments = 32;
			for (int index = 0; index < kArenaGuideSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kArenaGuideSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlGlow_ArenaRing_" + std::to_string(index + 1),
					PolarPosition(29.0f, angle, 0.06f),
					{ 1.25f, 0.035f, 0.16f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildOuterWall(std::vector<ObjectData>& objects)
		{
			constexpr int kWallSegments = 48;
			for (int index = 0; index < kWallSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kWallSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ColosseumOuterWall_" + std::to_string(index + 1),
					PolarPosition(47.0f, angle, 6.0f),
					{ 3.25f, 6.0f, 1.35f },
					"Obstacle",
					{ 0.0f, angle, 0.0f });
			}

			constexpr int kCrownSegments = 48;
			for (int index = 0; index < kCrownSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kCrownSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ColosseumCrownRing_" + std::to_string(index + 1),
					PolarPosition(47.0f, angle, 12.8f),
					{ 3.35f, 0.42f, 1.65f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildSpectatorTiers(std::vector<ObjectData>& objects)
		{
			struct TierSpec
			{
				float radius;
				float y;
				float halfHeight;
				float radialHalf;
			};
			const std::array<TierSpec, 3> tiers = {{
				{ 33.5f, 1.25f, 1.25f, 2.8f },
				{ 38.5f, 2.75f, 2.75f, 2.8f },
				{ 43.5f, 4.50f, 4.50f, 2.8f }
			}};

			constexpr int kTierSegments = 32;
			for (size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex)
			{
				for (int index = 0; index < kTierSegments; ++index)
				{
					const float angle = static_cast<float>(index) / static_cast<float>(kTierSegments) * kPi * 2.0f;
					AddBox(
						objects,
						"ColosseumSeatTier_" + std::to_string(tierIndex + 1) + "_" + std::to_string(index + 1),
						PolarPosition(tiers[tierIndex].radius, angle, tiers[tierIndex].y),
						{ 3.8f, tiers[tierIndex].halfHeight, tiers[tierIndex].radialHalf },
						"Obstacle",
						{ 0.0f, angle, 0.0f });
				}
			}
		}

		static void BuildColumnArcades(std::vector<ObjectData>& objects)
		{
			constexpr int kColumnCount = 24;
			for (int index = 0; index < kColumnCount; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kColumnCount) * kPi * 2.0f;
				const Vector3 columnPosition = PolarPosition(43.7f, angle, 10.2f);
				AddBox(
					objects,
					"ColosseumColumn_" + std::to_string(index + 1),
					columnPosition,
					{ 0.80f, 5.7f, 0.80f },
					"Pillar");
				AddBox(
					objects,
					"ColosseumCapital_" + std::to_string(index + 1),
					columnPosition + Vector3{ 0.0f, 6.15f, 0.0f },
					{ 1.25f, 0.34f, 1.25f });
				AddBox(
					objects,
					"ColosseumArcLintel_" + std::to_string(index + 1),
					PolarPosition(43.7f, angle + kPi / static_cast<float>(kColumnCount), 16.35f),
					{ 5.45f, 0.38f, 0.62f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildControlRing(std::vector<ObjectData>& objects)
		{
			constexpr int kControlPylonCount = 8;
			for (int index = 0; index < kControlPylonCount; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kControlPylonCount) * kPi * 2.0f;
				const Vector3 pylonPosition = PolarPosition(30.8f, angle, 3.6f);
				AddBox(
					objects,
					"ControlPylon_" + std::to_string(index + 1),
					pylonPosition,
					{ 1.15f, 3.6f, 1.15f },
					"Pillar",
					{ 0.0f, angle, 0.0f });
				AddBox(
					objects,
					"ControlGlow_PylonCore_" + std::to_string(index + 1),
					pylonPosition + Vector3{ 0.0f, 0.4f, 0.0f },
					{ 0.36f, 2.5f, 0.36f });
				AddBox(
					objects,
					"ControlPanel_PylonHead_" + std::to_string(index + 1),
					pylonPosition + Vector3{ 0.0f, 4.15f, 0.0f },
					{ 1.55f, 0.28f, 1.55f });
			}

			constexpr int kUpperRingSegments = 32;
			for (int index = 0; index < kUpperRingSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kUpperRingSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlUpperRing_" + std::to_string(index + 1),
					PolarPosition(31.0f, angle, 8.0f),
					{ 3.25f, 0.22f, 0.34f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildImperialGate(std::vector<ObjectData>& objects)
		{
			AddBox(objects, "ColosseumImperialGate_Left", { -8.5f, 7.0f, 43.5f }, { 2.2f, 7.0f, 2.1f }, "Obstacle");
			AddBox(objects, "ColosseumImperialGate_Right", { 8.5f, 7.0f, 43.5f }, { 2.2f, 7.0f, 2.1f }, "Obstacle");
			AddBox(objects, "ColosseumImperialGate_Lintel", { 0.0f, 14.0f, 43.5f }, { 10.7f, 1.1f, 2.1f }, "Obstacle");
			AddBox(objects, "ControlGlow_ImperialGate", { 0.0f, 10.8f, 41.25f }, { 5.8f, 0.28f, 0.16f });

			AddBox(objects, "ColosseumEntryGate_Left", { -7.0f, 4.5f, -43.5f }, { 1.4f, 4.5f, 2.0f }, "Obstacle");
			AddBox(objects, "ColosseumEntryGate_Right", { 7.0f, 4.5f, -43.5f }, { 1.4f, 4.5f, 2.0f }, "Obstacle");
			AddBox(objects, "ColosseumEntryGate_Lintel", { 0.0f, 9.2f, -43.5f }, { 8.4f, 0.7f, 2.0f }, "Obstacle");
			AddBox(objects, "ControlGlow_EntryGuide", { 0.0f, 0.06f, -32.0f }, { 2.2f, 0.035f, 7.0f });
		}

		static void AddBox(
			std::vector<ObjectData>& objects,
			std::string name,
			const Vector3& position,
			const Vector3& scale,
			const char* collisionType = nullptr,
			const Vector3& rotation = {})
		{
			ObjectData data{};
			data.name = std::move(name);
			data.type = "StaticMesh";
			data.modelName = "Sample/cube.gltf";
			data.position = position;
			data.rotation = rotation;
			data.scale = scale;
			if (collisionType)
			{
				data.collider.enabled = true;
				data.collider.type = "BOX";
				data.collider.size = { 2.0f, 2.0f, 2.0f };
				data.collider.collisionType = collisionType;
			}
			objects.push_back(std::move(data));
		}
	};
} // namespace Ken4lowEngine
