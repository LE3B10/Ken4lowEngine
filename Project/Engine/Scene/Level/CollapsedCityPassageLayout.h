#pragma once

#include "LevelData.h"

#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// 動的な中央橋を必須ルートにするため、陥没地点の旧迂回路だけを崩落瓦礫で封鎖する。
	class CollapsedCityPassageLayout final
	{
	public:
		static std::vector<ObjectData> Build(const LevelData& levelData)
		{
			if (!IsTargetLevel(levelData)) return {};

			std::vector<ObjectData> objects;
			objects.reserve(3u);
			AddBox(objects, "CityBypassPermanentCollapse_Main", { -16.0f, 2.6f, -51.0f }, { 9.5f, 2.6f, 1.6f }, "Obstacle", { 0.04f, 0.08f, -0.03f });
			AddBox(objects, "Rubble_BypassPermanentCollapse_Left", { -25.0f, 1.55f, -46.5f }, { 3.6f, 1.55f, 2.4f }, "Obstacle", { -0.08f, -0.30f, 0.06f });
			AddBox(objects, "BrokenBeam_BypassPermanentCollapse_Top", { -14.0f, 5.7f, -50.5f }, { 7.5f, 0.35f, 0.45f }, nullptr, { 0.18f, 0.12f, 0.22f });
			return objects; // 左側の旧迂回路を残したまま中央橋を待ててしまう抜け道を塞ぎ、敵全滅まで進行不能にする。
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
			const char* collisionType,
			const Vector3& rotation)
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
} // namespace Ken4lowEngine
