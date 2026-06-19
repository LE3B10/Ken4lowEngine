#include "WorldCollisionResolver.h"

#include <cfloat>
#include <cmath>

namespace Ken4lowEngine
{
	namespace
	{
		bool IsSameAABB(const AABB& lhs, const AABB& rhs)
		{
			constexpr float kEpsilon = 0.0001f;
			return std::fabs(lhs.min.x - rhs.min.x) <= kEpsilon &&
				std::fabs(lhs.min.y - rhs.min.y) <= kEpsilon &&
				std::fabs(lhs.min.z - rhs.min.z) <= kEpsilon &&
				std::fabs(lhs.max.x - rhs.max.x) <= kEpsilon &&
				std::fabs(lhs.max.y - rhs.max.y) <= kEpsilon &&
				std::fabs(lhs.max.z - rhs.max.z) <= kEpsilon;
		}

		bool IsObstacleBroadPhaseAABB(const AABB& candidate, const std::vector<AABB>* obstacleAABBs)
		{
			// Obstacleの包み込みAABBは最終押し戻しから除外し、対応するOBBへNarrowPhaseを任せる。
			if (!obstacleAABBs)
			{
				return false;
			}
			for (const AABB& obstacleAABB : *obstacleAABBs)
			{
				if (IsSameAABB(candidate, obstacleAABB))
				{
					return true;
				}
			}
			return false;
		}

		size_t ResolveObstacleOBBsXZ(
			const std::vector<AABB>* obstacleBroadPhaseAABBs,
			const std::vector<OBB>* obstacleOBBs,
			const WorldCollisionSettings& settings,
			float targetCenterY,
			Vector3& fixedCenter)
		{
			if (!obstacleOBBs)
			{
				return 0;
			}

			size_t hitCount = 0;
			for (size_t obstacleIndex = 0; obstacleIndex < obstacleOBBs->size(); ++obstacleIndex)
			{
				const OBB& obstacle = (*obstacleOBBs)[obstacleIndex];
				if (obstacleBroadPhaseAABBs && obstacleIndex < obstacleBroadPhaseAABBs->size())
				{
					const Vector3 playerCenter{ fixedCenter.x, targetCenterY, fixedCenter.z };
					const AABB playerAABB{ playerCenter - settings.half, playerCenter + settings.half };
					const AABB& obstacleBroadPhase = (*obstacleBroadPhaseAABBs)[obstacleIndex];
					// 包み込みAABBで候補を絞り、重なる障害物だけ回転OBBのXZ SATへ進める。
					if (playerAABB.max.x < obstacleBroadPhase.min.x || playerAABB.min.x > obstacleBroadPhase.max.x ||
						playerAABB.max.y < obstacleBroadPhase.min.y || playerAABB.min.y > obstacleBroadPhase.max.y ||
						playerAABB.max.z < obstacleBroadPhase.min.z || playerAABB.min.z > obstacleBroadPhase.max.z)
					{
						continue;
					}
				}

				const float obstacleYExtent =
					std::fabs(obstacle.orientations[0].y) * obstacle.size.x +
					std::fabs(obstacle.orientations[1].y) * obstacle.size.y +
					std::fabs(obstacle.orientations[2].y) * obstacle.size.z;
				if (targetCenterY + settings.half.y < obstacle.center.y - obstacleYExtent ||
					targetCenterY - settings.half.y > obstacle.center.y + obstacleYExtent)
				{
					continue;
				}

				Vector3 candidateAxes[5] = {
					{ 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f },
					{}, {}, {},
				};
				for (int i = 0; i < 3; ++i)
				{
					const Vector3 projectedAxis{ obstacle.orientations[i].x, 0.0f, obstacle.orientations[i].z };
					if (Vector3::LengthSquared(projectedAxis) > 0.000001f)
					{
						// 投影されたOBB辺の法線をXZ分離軸に加え、斜め壁の向きに沿った押し戻しを求める。
						candidateAxes[i + 2] = Vector3::Normalize({ -projectedAxis.z, 0.0f, projectedAxis.x });
					}
				}

				bool overlaps = true;
				float minimumPenetration = FLT_MAX;
				Vector3 minimumAxis{};
				float minimumDistance = 0.0f;
				for (const Vector3& rawAxis : candidateAxes)
				{
					if (Vector3::LengthSquared(rawAxis) <= 0.000001f)
					{
						continue;
					}

					const Vector3 axis = Vector3::Normalize(rawAxis);
					const float obstacleRadius =
						std::fabs(Vector3::Dot(obstacle.orientations[0] * obstacle.size.x, axis)) +
						std::fabs(Vector3::Dot(obstacle.orientations[1] * obstacle.size.y, axis)) +
						std::fabs(Vector3::Dot(obstacle.orientations[2] * obstacle.size.z, axis));
					const float playerRadius = std::fabs(axis.x) * settings.half.x + std::fabs(axis.z) * settings.half.z;
					const float distance = Vector3::Dot(fixedCenter - obstacle.center, axis);
					const float penetration = obstacleRadius + playerRadius - std::fabs(distance);
					if (penetration <= 0.0f)
					{
						overlaps = false;
						break;
					}
					if (penetration < minimumPenetration)
					{
						minimumPenetration = penetration;
						minimumAxis = axis;
						minimumDistance = distance;
					}
				}

				if (!overlaps || minimumPenetration == FLT_MAX)
				{
					continue;
				}

				const float direction = minimumDistance >= 0.0f ? 1.0f : -1.0f;
				// OBBとの最小XZ侵入量だけを戻し、床のY解決は既存AABB処理へ残す。
				fixedCenter += minimumAxis * ((minimumPenetration + settings.eps) * direction);
				++hitCount;
			}

			return hitCount;
		}
	}

