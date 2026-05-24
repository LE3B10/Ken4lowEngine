#include "StageCollisionBuilder.h"
#include "CollisionTypeIdDef.h"
#include "Matrix4x4.h"
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{
		StageCollisionType ParseStageCollisionType(const std::string& collisionTypeName)
		{
			if (collisionTypeName == "Floor") { return StageCollisionType::Floor; }
			if (collisionTypeName == "Obstacle") { return StageCollisionType::Obstacle; }
			if (collisionTypeName == "Pillar") { return StageCollisionType::Pillar; }
			if (collisionTypeName == "Fence") { return StageCollisionType::Fence; }
			if (collisionTypeName == "Tree") { return StageCollisionType::Tree; }
			return StageCollisionType::Unknown;
		}

		Vector3 TransformDirection(const Vector3& v, const Matrix4x4& m)
		{
			// 回転方向だけを変換するため、平行移動成分は使わない
			return {
				v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
				v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
				v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2],
			};
		}

		AABB BuildAABBFromCorners(const std::array<Vector3, 8>& corners)
		{
			AABB aabb{};
			aabb.min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
			aabb.max = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
			for (const auto& c : corners)
			{
				aabb.min.x = (std::min)(aabb.min.x, c.x);
				aabb.min.y = (std::min)(aabb.min.y, c.y);
				aabb.min.z = (std::min)(aabb.min.z, c.z);
				aabb.max.x = (std::max)(aabb.max.x, c.x);
				aabb.max.y = (std::max)(aabb.max.y, c.y);
				aabb.max.z = (std::max)(aabb.max.z, c.z);
			}
			return aabb;
		}

		std::array<Vector3, 8> BuildColliderObbCorners(const Vector3& center, const Vector3& half, const Vector3& rotationRad)
		{
			const Matrix4x4 rotation = Matrix4x4::MakeRotateMatrix(rotationRad);
			std::array<Vector3, 8> corners = {
				Vector3{ -half.x, -half.y, -half.z }, // 0 bottom
				Vector3{  half.x, -half.y, -half.z }, // 1
				Vector3{  half.x, -half.y,  half.z }, // 2
				Vector3{ -half.x, -half.y,  half.z }, // 3
				Vector3{ -half.x,  half.y, -half.z }, // 4 top
				Vector3{  half.x,  half.y, -half.z }, // 5
				Vector3{  half.x,  half.y,  half.z }, // 6
				Vector3{ -half.x,  half.y,  half.z }, // 7
			};
			for (Vector3& cornerLocal : corners)
			{
				cornerLocal = Vector3::Transform(cornerLocal, rotation) + center;
			}
			return corners;
		}

		AABB BuildAABBFromRotatedOBB(const Vector3& center, const Vector3& half, const Vector3& rotationRad)
		{
			return BuildAABBFromCorners(BuildColliderObbCorners(center, half, rotationRad));
		}
	}

	StageCollisionBuildResult StageCollisionBuilder::Build(const LevelData& levelData, const Vector3& offset)
	{
		StageCollisionBuildResult result{};
		result.worldAABBs.reserve(levelData.objects.size());
		result.floorAABBs.reserve(levelData.objects.size());
		result.wallObstacleAABBs.reserve(levelData.objects.size());
		result.navigationObstacleAABBs.reserve(levelData.objects.size());
		result.obstacleBoxes.reserve(levelData.objects.size());
		result.worldColliders.reserve(levelData.objects.size());

		for (const ObjectData& data : levelData.objects)
		{
			if (!data.collider.enabled)
			{
				continue;
			}

			if (data.collider.type != "BOX")
			{
				continue;
			}

			const Vector3 centerW = {
				data.position.x + data.collider.center.x * data.scale.x + offset.x,
				data.position.y + data.collider.center.y * data.scale.y + offset.y,
				data.position.z + data.collider.center.z * data.scale.z + offset.z
			};

			const Vector3 halfW = {
				0.5f * data.collider.size.x * data.scale.x,
				0.5f * data.collider.size.y * data.scale.y,
				0.5f * data.collider.size.z * data.scale.z,
			};

			auto collider = std::make_unique<Collider>();
			// JSONのcollision_type_idはステージ分類IDなので、CollisionTypeIdDefへ直接変換せずStageCollisionTypeとして扱う
			collider->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kWorld));
			collider->SetCenterPosition(centerW);
			collider->SetOBBHalfSize(halfW);
			const Vector3 colliderRotation = {
				data.rotation.x + data.collider.rotation.x,
				data.rotation.y + data.collider.rotation.y,
				data.rotation.z + data.collider.rotation.z,
			};
			collider->SetOrientation(colliderRotation);
			// OBB表示とNavigation/Wall用AABBの生成元を揃え、見た目と判定の向き不一致を解消する
			const AABB legacyAabb = {
				centerW - halfW,
				centerW + halfW,
			};
			const std::array<Vector3, 8> corners = BuildColliderObbCorners(centerW, halfW, colliderRotation);
			const AABB aabb = BuildAABBFromCorners(corners);
			result.worldAABBsLegacy.push_back(legacyAabb);
			result.worldAABBs.push_back(aabb);
			StageObstacleBox box{};
			box.name = data.name;
			box.collisionTypeName = data.collider.collisionType;
			box.rawCollisionTypeId = data.collider.collisionTypeId;
			box.stageCollisionType = data.collider.stageCollisionType;
			box.corners = corners;
			box.enclosingAABB = aabb;
			box.center = centerW;
			box.halfSize = halfW;
			const Matrix4x4 rotationM = Matrix4x4::MakeRotateMatrix(colliderRotation);
			// 回転行列でローカル軸を変換し、StageObstacleBoxのOBB軸として保持する
			box.axisX = Vector3::Normalize(TransformDirection({ 1.0f, 0.0f, 0.0f }, rotationM));
			box.axisY = Vector3::Normalize(TransformDirection({ 0.0f, 1.0f, 0.0f }, rotationM));
			box.axisZ = Vector3::Normalize(TransformDirection({ 0.0f, 0.0f, 1.0f }, rotationM));
			result.obstacleBoxes.push_back(box);
			const StageCollisionType stageCollisionType = data.collider.stageCollisionType;
			if (stageCollisionType == StageCollisionType::Floor)
			{
				result.floorAABBs.push_back(aabb);
				++result.floorCount;
			}
			else if (stageCollisionType == StageCollisionType::Obstacle ||
				stageCollisionType == StageCollisionType::Pillar ||
				stageCollisionType == StageCollisionType::Fence ||
				stageCollisionType == StageCollisionType::Tree)
			{
				result.wallObstacleAABBs.push_back(aabb);
				result.navigationObstacleAABBs.push_back(aabb);
				if (stageCollisionType == StageCollisionType::Obstacle) { ++result.obstacleCount; }
				else if (stageCollisionType == StageCollisionType::Pillar) { ++result.pillarCount; }
				else if (stageCollisionType == StageCollisionType::Fence) { ++result.fenceCount; }
				else if (stageCollisionType == StageCollisionType::Tree) { ++result.treeCount; }
			}

			result.worldColliders.push_back(std::move(collider));
		}

		return result;
	}
} // namespace Ken4lowEngine
