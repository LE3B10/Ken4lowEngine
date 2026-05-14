#include "EnemyTacticalPointDebugDrawer.h"

#include <Wireframe.h>

using namespace Ken4lowEngine;

void EnemyTacticalPointDebugDrawer::Draw(const std::vector<EnemyTacticalDebugPoint>& points, const Vector3& enemyPosition, float sphereRadius)
{
	Wireframe* wireframe = Wireframe::GetInstance();
	if (!wireframe) return;

	const Vector4 validColor{ 0.0f, 1.0f, 0.0f, 1.0f };
	const Vector4 selectedColor{ 0.0f, 0.35f, 1.0f, 1.0f };
	const Vector4 invalidColor{ 1.0f, 0.0f, 0.0f, 1.0f };
	const Vector4 selectedLineColor{ 1.0f, 0.9f, 0.0f, 1.0f };

	for (const EnemyTacticalDebugPoint& point : points)
	{
		const Vector4 color = point.selected ? selectedColor : (point.valid ? validColor : invalidColor);
		// EQS風に候補点ごとの判定結果を球色で示します。
		wireframe->DrawSphere(point.position, sphereRadius, color);
		if (point.selected)
		{
			wireframe->DrawLine(enemyPosition, point.position, selectedLineColor);
		}
	}
}