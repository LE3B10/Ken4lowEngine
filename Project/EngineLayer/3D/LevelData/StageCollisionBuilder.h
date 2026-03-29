#pragma once
#include "AABB.h"
#include "Collider.h"
#include "LevelData.h"

#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	struct StageCollisionBuildResult
	{
		std::vector<AABB> worldAABBs;
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