	WorldCollisionResult WorldCollisionResolver::Resolve(
		const std::vector<AABB>& worldAABBs,
		const WorldCollisionSettings& s,
		const Vector3& oldTranslate,
		const Vector3& newTranslate,
		bool useGrounded,
		float* inoutJumpVelocity,
		const std::vector<AABB>* obstacleBroadPhaseAABBs,
		const std::vector<OBB>* obstacleOBBs)
	{
		WorldCollisionResult r{};

		// StageCollision専用: 汎用CollisionManagerのイベント通知ではなく、移動後座標を静的AABBから解決する。

		// old/new の中心（物理中心）
		Vector3 oldCenter = oldTranslate - s.centerOffset;
		Vector3 newCenter = newTranslate - s.centerOffset;

		auto buildMovingBodyAABB = [&](const Vector3& c) { return AABB{ c - s.half, c + s.half }; };

		Vector3 fixedCenter = oldCenter;

		auto resolveAxis = [&](int axis, float delta)
			{
				if (delta == 0.0f) return;

				if (axis == 0) fixedCenter.x += delta;
				if (axis == 1) fixedCenter.y += delta;
				if (axis == 2) fixedCenter.z += delta;

				AABB p = buildMovingBodyAABB(fixedCenter);

				bool hit = false;
				float bestFix = 0.0f;
				float bestDist = FLT_MAX;

				for (const auto& w : worldAABBs)
				{
					if (IsObstacleBroadPhaseAABB(w, obstacleBroadPhaseAABBs))
					{
						continue;
					}
					// 移動体AABBと静的ステージAABBの重なりだけを見る。ObjectChannel/Responseはここでは扱わない。
					if (!(p.min.x <= w.max.x && p.max.x >= w.min.x &&
						p.min.y <= w.max.y && p.max.y >= w.min.y &&
						p.min.z <= w.max.z && p.max.z >= w.min.z))
					{
						continue;
					}

					float cand = 0.0f;
					bool valid = false;

					if (axis == 0)
					{
						if (oldCenter.x + s.half.x <= w.min.x) { cand = (w.min.x - s.half.x) - s.eps; valid = true; }
						else if (oldCenter.x - s.half.x >= w.max.x) { cand = (w.max.x + s.half.x) + s.eps; valid = true; }
						else
						{
							float dMin = std::fabs((w.min.x - s.half.x) - oldCenter.x);
							float dMax = std::fabs((w.max.x + s.half.x) - oldCenter.x);
							cand = (dMin <= dMax) ? (w.min.x - s.half.x - s.eps) : (w.max.x + s.half.x + s.eps);
							valid = true;
						}

						if (valid)
						{
							float dist = std::fabs(cand - fixedCenter.x);
							if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
						}
					}
					else if (axis == 2)
					{
						if (oldCenter.z + s.half.z <= w.min.z) { cand = (w.min.z - s.half.z) - s.eps; valid = true; }
						else if (oldCenter.z - s.half.z >= w.max.z) { cand = (w.max.z + s.half.z) + s.eps; valid = true; }
						else
						{
							float dMin = std::fabs((w.min.z - s.half.z) - oldCenter.z);
							float dMax = std::fabs((w.max.z + s.half.z) - oldCenter.z);
							cand = (dMin <= dMax) ? (w.min.z - s.half.z - s.eps) : (w.max.z + s.half.z + s.eps);
							valid = true;
						}

						if (valid)
						{
							float dist = std::fabs(cand - fixedCenter.z);
							if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
						}
					}
					else // axis == 1
					{
						if (oldCenter.y - s.half.y >= w.max.y) { cand = (w.max.y + s.half.y) + s.eps; valid = true; }     // 床
						else if (oldCenter.y + s.half.y <= w.min.y) { cand = (w.min.y - s.half.y) - s.eps; valid = true; } // 天井
						else
						{
							float dFloor = std::fabs((w.max.y + s.half.y) - oldCenter.y);
							float dCeil = std::fabs((w.min.y - s.half.y) - oldCenter.y);
							cand = (dFloor <= dCeil) ? (w.max.y + s.half.y + s.eps) : (w.min.y - s.half.y - s.eps);
							valid = true;
						}

						if (valid)
						{
							float dist = std::fabs(cand - fixedCenter.y);
							if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
						}
					}
				}

				if (hit)
				{
					// 押し戻しは軸ごとに最も近い補正位置だけを採用し、既存の移動安定性を保つ。
					if (axis == 0) fixedCenter.x = bestFix;
					if (axis == 2) fixedCenter.z = bestFix;
					if (axis == 1)
					{
						fixedCenter.y = bestFix;

						// Player向け：床に落ちたら grounded / 上向き衝突なら上向き速度を0
						if (useGrounded && inoutJumpVelocity)
						{
							if (delta < 0.0f) { r.grounded = true; *inoutJumpVelocity = 0.0f; }
							else if (*inoutJumpVelocity > 0.0f) { *inoutJumpVelocity = 0.0f; }
						}
					}
				}
			};

		// Boss/Enemyと同じ順でもOK（Playerに合わせて X,Z,Y の順が安定）
		resolveAxis(0, newCenter.x - oldCenter.x);
		resolveAxis(2, newCenter.z - oldCenter.z);
		// Obstacle系だけは包み込みAABBではなく回転OBBで最終的なXZ押し戻しを行う。
		r.obbHitCount = ResolveObstacleOBBsXZ(obstacleBroadPhaseAABBs, obstacleOBBs, s, newCenter.y, fixedCenter);
		resolveAxis(1, newCenter.y - oldCenter.y);

		r.fixedCenter = fixedCenter;
		return r;
	}

} // namespace Ken4lowEngine
