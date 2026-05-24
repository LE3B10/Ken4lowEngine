#include "StageCollisionBuilder.h"
#include "CollisionTypeIdDef.h"
#include "Matrix4x4.h"
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{

		Vector3 TransformDirection(const Vector3& v, const Matrix4x4& m)
		{
			// 回転方向だけを変換するため、平行移動成分は使わない
			return {
				v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
				v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
				v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2],
			};
		}

		std::array<Vector3, 8> BuildCornersFromOBB(const OBB& obb)
		{
			const std::array<Vector3, 8> localVertices = {
				Vector3{-obb.size.x, -obb.size.y, -obb.size.z},
				Vector3{ obb.size.x, -obb.size.y, -obb.size.z},
				Vector3{ obb.size.x, -obb.size.y,  obb.size.z},
				Vector3{-obb.size.x, -obb.size.y,  obb.size.z},
				Vector3{-obb.size.x,  obb.size.y, -obb.size.z},
				Vector3{ obb.size.x,  obb.size.y, -obb.size.z},
				Vector3{ obb.size.x,  obb.size.y,  obb.size.z},
				Vector3{-obb.size.x,  obb.size.y,  obb.size.z},
			};
			std::array<Vector3, 8> corners{};
			for (size_t i = 0; i < localVertices.size(); ++i)
			{
				corners[i] = obb.center
					+ obb.orientations[0] * localVertices[i].x
					+ obb.orientations[1] * localVertices[i].y
					+ obb.orientations[2] * localVertices[i].z;
			}
			return corners;
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

			const Vector3 scaledLocalCenter = {
				data.collider.center.x * data.scale.x,
				data.collider.center.y * data.scale.y,
				data.collider.center.z * data.scale.z,
			};
			const Matrix4x4 objectRotationM = Matrix4x4::MakeRotateMatrix(data.rotation);
			// collider.center はローカルオフセットなので、オブジェクト回転を適用してワールド中心へ変換する
			const Vector3 rotatedLocalCenter = TransformDirection(scaledLocalCenter, objectRotationM);
			const Vector3 centerW = data.position + rotatedLocalCenter + offset;

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
			const OBB colliderObb = collider->GetOBB();
			// Debug表示とNavigation/Wall判定を実ColliderのOBBから同一生成し、差分をなくす
			const std::array<Vector3, 8> corners = BuildCornersFromOBB(colliderObb);
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
			box.center = colliderObb.center;
			box.halfSize = colliderObb.size;
			box.axisX = colliderObb.orientations[0];
			box.axisY = colliderObb.orientations[1];
			box.axisZ = colliderObb.orientations[2];
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
