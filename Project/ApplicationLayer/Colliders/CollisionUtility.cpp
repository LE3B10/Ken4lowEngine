#include "CollisionUtility.h"

#include <algorithm>

bool CollisionUtility::IsCollision(const Sphere& s1, const Sphere& s2)
{
	//2つの球の中心点間の距離を求める
	float distance = Vector3::Length(Vector3::Subtract(s2.center, s1.center));
	// 半径の合計よりも短ければ衝突
	return distance <= (s1.radius + s2.radius);
}

bool CollisionUtility::IsCollision(const Sphere& sphere, const Plane& plane)
{
	// 平面の法線ベクトルと球の中心点との距離
	float distance = Vector3::Dot(plane.normal, sphere.center) - plane.distance;
	// その距離が球の半径以下なら衝突している
	return fabs(distance) <= sphere.radius;
}

bool CollisionUtility::IsCollision(const Segment& segment, const Plane& plane)
{
	//まず垂直判定を行うために、法線と線の内積を求める
	float dot = Vector3::Dot(plane.normal, segment.diff);

	//垂直 = 平行であるので、衝突しているはずがない
	// 浮動小数点数の比較は通常、直接の等号判定は避ける
	const float epsilon = 1e-6f;
	if (fabs(dot) < epsilon)
	{
		return false;
	}

	//tを求める
	float t = (plane.distance - Vector3::Dot(segment.origin, plane.normal)) / dot;

	//tの値と線の種類によって衝突しているかを判断する
	return t >= 0.0f && t <= 1.0f;
}

bool CollisionUtility::IsCollision(const Triangle& triangle, const Segment& segment)
{
	// 三角形の辺
	Vector3 edge1 = Vector3::Subtract(triangle.vertices[1], triangle.vertices[0]);
	Vector3 edge2 = Vector3::Subtract(triangle.vertices[2], triangle.vertices[0]);

	// 平面の法線ベクトルを計算
	Vector3 normal = Vector3::Cross(edge1, edge2);
	normal = Vector3::Normalize(normal);

	// 線分の方向ベクトル
	Vector3 dir = segment.diff;
	dir = Vector3::Normalize(dir);

	// 平面と線分の始点のベクトル
	Vector3 diff = Vector3::Subtract(triangle.vertices[0], segment.origin);

	// 線分が平面と平行かどうかをチェック
	float dotND = Vector3::Dot(normal, dir);
	if (fabs(dotND) < 1e-6f)
	{
		return false; // 線分が平面と平行
	}

	// 線分の始点と平面の交点を計算
	float t = Vector3::Dot(normal, diff) / dotND;

	if (t < 0.0f || t > Vector3::Length(segment.diff))
	{
		return false; // 線分上に交点がない
	}

	Vector3 intersection = Vector3::Add(segment.origin, Vector3::Multiply(t, dir));

	// バリツチェックで三角形の内部に交点があるかを確認
	Vector3 c0 = Vector3::Cross(Vector3::Subtract(triangle.vertices[1], triangle.vertices[0]), Vector3::Subtract(intersection, triangle.vertices[0]));
	Vector3 c1 = Vector3::Cross(Vector3::Subtract(triangle.vertices[2], triangle.vertices[1]), Vector3::Subtract(intersection, triangle.vertices[1]));
	Vector3 c2 = Vector3::Cross(Vector3::Subtract(triangle.vertices[0], triangle.vertices[2]), Vector3::Subtract(intersection, triangle.vertices[2]));

	if (Vector3::Dot(c0, normal) >= 0.0f && Vector3::Dot(c1, normal) >= 0.0f && Vector3::Dot(c2, normal) >= 0.0f)
	{
		return true; // 衝突
	}

	return false; // 衝突なし
}

bool CollisionUtility::IsCollision(const AABB& aabb, const Vector3& point)
{
	// 点がAABBの範囲内にあるかチェック
	return (point.x >= aabb.min.x && point.x <= aabb.max.x &&
		point.y >= aabb.min.y && point.y <= aabb.max.y &&
		point.z >= aabb.min.z && point.z <= aabb.max.z);
}

bool CollisionUtility::IsCollision(const AABB& aabb1, const AABB& aabb2)
{
	return
		(aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) && //x軸
		(aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) &&
		(aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z);
}

