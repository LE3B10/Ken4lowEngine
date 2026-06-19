#include "StageCollisionBuilder.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "Matrix4x4.h"
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
	}

	StageCollisionBuildResult StageCollisionBuilder::Build(const LevelData& levelData, const Vector3& offset)
	{
		// LevelData上のステージColliderを、移動解決・Navigation・汎用通知の用途別データへ分解する。
		StageCollisionBuildResult result{};
		result.worldAABBs.reserve(levelData.objects.size());
		result.floorAABBs.reserve(levelData.objects.size());
		result.wallObstacleAABBs.reserve(levelData.objects.size());
		result.navigationObstacleAABBs.reserve(levelData.objects.size());
		result.worldColliders.reserve(levelData.objects.size());
		result.wallObstacleOBBs.reserve(levelData.objects.size());
		result.wallObstacleWalkable.reserve(levelData.objects.size());
		result.navigationObstacleOBBs.reserve(levelData.objects.size());

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
			ApplyCollisionPreset(*collider, ECollisionPresetId::WorldStatic);
			collider->SetCenterPosition(centerW);
			collider->SetOBBHalfSize(halfW);
			const Vector3 colliderRotation = {
				data.rotation.x + data.collider.rotation.x,
				data.rotation.y + data.collider.rotation.y,
				data.rotation.z + data.collider.rotation.z,
			};
			collider->SetOrientation(colliderRotation);
			// 正式形状はColliderの回転OBBとし、包み込みAABBはBroadPhase・簡易判定用に併存させる。
			const AABB aabb = BuildAABBFromRotatedOBB(centerW, halfW, colliderRotation);
			result.worldAABBs.push_back(aabb);
			const OBB colliderObb = collider->GetOBB();
			const std::string& collisionType = data.collider.collisionType;
			if (collisionType == "Floor")
			{
				// Floorは接地・スポーン補正向けに分け、壁押し戻しやNavigation障害物とは混ぜない。
				result.floorAABBs.push_back(aabb);
			}
			else if (collisionType == "Obstacle" || collisionType == "Pillar" || collisionType == "Fence" || collisionType == "Tree")
			{
				// Obstacle系AABBはBroadPhase用、OBBは見た目に沿う最終横押し戻し用として対で保持する。
				result.wallObstacleAABBs.push_back(aabb);
				result.navigationObstacleAABBs.push_back(aabb);
				// NarrowPhaseとOBB表示にはCollider::GetOBB()由来の姿勢をそのまま使う。
				result.wallObstacleOBBs.push_back(colliderObb);
				// 現段階ではObstacle上面を歩行可能とし、将来JSONのwalkable指定へ差し替えられる配列を併設する。
				result.wallObstacleWalkable.push_back(1u);
				result.navigationObstacleOBBs.push_back(colliderObb);
			}

			result.worldColliders.push_back(std::move(collider));
		}

		return result;
	}

	std::vector<Collider*> StageCollisionBuilder::GetColliders(const StageCollisionBuildResult& result)
	{
		// StageCollisionBuilderが生成したColliderの所有権は移さず、PhysicsWorld登録用の生ポインタだけを集める。
		std::vector<Collider*> colliders{};
		colliders.reserve(result.worldColliders.size());
		for (const std::unique_ptr<Collider>& collider : result.worldColliders)
		{
			if (collider)
			{
				colliders.push_back(collider.get());
			}
		}
		return colliders;
	}
} // namespace Ken4lowEngine
