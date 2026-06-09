#pragma once
#include "AABB.h"
#include "Collider.h"
#include "OBB.h"
#include "LevelData.h"

#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	struct StageCollisionBuildResult
	{
		std::vector<AABB> worldAABBs;
		std::vector<AABB> floorAABBs;
		std::vector<AABB> wallObstacleAABBs;
		std::vector<AABB> navigationObstacleAABBs;
		std::vector<std::unique_ptr<Collider>> worldColliders;
		std::vector<OBB> wallObstacleOBBs;
		std::vector<OBB> navigationObstacleOBBs;
	};

	/*
	StageCollisionBuilder責務メモ:
	- LevelDataのBOX colliderから、移動解決用AABB、Navigation用AABB、デバッグ表示用OBB、汎用CollisionManager登録用Colliderを生成する。
	- Mesh/Polygonを直接衝突判定には使わず、ステージ衝突は単純Primitiveへ変換して扱う。
	- 将来は生成結果をStageCollisionSystemへ分けられるが、現段階ではStageが用途別配列を保持して既存挙動を保つ。
	*/

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
