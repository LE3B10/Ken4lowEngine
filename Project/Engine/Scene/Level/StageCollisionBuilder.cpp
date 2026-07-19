#include "StageCollisionBuilder.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "Matrix4x4.h"
#include "MineHiddenArenaLayout.h"
#include <algorithm>
#include <cctype>
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{
		AABB BuildAABBFromRotatedOBB(const Vector3& center, const Vector3& half, const Vector3& rotationRad)
		{
			// StageCollision専用: 回転BOXを移動解決向けAABBへ保守的に変換する。
			const Matrix4x4 rotation = Matrix4x4::MakeRotateMatrix(rotationRad);
			AABB aabb{};
			aabb.min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
			aabb.max = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

			for (int ix = -1; ix <= 1; ix += 2)
			{
				for (int iy = -1; iy <= 1; iy += 2)
				{
					for (int iz = -1; iz <= 1; iz += 2)
					{
						Vector3 cornerLocal = {
							half.x * static_cast<float>(ix),
							half.y * static_cast<float>(iy),
							half.z * static_cast<float>(iz),
						};
						const Vector3 cornerWorld = Vector3::Transform(cornerLocal, rotation) + center;
						aabb.min.x = (std::min)(aabb.min.x, cornerWorld.x);
						aabb.min.y = (std::min)(aabb.min.y, cornerWorld.y);
						aabb.min.z = (std::min)(aabb.min.z, cornerWorld.z);
						aabb.max.x = (std::max)(aabb.max.x, cornerWorld.x);
						aabb.max.y = (std::max)(aabb.max.y, cornerWorld.y);
						aabb.max.z = (std::max)(aabb.max.z, cornerWorld.z);
					}
				}
			}
			return aabb;
		}

		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		bool ContainsLadderName(const std::string& name)
		{
			return ToLower(name).find("ladder") != std::string::npos;
		}

		bool IsWalkableObstacleSurface(const ObjectData& data)
		{
			const std::string loweredName = ToLower(data.name);
			const bool lowCover = loweredName.find("cover") != std::string::npos ||
				loweredName.find("rubble") != std::string::npos ||
				loweredName.find("brokenbeam") != std::string::npos;
			const bool structural = loweredName.find("wall") != std::string::npos ||
				loweredName.find("gate") != std::string::npos ||
				loweredName.find("pillar") != std::string::npos ||
				loweredName.find("ceiling") != std::string::npos ||
				loweredName.find("rockdetail") != std::string::npos ||
				loweredName.find("support") != std::string::npos ||
				loweredName.find("dome") != std::string::npos ||
				(loweredName.find("beam") != std::string::npos && !lowCover);
			return lowCover && !structural; // 低い遮蔽物だけを登れる面にし、通路壁・ドーム壁・天井の上へ立てなくする。
		}
	}

	StageCollisionBuildResult StageCollisionBuilder::Build(const LevelData& levelData, const Vector3& offset)
	{
		StageCollisionBuildResult result{};
		const std::vector<ObjectData> hiddenArenaObjects = MineHiddenArenaLayout::Build(levelData);
		const size_t estimatedCount = levelData.objects.size() + hiddenArenaObjects.size();
		result.worldAABBs.reserve(estimatedCount);
		result.floorAABBs.reserve(estimatedCount);
		result.wallObstacleAABBs.reserve(estimatedCount);
		result.navigationObstacleAABBs.reserve(estimatedCount);
		result.worldColliders.reserve(estimatedCount);
		result.wallObstacleOBBs.reserve(estimatedCount);
		result.wallObstacleWalkable.reserve(estimatedCount);
		result.navigationObstacleOBBs.reserve(estimatedCount);
		result.ladderColliders.reserve(estimatedCount);
		result.ladderAABBs.reserve(estimatedCount);
		result.ladderOBBs.reserve(estimatedCount);

		auto appendCollider = [&](const ObjectData& data)
		{
			if (!data.collider.enabled || data.collider.type != "BOX") return;

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
			ApplyCollisionPreset(*collider, ECollisionPresetId::WorldStatic);
			collider->SetCenterPosition(centerW);
			collider->SetOBBHalfSize(halfW);
			const Vector3 colliderRotation = {
				data.rotation.x + data.collider.rotation.x,
				data.rotation.y + data.collider.rotation.y,
				data.rotation.z + data.collider.rotation.z,
			};
			collider->SetOrientation(colliderRotation);
			collider->SetDebugName(data.name);
			const AABB aabb = BuildAABBFromRotatedOBB(centerW, halfW, colliderRotation);
			const OBB colliderObb = collider->GetOBB();
			const std::string& collisionType = data.collider.collisionType;
			const bool isLadder = collisionType == "Ladder" || ContainsLadderName(data.name);
			if (isLadder)
			{
				collider->SetTrigger(true);
				collider->SetCollisionTag("Ladder");
				collider->SetCollisionLayer(kLadderPhysicsLayer);
				result.ladderColliders.push_back(collider.get());
				result.ladderAABBs.push_back(aabb);
				result.ladderOBBs.push_back(colliderObb);
			}
			else if (collisionType == "Floor")
			{
				result.worldAABBs.push_back(aabb);
				result.floorAABBs.push_back(aabb);
			}
			else if (collisionType == "Obstacle" || collisionType == "Pillar" || collisionType == "Fence" || collisionType == "Tree")
			{
				result.worldAABBs.push_back(aabb);
				result.wallObstacleAABBs.push_back(aabb);
				result.navigationObstacleAABBs.push_back(aabb);
				result.wallObstacleOBBs.push_back(colliderObb);
				result.wallObstacleWalkable.push_back(IsWalkableObstacleSurface(data) ? 1u : 0u);
				result.navigationObstacleOBBs.push_back(colliderObb);
			}
			else
			{
				result.worldAABBs.push_back(aabb);
			}

			result.worldColliders.push_back(std::move(collider));
		};

		for (const ObjectData& data : levelData.objects) appendCollider(data);
		for (const ObjectData& data : hiddenArenaObjects) appendCollider(data); // 追加広間の見た目と同じ配置からFloor/Wall判定を生成する。

		return result;
	}

	std::vector<Collider*> StageCollisionBuilder::GetColliders(const StageCollisionBuildResult& result)
	{
		std::vector<Collider*> colliders{};
		colliders.reserve(result.worldColliders.size());
		for (const std::unique_ptr<Collider>& collider : result.worldColliders)
		{
			if (collider) colliders.push_back(collider.get());
		}
		return colliders;
	}
} // namespace Ken4lowEngine
