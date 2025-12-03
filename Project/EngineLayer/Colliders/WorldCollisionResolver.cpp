#include "WorldCollisionResolver.h"

WorldCollisionResult WorldCollisionResolver::Resolve(const std::vector<AABB>& worldAABBs, const WorldCollisionSettings& s, const Vector3& oldTranslate, const Vector3& newTranslate, bool useGrounded, float* inoutJumpVelocity)
{
    WorldCollisionResult r{};

    // old/new の中心（物理中心）
    Vector3 oldCenter = oldTranslate - s.centerOffset;
    Vector3 newCenter = newTranslate - s.centerOffset;

    auto makeAABB = [&](const Vector3& c) { return AABB{ c - s.half, c + s.half }; };

    Vector3 fixedCenter = oldCenter;

    auto resolveAxis = [&](int axis, float delta)
        {
            if (delta == 0.0f) return;

            if (axis == 0) fixedCenter.x += delta;
            if (axis == 1) fixedCenter.y += delta;
            if (axis == 2) fixedCenter.z += delta;

            AABB p = makeAABB(fixedCenter);

            bool hit = false;
            float bestFix = 0.0f;
            float bestDist = FLT_MAX;

            for (const auto& w : worldAABBs)
            {
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
    resolveAxis(1, newCenter.y - oldCenter.y);

    r.fixedCenter = fixedCenter;
    return r;
}