bool CollisionUtility::IsCollision(const AABB& aabb, const Sphere& sphere)
{
	//最近接点を求める
	Vector3 clossestPoint
	{
		std::clamp(sphere.center.x,aabb.min.x,aabb.max.x),
		std::clamp(sphere.center.y,aabb.min.y,aabb.max.y),
		std::clamp(sphere.center.z,aabb.min.z,aabb.max.z)
	};
	//最近接点と球の中途の距離を求める
	float distance = Vector3::Length(Vector3::Subtract(clossestPoint, sphere.center));
	//距離が半径よりも小さければ衝突
	return distance <= sphere.radius;
}

bool CollisionUtility::IsCollision(const AABB& aabb, const Segment& segment)
{
	float tNearX = (aabb.min.x - segment.origin.x) / segment.diff.x;
	float tFarX = (aabb.max.x - segment.origin.x) / segment.diff.x;
	if (tNearX > tFarX) std::swap(tNearX, tFarX);

	float tNearY = (aabb.min.y - segment.origin.y) / segment.diff.y;
	float tFarY = (aabb.max.y - segment.origin.y) / segment.diff.y;
	if (tNearY > tFarY) std::swap(tNearY, tFarY);

	float tNearZ = (aabb.min.z - segment.origin.z) / segment.diff.z;
	float tFarZ = (aabb.max.z - segment.origin.z) / segment.diff.z;
	if (tNearZ > tFarZ) std::swap(tNearZ, tFarZ);

	// 線分がAABBを貫通しているかどうかを判定
	float tmin = std::max(std::max(tNearX, tNearY), tNearZ);
	float tmax = std::min(std::min(tFarX, tFarY), tFarZ);

	// 衝突しているかどうかの判定
	if (tmin <= tmax && tmax >= 0.0f && tmin <= 1.0f) {
		return true;
	}
	return false;
}

bool CollisionUtility::IsCollision(const OBB& obb, const Sphere& sphere)
{
	// Step 1: Transform the sphere's center into the OBB's local space
	Matrix4x4 obbWorldMatrix = Matrix4x4::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, obb.center);
	Matrix4x4 obbRotationMatrix = Matrix4x4::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, {}, Vector3());
	obbRotationMatrix.m[0][0] = obb.orientations[0].x;
	obbRotationMatrix.m[0][1] = obb.orientations[0].y;
	obbRotationMatrix.m[0][2] = obb.orientations[0].z;

	obbRotationMatrix.m[1][0] = obb.orientations[1].x;
	obbRotationMatrix.m[1][1] = obb.orientations[1].y;
	obbRotationMatrix.m[1][2] = obb.orientations[1].z;

	obbRotationMatrix.m[2][0] = obb.orientations[2].x;
	obbRotationMatrix.m[2][1] = obb.orientations[2].y;
	obbRotationMatrix.m[2][2] = obb.orientations[2].z;

	obbWorldMatrix = Matrix4x4::Multiply(obbWorldMatrix, obbRotationMatrix);
	Matrix4x4 obbWorldMatrixInverse = Matrix4x4::Inverse(obbWorldMatrix);
	Vector3 centerInOBBLocalSpace = Vector3::Transform(sphere.center, obbWorldMatrixInverse);

	// Step 2: Find the closest point on the OBB to the sphere center in local space
	Vector3 closestPoint = centerInOBBLocalSpace;
	closestPoint.x = std::max(-obb.size.x * 0.5f, std::min(closestPoint.x, obb.size.x * 0.5f));
	closestPoint.y = std::max(-obb.size.y * 0.5f, std::min(closestPoint.y, obb.size.y * 0.5f));
	closestPoint.z = std::max(-obb.size.z * 0.5f, std::min(closestPoint.z, obb.size.z * 0.5f));

	// Step 3: Calculate the distance between the sphere center and this closest point
	Vector3 difference = centerInOBBLocalSpace - closestPoint;
	float distanceSquared = Vector3::Dot(difference, difference);

	// Step 4: Check if the distance is less than or equal to the sphere's radius squared
	return distanceSquared <= sphere.radius * sphere.radius;
}

