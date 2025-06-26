#pragma once
#include "Matrix4x4.h"
#include "Sphere.h"
#include "Plane.h"
#include "Segment.h"
#include "Triangle.h"
#include "AABB.h"
#include "OBB.h"
#include "Capsule.h"


//// -------------------------------------------------------------
///						衝突判定ユーティリティ
/// -------------------------------------------------------------
class CollisionUtility
{
public: /// ---------- メンバ関数 ---------- ///

	// 球と球の衝突判定
	static bool IsCollision(const Sphere& s1, const Sphere& s2);

	// 球と平面の衝突判定
	static bool IsCollision(const Sphere& sphere, const Plane& plane);

	// 線分と平面の衝突判定
	static bool IsCollision(const Segment& segment, const Plane& plane);

	// 線分と三角形の衝突判定
	static bool IsCollision(const Triangle& triangle, const Segment& segment);

	// 球と三角形の衝突判定
	static bool IsCollision(const AABB& aabb1, const AABB& aabb2);

	// AABBと球の衝突判定
	static bool IsCollision(const AABB& aabb, const Sphere& sphere);

	// AABBと線分の衝突判定
	static bool IsCollision(const AABB& aabb, const Segment& segment);

	// AABBとOBBの衝突判定
	static bool IsCollision(const OBB& obb, const Sphere& sphere);

	// OBBと線分の衝突判定
	static bool IsCollision(const OBB& obb, const Segment& segment);

	// 線分とOBBの衝突判定
	static bool IsCollision(const Segment& segment, const OBB& obb);

	// OBBとOBBの衝突判定
	static bool IsCollision(const OBB& obb1, const OBB& obb2);

	// CapsuleとCapsuleの衝突判定
	static bool IsCollision(const Capsule& capsule1, const Capsule& capsule2);

	// CapsuleとOBBの衝突判定
	static bool IsCollision(const Capsule& capsule, const OBB& obb);

	// CapsuleとAABBの衝突判定
	static bool IsCollision(const Capsule& capsule, const AABB& aabb);

	// CapsuleとSphereの衝突判定
	static bool IsCollision(const Capsule& capsule, const Sphere& sphere);

	// CapsuleとSegmentの衝突判定
	static bool IsCollision(const Capsule& capsule, const Segment& segment);
	static bool IsCollision(const Segment& segment, const Capsule& capsule);

	//// OBBとAABBの衝突判定
	//static bool IsCollision(const OBB& obb, const AABB& aabb);

	//// OBBと三角形の衝突判定
	//static bool IsCollision(const OBB& obb, const Triangle& triangle);

	//// OBBと球の衝突判定
	//static bool IsCollision(const OBB& obb, const Sphere& sphere);
};
