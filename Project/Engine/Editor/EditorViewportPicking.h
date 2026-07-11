#pragma once

#include "EditorObjectInfo.h"

#include <CameraManager.h>
#include <Matrix4x4.h>
#include <Vector2.h>
#include <Vector3.h>

#include <cmath>
#include <limits>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Main Viewportのスクリーン座標から作成したEditor選択用Rayです。
	/// </summary>
	struct EditorViewportPickingRay
	{
		Vector3 origin{};
		Vector3 direction{ 0.0f, 0.0f, 1.0f };
	};

	/// <summary>
	/// EditorObjectInfoが公開するPicking Boundsを使い、最前面のEditor IDを解決します。
	/// </summary>
	class EditorViewportPicking
	{
	public:
		/// <summary>
		/// Main Viewport上のスクリーン座標をカメラRayへ変換します。
		/// </summary>
		static bool BuildRay(
			const Vector2& screenMouse,
			const Vector2& viewportScreenMin,
			const Vector2& viewportImageSize,
			EditorViewportPickingRay& outRay)
		{
			if (viewportImageSize.x <= 1.0f || viewportImageSize.y <= 1.0f)
			{
				return false;
			}

			const float localX = screenMouse.x - viewportScreenMin.x;
			const float localY = screenMouse.y - viewportScreenMin.y;
			if (localX < 0.0f || localY < 0.0f || localX > viewportImageSize.x || localY > viewportImageSize.y)
			{
				return false;
			}

			const float ndcX = (localX / viewportImageSize.x) * 2.0f - 1.0f;
			const float ndcY = 1.0f - (localY / viewportImageSize.y) * 2.0f;
			const Matrix4x4 projection = CameraManager::GetInstance()->GetActiveProjectionMatrix();
			if (std::abs(projection.m[0][0]) <= 0.00001f || std::abs(projection.m[1][1]) <= 0.00001f)
			{
				return false;
			}

			const Vector3 viewDirection{
				ndcX / projection.m[0][0],
				ndcY / projection.m[1][1],
				1.0f
			};
			const Matrix4x4 inverseView = Matrix4x4::Inverse(CameraManager::GetInstance()->GetActiveViewMatrix());
			Vector3 worldDirection{
				viewDirection.x * inverseView.m[0][0] + viewDirection.y * inverseView.m[1][0] + viewDirection.z * inverseView.m[2][0],
				viewDirection.x * inverseView.m[0][1] + viewDirection.y * inverseView.m[1][1] + viewDirection.z * inverseView.m[2][1],
				viewDirection.x * inverseView.m[0][2] + viewDirection.y * inverseView.m[1][2] + viewDirection.z * inverseView.m[2][2]
			};

			worldDirection = Vector3::NormalizeSafe(worldDirection, CameraManager::GetInstance()->GetActiveCameraForward());
			if (Vector3::LengthSquared(worldDirection) <= 0.000001f)
			{
				return false;
			}

			outRay.origin = CameraManager::GetInstance()->GetActiveCameraPosition();
			outRay.direction = worldDirection;
			return true;
		}

		/// <summary>
		/// Rayと交差するPicking Boundsのうち、最も近いEditorObjectInfoを返します。
		/// </summary>
		static bool PickClosest(
			const EditorViewportPickingRay& ray,
			const std::vector<EditorObjectInfo>& objects,
			EditorObjectInfo& outObject,
			float* outDistance = nullptr)
		{
			float closestDistance = std::numeric_limits<float>::max();
			bool found = false;
			std::vector<BoundingSphere> pickingSpheres;

			for (const EditorObjectInfo& object : objects)
			{
				pickingSpheres.clear();
				if (!object.CollectViewportPickingSpheres(pickingSpheres))
				{
					continue;
				}

				for (const BoundingSphere& sphere : pickingSpheres)
				{
					float hitDistance = 0.0f;
					if (!IntersectRaySphere(ray, sphere, hitDistance) || hitDistance >= closestDistance)
					{
						continue;
					}

					closestDistance = hitDistance;
					outObject = object; // 選択には毎フレーム再収集されたEditor IDと安全なCallbackだけを保存する。
					found = true;
				}
			}

			if (found && outDistance)
			{
				*outDistance = closestDistance;
			}
			return found;
		}

	private:
		static bool IntersectRaySphere(
			const EditorViewportPickingRay& ray,
			const BoundingSphere& sphere,
			float& outDistance)
		{
			if (!std::isfinite(sphere.radius) || sphere.radius <= 0.0001f)
			{
				return false;
			}

			const Vector3 originToCenter = ray.origin - sphere.center;
			const float b = Vector3::Dot(originToCenter, ray.direction);
			const float c = Vector3::Dot(originToCenter, originToCenter) - sphere.radius * sphere.radius;
			const float discriminant = b * b - c;
			if (discriminant < 0.0f)
			{
				return false;
			}

			const float squareRoot = std::sqrt(discriminant);
			float distance = -b - squareRoot;
			if (distance < 0.0f)
			{
				distance = -b + squareRoot;
			}
			if (distance < 0.0f)
			{
				return false;
			}

			outDistance = distance;
			return true;
		}
	};
} // namespace Ken4lowEngine
