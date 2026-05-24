#pragma once
#include "AABB.h"
#include "Collider.h"
#include "LevelData.h"

#include <memory>
#include <array>
#include <vector>

namespace Ken4lowEngine
{
	struct StageObstacleBox
	{
			std::string name;
			std::string collisionTypeName;
			int collisionTypeId = -1;
			Vector3 center{};
			Vector3 halfSize{};
			Vector3 axisX{ 1.0f, 0.0f, 0.0f };
			Vector3 axisY{ 0.0f, 1.0f, 0.0f };
			Vector3 axisZ{ 0.0f, 0.0f, 1.0f };
			std::array<Vector3, 8> corners{};
			AABB enclosingAABB{};
	};

	struct StageCollisionBuildResult
	{
		std::vector<Ken4lowEngine::AABB> worldAABBs;
		std::vector<Ken4lowEngine::AABB> worldAABBsLegacy;
		std::vector<Ken4lowEngine::AABB> floorAABBs;
		std::vector<Ken4lowEngine::AABB> wallObstacleAABBs;
		std::vector<Ken4lowEngine::AABB> navigationObstacleAABBs;
		std::vector<StageObstacleBox> obstacleBoxes;
		std::vector<std::unique_ptr<Collider>> worldColliders;
	};

	/// -------------------------------------------------------------
	///				ステージ衝突生成
	/// -------------------------------------------------------------
	class StageCollisionBuilder
	{
	public:
		/// <summary>
		/// LevelData からワールド衝突情報を構築する
		/// </summary>
		static StageCollisionBuildResult Build(const LevelData& levelData, const Vector3& offset);
	};
} // namespace Ken4lowEngine
