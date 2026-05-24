#include "StageCollisionBuilder.h"
#include "CollisionTypeIdDef.h"
#include "Matrix4x4.h"
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{
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

		std::array<Vector3, 8> BuildObbCorners(const Vector3& center, const Vector3& half, const Vector3& rotationRad)
		{
			const Matrix4x4 rotation = Matrix4x4::MakeRotateMatrix(rotationRad);
			std::array<Vector3, 8> corners{};
			int index = 0;
			for (int ix = -1; ix <= 1; ix += 2)
			{
				for (int iy = -1; iy <= 1; iy += 2)
				{
					for (int iz = -1; iz <= 1; iz += 2)
					{
						Vector3 cornerLocal = { half.x * static_cast<float>(ix), half.y * static_cast<float>(iy), half.z * static_cast<float>(iz) };
						corners[static_cast<size_t>(index++)] = Vector3::Transform(cornerLocal, rotation) + center;
					}
				}
			}
			return corners;
		}

		AABB BuildAABBFromRotatedOBB(const Vector3& center, const Vector3& half, const Vector3& rotationRad)
		{
			return BuildAABBFromCorners(BuildObbCorners(center, half, rotationRad));
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
			const std::array<Vector3, 8> corners = BuildObbCorners(centerW, halfW, colliderRotation);
			const AABB aabb = BuildAABBFromCorners(corners);
			result.worldAABBsLegacy.push_back(legacyAabb);
			result.worldAABBs.push_back(aabb);
			StageCollisionBuildResult::StageObstacleBox box{};
			box.name = data.name;
			box.collisionTypeName = data.collider.collisionType;
			box.collisionTypeId = data.collider.collisionTypeId;
			box.corners = corners;
			box.enclosingAABB = aabb;
			box.center = centerW;
			box.halfSize = halfW;
			const Matrix4x4 rotationM = Matrix4x4::MakeRotateMatrix(colliderRotation);
			box.axisX = Vector3::Normalize(Vector3::TransformNormal({ 1.0f, 0.0f, 0.0f }, rotationM));
			box.axisY = Vector3::Normalize(Vector3::TransformNormal({ 0.0f, 1.0f, 0.0f }, rotationM));
			box.axisZ = Vector3::Normalize(Vector3::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotationM));
			result.obstacleBoxes.push_back(box);
			const std::string& collisionType = data.collider.collisionType;
			if (collisionType == "Floor")
			{
				result.floorAABBs.push_back(aabb);
			}
			else if (collisionType == "Obstacle" || collisionType == "Pillar" || collisionType == "Fence" || collisionType == "Tree")
			{
				result.wallObstacleAABBs.push_back(aabb);
				result.navigationObstacleAABBs.push_back(aabb);
			}

			result.worldColliders.push_back(std::move(collider));
		}

		return result;
	}
} // namespace Ken4lowEngine
