#include "StageCollisionBuilder.h"
#include "CollisionTypeIdDef.h"
#include <array>
#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

		Vector3 ConvertSourcePointToGamePoint(const Vector3& source)
		{
			return { -source.x, source.z, source.y };
		}

		void ConvertSourceRotationToGameBasis(const Matrix4x4& sourceRotation, Vector3& outX, Vector3& outY, Vector3& outZ)
		{
			const Vector3 sx = { sourceRotation.m[0][0], sourceRotation.m[0][1], sourceRotation.m[0][2] };
			const Vector3 sy = { sourceRotation.m[1][0], sourceRotation.m[1][1], sourceRotation.m[1][2] };
			const Vector3 sz = { sourceRotation.m[2][0], sourceRotation.m[2][1], sourceRotation.m[2][2] };
			outX = Vector3::Normalize(ConvertSourcePointToGamePoint(sx));
			outY = Vector3::Normalize(ConvertSourcePointToGamePoint(sy));
			outZ = Vector3::Normalize(ConvertSourcePointToGamePoint(sz));
		}
	}

	StageCollisionBuildResult StageCollisionBuilder::Build(const LevelData& levelData, const Vector3& offset)
	{
		StageCollisionBuildResult result{};
		result.worldAABBs.reserve(levelData.objects.size());
		result.worldColliders.reserve(levelData.objects.size());

		for (const ObjectData& data : levelData.objects)
		{
			if (!data.collider.enabled || data.collider.type != "BOX") { continue; }

			const Vector3 sourceObjectRotationRad = data.sourceRotationDeg * kDegToRad;
			const Matrix4x4 sourceObjectRotation = Matrix4x4::MakeRotateMatrix(sourceObjectRotationRad);
			const Matrix4x4 sourceColliderRotation = Matrix4x4::MakeRotateMatrix(data.collider.sourceRotationDeg * kDegToRad);
			const Matrix4x4 sourceLocalRotation = Matrix4x4::Multiply(sourceColliderRotation, sourceObjectRotation);

			Vector3 axisX{}, axisY{}, axisZ{};
			// モデルと同じsource→game変換をコライダーにも適用し、見た目と当たり判定の向きを一致させる
			ConvertSourceRotationToGameBasis(sourceLocalRotation, axisX, axisY, axisZ);

			const Vector3 localCenterScaled = Vector3::Multiply(data.collider.sourceCenter, data.sourceScale);
			const Vector3 sourceCenter = data.sourcePosition + localCenterScaled;
			const Vector3 centerW = ConvertSourcePointToGamePoint(sourceCenter) + offset;

			const Vector3 halfSource = data.collider.sourceSize * 0.5f;
			const Vector3 halfScaled = Vector3::Multiply(halfSource, data.sourceScale);
			auto collider = std::make_unique<Collider>();
			collider->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kWorld));
			collider->SetCenterPosition(centerW);
			collider->SetOBBHalfSize(halfScaled);
			collider->SetOBBBasis(axisX, axisY, axisZ);

			std::array<Vector3, 8> corners{};
			int i = 0;
			for (int sx : { -1, 1 }) for (int sy : { -1, 1 }) for (int sz : { -1, 1 })
			{
				corners[i++] = centerW + axisX * (halfScaled.x * static_cast<float>(sx)) + axisY * (halfScaled.y * static_cast<float>(sy)) + axisZ * (halfScaled.z * static_cast<float>(sz));
			}
			AABB aabb{};
			aabb.min = aabb.max = corners[0];
			for (const auto& p : corners)
			{
				aabb.min.x = std::min(aabb.min.x, p.x); aabb.min.y = std::min(aabb.min.y, p.y); aabb.min.z = std::min(aabb.min.z, p.z);
				aabb.max.x = std::max(aabb.max.x, p.x); aabb.max.y = std::max(aabb.max.y, p.y); aabb.max.z = std::max(aabb.max.z, p.z);
			}
			result.worldAABBs.push_back(aabb);
			result.worldColliders.push_back(std::move(collider));
		}
		return result;
	}
}
