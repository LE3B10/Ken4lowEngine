#pragma once

#include "LevelData.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// 崩落都市圏の陥没区画へ、進行不能を防ぐ中央仮設橋を追加するレイアウト定義。
	class CollapsedCityPassageLayout final
	{
	public:
		static std::vector<ObjectData> Build(const LevelData& levelData)
		{
			if (!IsTargetLevel(levelData)) return {};

			std::vector<ObjectData> objects;
			objects.reserve(20u);

			struct SlabSpec
			{
				Vector3 position;
				Vector3 scale;
				float yaw;
			};

			const std::array<SlabSpec, 6> slabs = {{
				{ { 0.0f, 0.20f, -52.0f }, { 3.4f, 0.20f, 3.0f }, 0.00f },
				{ { -0.4f, 0.35f, -44.0f }, { 3.2f, 0.30f, 5.0f }, 0.03f },
				{ { 0.5f, 0.50f, -34.0f }, { 3.2f, 0.35f, 5.0f }, -0.04f },
				{ { -0.5f, 0.50f, -24.0f }, { 3.2f, 0.35f, 5.0f }, 0.05f },
				{ { 0.4f, 0.35f, -14.0f }, { 3.2f, 0.30f, 5.0f }, -0.03f },
				{ { 0.0f, 0.20f, -6.0f }, { 3.4f, 0.20f, 3.0f }, 0.00f }
			}};

			for (size_t index = 0; index < slabs.size(); ++index)
			{
				AddBox(
					objects,
					"CityRoad_CollapseBridge_" + std::to_string(index + 1),
					slabs[index].position,
					slabs[index].scale,
					"Floor",
					{ 0.0f, slabs[index].yaw, 0.0f });
			}

			const std::array<Vector3, 6> guidePositions = {{
				{ 0.0f, 0.46f, -52.0f },
				{ -0.4f, 0.71f, -44.0f },
				{ 0.5f, 0.91f, -34.0f },
				{ -0.5f, 0.91f, -24.0f },
				{ 0.4f, 0.71f, -14.0f },
				{ 0.0f, 0.46f, -6.0f }
			}};
			for (size_t index = 0; index < guidePositions.size(); ++index)
			{
				AddBox(
					objects,
					"Strip_CollapseBridgeGuide_" + std::to_string(index + 1),
					guidePositions[index],
					{ 0.45f, 0.03f, 1.2f });
			}

			const std::array<Vector3, 6> brokenRailPositions = {{
				{ -3.25f, 1.05f, -43.5f }, { 3.25f, 1.05f, -43.0f },
				{ -3.25f, 1.20f, -33.5f }, { 3.25f, 1.20f, -23.0f },
				{ -3.25f, 1.05f, -13.5f }, { 3.25f, 0.95f, -13.0f }
			}};
			for (size_t index = 0; index < brokenRailPositions.size(); ++index)
			{
				const float side = brokenRailPositions[index].x < 0.0f ? -1.0f : 1.0f;
				AddBox(
					objects,
					"BrokenBeam_CollapseBridgeRail_" + std::to_string(index + 1),
					brokenRailPositions[index],
					{ 0.16f, 0.18f, 3.8f },
					nullptr,
					{ side * 0.08f, side * 0.04f, side * 0.10f });
			}

			return objects; // 中央の静的な橋でまず詰みを防ぎ、後から開通ギミックへ置き換えられるよう独立させる。
		}

	private:
		static bool IsTargetLevel(const LevelData& levelData)
		{
			for (const ObjectData& data : levelData.objects)
			{
				if (data.name == "CollapsedCity_InstancedOnlyMarker") return true;
			}
			return false;
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
			data.scale = scale;
			data.rotation = rotation;
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
}