bool CollisionUtility::IsCollision(const OBB& obb, const Segment& segment)
{
	// OBBの回転行列を作成
	Matrix4x4 rotationMatrix = {
		obb.orientations[0].x, obb.orientations[0].y, obb.orientations[0].z, 0.0f,
		obb.orientations[1].x, obb.orientations[1].y, obb.orientations[1].z, 0.0f,
		obb.orientations[2].x, obb.orientations[2].y, obb.orientations[2].z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	// OBBの逆変換行列（回転の転置行列と平行移動の逆変換）を作成
	Matrix4x4 obbWorldMatrixInverse = Matrix4x4::Inverse(rotationMatrix);
	obbWorldMatrixInverse.m[3][0] = -(obb.center.x * obbWorldMatrixInverse.m[0][0] + obb.center.y * obbWorldMatrixInverse.m[1][0] + obb.center.z * obbWorldMatrixInverse.m[2][0]);
	obbWorldMatrixInverse.m[3][1] = -(obb.center.x * obbWorldMatrixInverse.m[0][1] + obb.center.y * obbWorldMatrixInverse.m[1][1] + obb.center.z * obbWorldMatrixInverse.m[2][1]);
	obbWorldMatrixInverse.m[3][2] = -(obb.center.x * obbWorldMatrixInverse.m[0][2] + obb.center.y * obbWorldMatrixInverse.m[1][2] + obb.center.z * obbWorldMatrixInverse.m[2][2]);

	// セグメントの始点と終点をOBBのローカル空間に変換
	Vector3 localOrigin = Vector3::Transform(segment.origin, obbWorldMatrixInverse);
	Vector3 localEnd = Vector3::Transform(segment.origin + segment.diff, obbWorldMatrixInverse);

	// 変換後のセグメント
	Segment localSegment;
	localSegment.origin = localOrigin;
	localSegment.diff = localEnd - localOrigin;

	// OBBのローカル空間でAABBとの衝突判定を行う
	AABB aabbOBBLocal{ -obb.size, obb.size };
	return IsCollision(aabbOBBLocal, localSegment);
}

bool CollisionUtility::IsCollision(const Segment& segment, const OBB& obb)
{
	return IsCollision(obb, segment);
}

bool CollisionUtility::IsCollision(const OBB& obb1, const OBB& obb2)
{
	const float epsilon = 1e-5f;

	// OBBの軸
	Vector3 axes[15] = {
		obb1.orientations[0],
		obb1.orientations[1],
		obb1.orientations[2],
		obb2.orientations[0],
		obb2.orientations[1],
		obb2.orientations[2],
		Vector3::Cross(obb1.orientations[0], obb2.orientations[0]),
		Vector3::Cross(obb1.orientations[0], obb2.orientations[1]),
		Vector3::Cross(obb1.orientations[0], obb2.orientations[2]),
		Vector3::Cross(obb1.orientations[1], obb2.orientations[0]),
		Vector3::Cross(obb1.orientations[1], obb2.orientations[1]),
		Vector3::Cross(obb1.orientations[1], obb2.orientations[2]),
		Vector3::Cross(obb1.orientations[2], obb2.orientations[0]),
		Vector3::Cross(obb1.orientations[2], obb2.orientations[1]),
		Vector3::Cross(obb1.orientations[2], obb2.orientations[2]),
	};

	// 各軸に対して分離が存在するかをチェック
	for (int i = 0; i < 15; ++i) {
		if (Vector3::Length(axes[i]) < epsilon) {
			continue; // 無視できる軸
		}

		// 軸を正規化
		Vector3 axis = Vector3::Normalize(axes[i]);

		// OBB1の投影範囲
		float center1 = Vector3::Dot(obb1.center, axis);
		float extent1 =
			std::abs(Vector3::Dot(obb1.orientations[0] * obb1.size.x, axis)) +
			std::abs(Vector3::Dot(obb1.orientations[1] * obb1.size.y, axis)) +
			std::abs(Vector3::Dot(obb1.orientations[2] * obb1.size.z, axis));

		float min1 = center1 - extent1;
		float max1 = center1 + extent1;

		// OBB2の投影範囲
		float center2 = Vector3::Dot(obb2.center, axis);
		float extent2 =
			std::abs(Vector3::Dot(obb2.orientations[0] * obb2.size.x, axis)) +
			std::abs(Vector3::Dot(obb2.orientations[1] * obb2.size.y, axis)) +
			std::abs(Vector3::Dot(obb2.orientations[2] * obb2.size.z, axis));

		float min2 = center2 - extent2;
		float max2 = center2 + extent2;

		// 投影範囲が重ならなければ分離している
		if (max1 < min2 || max2 < min1) {
			return false; // 分離軸が存在する
		}
	}

	// すべての軸で分離がなければ衝突している
	return true;
}

/// 線分 PQ, RS 間の最近接距離²
static float SegmentSegmentDist2(const Vector3& P0, const Vector3& P1,
	const Vector3& Q0, const Vector3& Q1)
{
	const Vector3  u = P1 - P0;
	const Vector3  v = Q1 - Q0;
	const Vector3  w = P0 - Q0;
	const float    a = Vector3::Dot(u, u);
	const float    b = Vector3::Dot(u, v);
	const float    c = Vector3::Dot(v, v);
	const float    d = Vector3::Dot(u, w);
	const float    e = Vector3::Dot(v, w);
	const float    D = a * c - b * b;
	const float    EPS = 1e-6f;

	float sN, sD = D, tN, tD = D;

	// 線分がほぼ平行
	if (D < EPS) { sN = 0; sD = 1; tN = e; tD = c; }
	else {
		sN = (b * e - c * d);
		tN = (a * e - b * d);
		if (sN < 0) { sN = 0;  tN = e;        tD = c; }
		else if (sN > sD) { sN = sD; tN = e + b; tD = c; }
	}

	// t を [0,1] にクランプ
	if (tN < 0) {
		tN = 0;
		if (-d < 0)   sN = 0;
		else if (-d > a)   sN = sD;
		else { sN = -d; sD = a; }
	}
	else if (tN > tD) {
		tN = tD;
		if ((-d + b) < 0)      sN = 0;
		else if ((-d + b) > a) sN = sD;
		else { sN = -d + b; sD = a; }
	}

	const float sc = (fabsf(sN) < EPS ? 0.0f : sN / sD);
	const float tc = (fabsf(tN) < EPS ? 0.0f : tN / tD);
	const Vector3  dP = w + u * sc - v * tc;

	return Vector3::Dot(dP, dP);   // 距離²
}

/// Capsule–Capsule
inline bool IsCapsuleCapsuleHit(const Capsule& c1, const Capsule& c2)
{
	const float dist2 = SegmentSegmentDist2(c1.pointA, c1.pointB,
		c2.pointA, c2.pointB);
	const float rSum = c1.radius + c2.radius;
	return dist2 <= rSum * rSum + 1e-6f;   // EPS で数値誤差吸収
}

// -------------------------------------------------------------
//  Capsule–Capsule 衝突判定
// -------------------------------------------------------------
bool CollisionUtility::IsCollision(const Capsule& cap1,
	const Capsule& cap2)
{
	// 1. 中心線同士の最近接距離²を取得
	float dist2 = SegmentSegmentDist2(
		cap1.pointA, cap1.pointB,
		cap2.pointA, cap2.pointB);

	// 2. しきい値 = 半径の和 の²
	float rSum = cap1.radius + cap2.radius;
	return dist2 <= rSum * rSum + 1e-6f;    // EPS で誤差吸収
}

// ============================================================================
//  Capsule–Segment
// ============================================================================
bool CollisionUtility::IsCollision(const Capsule& capsule, const Segment& seg)
{
	// seg の 2 端点を生成
	const Vector3 p0 = seg.origin;
	const Vector3 p1 = seg.origin + seg.diff;    // ★ diff を足して終点を導出

	// 軸線 [A,B] と 線分 [p0,p1] の最近接距離²
	const float dist2 = SegmentSegmentDist2(
		capsule.pointA, capsule.pointB,
		p0, p1);

	// 半径² と比較
	const float r2 = capsule.radius * capsule.radius;
	const float EPS = 1e-6f;                     // 浮動小数誤差吸収
	return dist2 <= r2 + EPS;
}

bool CollisionUtility::IsCollision(const Segment& segment, const Capsule& capsule)
{
	return IsCollision(capsule, segment);
}
