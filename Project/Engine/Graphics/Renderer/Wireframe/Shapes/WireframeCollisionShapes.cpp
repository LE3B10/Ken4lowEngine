// Wireframe のコリジョン確認用デバッグ形状をまとめる。
#include "Wireframe.h"
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{

	void Wireframe::DrawAABB(const AABB& aabb, const Vector4& color)
	{
		Vector3 min = aabb.min;
		Vector3 max = aabb.max;
		Vector3 p1 = Vector3(min.x, min.y, min.z);
		Vector3 p2 = Vector3(max.x, min.y, min.z);
		Vector3 p3 = Vector3(max.x, max.y, min.z);
		Vector3 p4 = Vector3(min.x, max.y, min.z);
		Vector3 p5 = Vector3(min.x, min.y, max.z);
		Vector3 p6 = Vector3(max.x, min.y, max.z);
		Vector3 p7 = Vector3(max.x, max.y, max.z);
		Vector3 p8 = Vector3(min.x, max.y, max.z);
		// 底面
		DrawLine(p1, p2, color);
		DrawLine(p2, p3, color);
		DrawLine(p3, p4, color);
		DrawLine(p4, p1, color);
		// 上面
		DrawLine(p5, p6, color);
		DrawLine(p6, p7, color);
		DrawLine(p7, p8, color);
		DrawLine(p8, p5, color);
		// 側面
		DrawLine(p1, p5, color);
		DrawLine(p2, p6, color);
		DrawLine(p3, p7, color);
		DrawLine(p4, p8, color);
	}

	void Wireframe::DrawOBB(const OBB& obb, const Vector4& color)
	{
		// OBBの各頂点を定義（ローカル座標）
		Vector3 localVertices[8] = {
			{ -obb.size.x, -obb.size.y, -obb.size.z }, { obb.size.x, -obb.size.y, -obb.size.z },
			{ obb.size.x, obb.size.y, -obb.size.z }, { -obb.size.x, obb.size.y, -obb.size.z },
			{ -obb.size.x, -obb.size.y, obb.size.z }, { obb.size.x, -obb.size.y, obb.size.z },
			{ obb.size.x, obb.size.y, obb.size.z }, { -obb.size.x, obb.size.y, obb.size.z }
		};
		// ワールド座標に変換（回転適用 & 平行移動）
		Vector3 worldVertices[8];
		for (int i = 0; i < 8; i++) {
			worldVertices[i] =
				obb.center +
				obb.orientations[0] * localVertices[i].x +
				obb.orientations[1] * localVertices[i].y +
				obb.orientations[2] * localVertices[i].z;
		}
		// OBBのエッジを結ぶ
		int edges[12][2] = {
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // 底面
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // 上面
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }  // 側面
		};
		for (int i = 0; i < 12; i++) {
			DrawLine(worldVertices[edges[i][0]], worldVertices[edges[i][1]], color);
		}
	}

	void Wireframe::DrawSphere(const Vector3& center, const float radius, const Vector4& color)
	{
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(Vector3(radius, radius, radius), Vector3(0.0f, 0.0f, 0.0f), center);
		for (uint32_t i = 0; i + 2 < spheres_.size(); i += 3)
		{
			Vector3 a = spheres_[i];
			Vector3 b = spheres_[i + 1];
			Vector3 c = spheres_[i + 2];
			a = Vector3::Transform(a, worldMatrix);
			b = Vector3::Transform(b, worldMatrix);
			c = Vector3::Transform(c, worldMatrix);
			// 線描画
			DrawLine(a, b, color);
			//DrawLine(b, c, color);
			DrawLine(a, c, color); // 三角形を完成させるための線を追加
		}
	}

	void Wireframe::DrawSphere(const Sphere& sphere, const Vector4& color)
	{
		DrawSphere(sphere.center, sphere.radius, color);
	}

	void Wireframe::DrawCapsule(const Vector3& center, float radius, float height, const Vector3& axis, uint32_t segments, const Vector4& color)
	{
		constexpr float PI = std::numbers::pi_v<float>;
		float angleStep = (2.0f * PI) / float(segments);
		// 軸ベクトル
		Vector3 up = Vector3::Normalize(axis);
		Vector3 right = Vector3(1.0f, 0.0f, 0.0f);
		if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
			right = Vector3(0.0f, 1.0f, 0.0f);
		}
		Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
		right = Vector3::Normalize(Vector3::Cross(forward, up));
		// カプセル全体の高さから Cylinder の高さを求める
		float cylinderHeight = height - 2.0f * radius;
		if (cylinderHeight < 0.0f) cylinderHeight = 0.0f; // 異常入力対策
		// Cylinder の Bottom / Top 中心位置
		Vector3 cylinderBottom = Vector3::Add(center, Vector3::Multiply(-0.5f * cylinderHeight, up));
		Vector3 cylinderTop = Vector3::Add(center, Vector3::Multiply(0.5f * cylinderHeight, up));
		// Sphere の中心位置（※ ここが重要！）
		Vector3 bottomSphereCenter = Vector3::Add(center, Vector3::Multiply(-0.5f * cylinderHeight, up));
		Vector3 topSphereCenter = Vector3::Add(center, Vector3::Multiply(0.5f * cylinderHeight, up));
		// Cylinder 円周の頂点
		std::vector<Vector3> bottomVertices(segments);
		std::vector<Vector3> topVertices(segments);
		for (uint32_t i = 0; i < segments; i++) {
			float angle = angleStep * i;
			Vector3 offset = Vector3::Add(
				Vector3::Multiply(radius * cosf(angle), right),
				Vector3::Multiply(radius * sinf(angle), forward));
			bottomVertices[i] = Vector3::Add(cylinderBottom, offset);
			topVertices[i] = Vector3::Add(cylinderTop, offset);
		}
		// Cylinder 円周と側面
		for (uint32_t i = 0; i < segments; i++) {
			DrawLine(bottomVertices[i], bottomVertices[(i + 1) % segments], color);
			DrawLine(topVertices[i], topVertices[(i + 1) % segments], color);
			DrawLine(bottomVertices[i], topVertices[i], color);
		}
		// 半球の縦方向線
		float hemisphereStep = (PI / 2.0f) / float(segments / 2);
		for (uint32_t i = 0; i <= segments / 2; ++i) {
			float vAngle0 = hemisphereStep * float(i);
			float vAngle1 = hemisphereStep * float(i + 1);
			float z0 = radius * sinf(vAngle0);
			float r0 = radius * cosf(vAngle0);
			float z1 = radius * sinf(vAngle1);
			float r1 = radius * cosf(vAngle1);
			for (uint32_t j = 0; j <= segments; ++j) {
				float hAngle = angleStep * j;
				Vector3 offset0 = Vector3::Add(
					Vector3::Multiply(r0 * cosf(hAngle), right),
					Vector3::Multiply(r0 * sinf(hAngle), forward));
				Vector3 offset1 = Vector3::Add(
					Vector3::Multiply(r1 * cosf(hAngle), right),
					Vector3::Multiply(r1 * sinf(hAngle), forward));
				// Bottom hemisphere
				Vector3 p0 = Vector3::Add(bottomSphereCenter, offset0);
				p0 = Vector3::Add(p0, Vector3::Multiply(-z0, up));
				Vector3 p1 = Vector3::Add(bottomSphereCenter, offset1);
				p1 = Vector3::Add(p1, Vector3::Multiply(-z1, up));
				DrawLine(p0, p1, color);
				// Top hemisphere
				Vector3 t0 = Vector3::Add(topSphereCenter, offset0);
				t0 = Vector3::Add(t0, Vector3::Multiply(z0, up));
				Vector3 t1 = Vector3::Add(topSphereCenter, offset1);
				t1 = Vector3::Add(t1, Vector3::Multiply(z1, up));
				DrawLine(t0, t1, color);
			}
		}
		// 半球 横リング
		for (uint32_t i = 1; i < segments / 2; ++i) {
			float vAngle = hemisphereStep * float(i);
			float z = radius * sinf(vAngle);
			float r = radius * cosf(vAngle);
			std::vector<Vector3> ringVertices(segments + 1);
			for (uint32_t j = 0; j <= segments; ++j) {
				float hAngle = angleStep * j;
				Vector3 offset = Vector3::Add(
					Vector3::Multiply(r * cosf(hAngle), right),
					Vector3::Multiply(r * sinf(hAngle), forward));
				// Bottom
				Vector3 p = Vector3::Add(bottomSphereCenter, offset);
				p = Vector3::Add(p, Vector3::Multiply(-z, up));
				ringVertices[j] = p;
			}
			for (uint32_t j = 0; j < segments; ++j) {
				DrawLine(ringVertices[j], ringVertices[j + 1], color);
			}
			for (uint32_t j = 0; j <= segments; ++j) {
				float hAngle = angleStep * j;
				Vector3 offset = Vector3::Add(
					Vector3::Multiply(r * cosf(hAngle), right),
					Vector3::Multiply(r * sinf(hAngle), forward));
				// Top
				Vector3 t = Vector3::Add(topSphereCenter, offset);
				t = Vector3::Add(t, Vector3::Multiply(z, up));
				ringVertices[j] = t;
			}
			for (uint32_t j = 0; j < segments; ++j) {
				DrawLine(ringVertices[j], ringVertices[j + 1], color);
			}
		}
	}

	void Wireframe::DrawCapsule(const Capsule& capsule, const Vector4& color)
	{
		const Vector3& start = capsule.segment.origin;
		const Vector3& end = capsule.segment.origin + capsule.segment.diff;
		float radius = capsule.radius;
		Vector3 center = (start + end) * 0.5f;
		// 高さは radius × 2 加算した「全体の高さ」
		float height = Vector3::Length(end - start) + radius * 2.0f;
		Vector3 axis = Vector3::Normalize(end - start);
		DrawCapsule(center, radius, height, axis, 8, color);
	}

	void Wireframe::DrawPlane(const Plane& plane, float size, const Vector4& color)
	{
		// Plane の法線と距離から原点位置を算出
		Vector3 center = plane.normal * plane.distance;
		// Planeの法線に垂直な2軸を求める
		Vector3 right = Vector3::Cross({ 0, 1, 0 }, plane.normal);
		if (Vector3::Length(right) < 0.001f) {
			right = Vector3::Cross({ 1, 0, 0 }, plane.normal);
		}
		right = Vector3::Normalize(right);
		Vector3 forward = Vector3::Normalize(Vector3::Cross(plane.normal, right));
		// 平面上の4頂点を作成（正方形として描画）
		Vector3 halfRight = right * (size * 0.5f);
		Vector3 halfForward = forward * (size * 0.5f);
		Vector3 p0 = center - halfRight - halfForward;
		Vector3 p1 = center + halfRight - halfForward;
		Vector3 p2 = center + halfRight + halfForward;
		Vector3 p3 = center - halfRight + halfForward;
		// 線で囲む
		DrawLine(p0, p1, color);
		DrawLine(p1, p2, color);
		DrawLine(p2, p3, color);
		DrawLine(p3, p0, color);
		// 法線の可視化（上向きの線）
		DrawLine(center, center + plane.normal * (size * 0.25f), { 1, 0, 0, 1 }); // 赤で表示
	}

	void Wireframe::DrawCylinder(const Vector3& baseCenter, float radius, float height, const Vector3& axis, uint32_t segmentCount, const Vector4& color)
	{
		constexpr float PI = std::numbers::pi_v<float>;
		float angleStep = (2.0f * PI) / float(segmentCount);
		std::vector<Vector3> topVertices(segmentCount);
		std::vector<Vector3> bottomVertices(segmentCount);
		// 円の基準軸を決める（axis に直交する2つのベクトルを求める）
		Vector3 up = Vector3::Normalize(axis);
		Vector3 right = Vector3(1.0f, 0.0f, 0.0f);
		if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
			right = Vector3(0.0f, 1.0f, 0.0f);
		}
		Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
		right = Vector3::Normalize(Vector3::Cross(forward, up));
		// 上面・下面の円の頂点を計算
		for (uint32_t i = 0; i < segmentCount; i++) {
			float angle = angleStep * i;
			Vector3 offset = Vector3::Add(Vector3::Multiply(radius * cosf(angle), right),
				Vector3::Multiply(radius * sinf(angle), forward));
			bottomVertices[i] = Vector3::Add(baseCenter, offset);
			topVertices[i] = Vector3::Add(bottomVertices[i], Vector3::Multiply(height, up));
		}
		// 上面の円を描画
		for (uint32_t i = 0; i < segmentCount; i++) {
			DrawLine(topVertices[i], topVertices[(i + 1) % segmentCount], color);
		}
		// 下面の円を描画
		for (uint32_t i = 0; i < segmentCount; i++) {
			DrawLine(bottomVertices[i], bottomVertices[(i + 1) % segmentCount], color);
		}
		// 上面と下面をつなぐ側面の線を描画
		for (uint32_t i = 0; i < segmentCount; i++) {
			DrawLine(bottomVertices[i], topVertices[i], color);
		}
	}

} // namespace Ken4lowEngine
