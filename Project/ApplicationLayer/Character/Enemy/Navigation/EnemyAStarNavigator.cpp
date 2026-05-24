#include "EnemyAStarNavigator.h"

#include <unordered_map>
#include <queue>
#include <cmath>
#include <algorithm>

namespace
{
	struct CellKey
	{
		int x = 0;
		int z = 0;

		bool operator==(const CellKey& other) const
		{
			return x == other.x && z == other.z;
		}
	};

	struct CellKeyHash
	{
		size_t operator()(const CellKey& key) const
		{
			return (static_cast<size_t>(static_cast<uint32_t>(key.x)) << 32ull)
				^ static_cast<size_t>(static_cast<uint32_t>(key.z));
		}
	};

	float HeuristicCost(int x0, int z0, int x1, int z1)
	{
		const float dx = static_cast<float>(x1 - x0);
		const float dz = static_cast<float>(z1 - z0);
		return std::sqrt(dx * dx + dz * dz);
	}

	bool SegmentIntersectsAABBXZ(const K4E::Vector3& from, const K4E::Vector3& to, const K4E::AABB& aabb)
	{
		const K4E::Vector3 d = to - from;
		float tMin = 0.0f;
		float tMax = 1.0f;
		constexpr float kEps = 1.0e-6f;
		const auto updateAxis = [&](float p, float dp, float minV, float maxV, float& inOutTMin, float& inOutTMax) -> bool
			{
				if (std::abs(dp) <= kEps)
				{
					return p >= minV && p <= maxV;
				}
				const float inv = 1.0f / dp;
				float t0 = (minV - p) * inv;
				float t1 = (maxV - p) * inv;
				if (t0 > t1) { std::swap(t0, t1); }
				inOutTMin = std::max(inOutTMin, t0);
				inOutTMax = std::min(inOutTMax, t1);
				return inOutTMin <= inOutTMax;
			};
		if (!updateAxis(from.x, d.x, aabb.min.x, aabb.max.x, tMin, tMax)) { return false; }
		if (!updateAxis(from.z, d.z, aabb.min.z, aabb.max.z, tMin, tMax)) { return false; }
		return tMax >= 0.0f && tMin <= 1.0f;
	}
}

void EnemyAStarNavigator::Reset()
{
	path_.clear();
	currentPathIndex_ = 0;
	repathTimer_ = 0.0f;
	hasLastGoal_ = false;
}
void EnemyAStarNavigator::TickTemporaryBlocks(float deltaTime)
{
	for (auto& b : temporaryBlockedAreas_) { b.remainingSec -= deltaTime; }
	temporaryBlockedAreas_.erase(std::remove_if(temporaryBlockedAreas_.begin(), temporaryBlockedAreas_.end(), [](const TemporaryBlockedArea& b) { return b.remainingSec <= 0.0f; }), temporaryBlockedAreas_.end());
}

void EnemyAStarNavigator::AddTemporaryBlockedArea(const K4E::Vector3& center, float radius, float durationSec, const char* reason)
{
	TemporaryBlockedArea b{};
	b.center = center;
	b.radius = std::max(0.1f, radius);
	b.remainingSec = std::max(0.1f, durationSec);
	b.reason = reason ? reason : "Unknown";
	temporaryBlockedAreas_.push_back(b);
}

void EnemyAStarNavigator::ClearTemporaryBlockedAreas()
{
	temporaryBlockedAreas_.clear();
}

bool EnemyAStarNavigator::GetNextWaypoint(
	const K4E::Vector3& current,
	const K4E::Vector3& goal,
	float sampleY,
	float deltaTime,
	K4E::Vector3& outWaypoint)
{
	repathTimer_ -= deltaTime;

	int goalX = 0;
	int goalZ = 0;
	WorldToCell(goal, goalX, goalZ);

	const bool goalChanged = (!hasLastGoal_ || goalX != lastGoalX_ || goalZ != lastGoalZ_);
	const bool needRepath = path_.empty() || goalChanged || repathTimer_ <= 0.0f;

	if (needRepath)
	{
		if (!RebuildPath(current, goal, sampleY))
		{
			outWaypoint = current;
			repathTimer_ = settings_.repathIntervalSec;
			return false;
		}

		lastGoalX_ = goalX;
		lastGoalZ_ = goalZ;
		hasLastGoal_ = true;
		repathTimer_ = settings_.repathIntervalSec;
	}

	if (path_.empty())
	{
		outWaypoint = current;
		return false;
	}

	while (currentPathIndex_ < static_cast<int>(path_.size()))
	{
		const K4E::Vector3 wp = path_[currentPathIndex_];
		const K4E::Vector3 d = wp - current;
		const float distSq = d.x * d.x + d.z * d.z;

		if (distSq <= settings_.waypointReachDistance * settings_.waypointReachDistance)
		{
			++currentPathIndex_;
			continue;
		}

		outWaypoint = wp;
		return true;
	}

	outWaypoint = goal;
	return true;
}

bool EnemyAStarNavigator::RebuildPath(const K4E::Vector3& current, const K4E::Vector3& goal, float sampleY)
{
	int startX = 0;
	int startZ = 0;
	int goalX = 0;
	int goalZ = 0;
	WorldToCell(current, startX, startZ);
	WorldToCell(goal, goalX, goalZ);

	if (!IsWalkableCell(startX, startZ, sampleY))
	{
		int walkableX = startX;
		int walkableZ = startZ;
		if (FindNearestWalkableCell(startX, startZ, sampleY, 4, walkableX, walkableZ))
		{
			startX = walkableX;
			startZ = walkableZ;
		}
	}

	if (!IsWalkableCell(goalX, goalZ, sampleY))
	{
		int walkableX = goalX;
		int walkableZ = goalZ;
		if (FindNearestWalkableCell(goalX, goalZ, sampleY, 6, walkableX, walkableZ))
		{
			goalX = walkableX;
			goalZ = walkableZ;
		}
	}

	const int minX = std::min(startX, goalX) - settings_.searchRangeCells;
	const int maxX = std::max(startX, goalX) + settings_.searchRangeCells;
	const int minZ = std::min(startZ, goalZ) - settings_.searchRangeCells;
	const int maxZ = std::max(startZ, goalZ) + settings_.searchRangeCells;

	std::unordered_map<CellKey, int, CellKeyHash> indexOf;
	std::vector<GridNode> nodes;
	nodes.reserve(512);

	auto addNode = [&](int x, int z, float g, float h, int parent) -> int
		{
			GridNode n{};
			n.x = x;
			n.z = z;
			n.g = g;
			n.h = h;
			n.parentIndex = parent;
			n.opened = true;
			n.closed = false;
			const int idx = static_cast<int>(nodes.size());
			nodes.push_back(n);
			indexOf[{x, z}] = idx;
			return idx;
		};

	struct OpenEntry
	{
		float f = 0.0f;
		int index = -1;
		bool operator<(const OpenEntry& rhs) const { return f > rhs.f; }
	};

	std::priority_queue<OpenEntry> open;
	const int startIndex = addNode(startX, startZ, 0.0f, HeuristicCost(startX, startZ, goalX, goalZ), -1);
	open.push({ nodes[startIndex].g + nodes[startIndex].h, startIndex });

	static constexpr int kDx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static constexpr int kDz[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	static constexpr float kCost[8] = { 1, 1, 1, 1, 1.4142135f, 1.4142135f, 1.4142135f, 1.4142135f };

	int expanded = 0;
	int goalIndex = -1;

	while (!open.empty() && expanded < settings_.maxExpandedNodes)
	{
		const OpenEntry top = open.top();
		open.pop();

		if (top.index < 0 || top.index >= static_cast<int>(nodes.size())) continue;
		GridNode& node = nodes[top.index];
		if (node.closed) continue;

		node.closed = true;
		++expanded;

		if (node.x == goalX && node.z == goalZ)
		{
			goalIndex = top.index;
			break;
		}

		for (int i = 0; i < 8; ++i)
		{
			const int nx = node.x + kDx[i];
			const int nz = node.z + kDz[i];

			if (nx < minX || nx > maxX || nz < minZ || nz > maxZ) continue;
			if (!IsWalkableCell(nx, nz, sampleY)) continue;

			if (settings_.disableCornerCutting && i >= 4)
			{
				if (!IsWalkableCell(node.x, nz, sampleY) || !IsWalkableCell(nx, node.z, sampleY))
				{
					continue;
				}
			}

			const float ng = node.g + kCost[i];
			auto it = indexOf.find({ nx, nz });
			if (it == indexOf.end())
			{
				const float nh = HeuristicCost(nx, nz, goalX, goalZ);
				const int ni = addNode(nx, nz, ng, nh, top.index);
				open.push({ ng + nh, ni });
			}
			else
			{
				GridNode& existing = nodes[it->second];
				if (existing.closed) continue;
				if (ng >= existing.g) continue;

				existing.g = ng;
				existing.parentIndex = top.index;
				open.push({ existing.g + existing.h, it->second });
			}
		}
	}

	if (goalIndex < 0)
	{
		path_.clear();
		currentPathIndex_ = 0;
		return false;
	}

	std::vector<K4E::Vector3> reversed;
	reversed.reserve(64);
	for (int i = goalIndex; i >= 0; i = nodes[i].parentIndex)
	{
		reversed.push_back(CellToWorld(nodes[i].x, nodes[i].z, current.y));
		if (nodes[i].parentIndex < 0) break;
	}

	std::reverse(reversed.begin(), reversed.end());
	path_ = std::move(reversed);
	currentPathIndex_ = (path_.size() > 1) ? 1 : 0;
	return true;
}

bool EnemyAStarNavigator::IsWalkableCell(int x, int z, float sampleY) const
{
	if (!worldAABBs_ || worldAABBs_->empty())
	{
		return true;
	}

	const K4E::Vector3 p = CellToWorld(x, z, sampleY);
	UpdateInflatedObstacleCache();

	for (const auto& aabb : inflatedObstacleAABBs_)
	{
		if (sampleY < aabb.min.y || sampleY > aabb.max.y) continue;
		if (p.x >= aabb.min.x &&
			p.x <= aabb.max.x &&
			p.z >= aabb.min.z &&
			p.z <= aabb.max.z)
		{
			return false;
		}
	}
	for (const auto& blocked : temporaryBlockedAreas_)
	{
		const float dx = p.x - blocked.center.x;
		const float dz = p.z - blocked.center.z;
		if ((dx * dx + dz * dz) <= blocked.radius * blocked.radius)
		{
			return false;
		}
	}

	return true;
}

void EnemyAStarNavigator::UpdateInflatedObstacleCache() const
{
	if (!worldAABBs_) { inflatedObstacleAABBs_.clear(); return; }
	if (obstacleCacheSourceCount_ == worldAABBs_->size() &&
		std::abs(obstacleCacheAgentRadius_ - settings_.agentRadius) <= 1.0e-5f)
	{
		return;
	}

	inflatedObstacleAABBs_.clear();
	inflatedObstacleAABBs_.reserve(worldAABBs_->size());
	for (const auto& aabb : *worldAABBs_)
	{
		K4E::AABB inflated = aabb;
		inflated.min.x -= settings_.agentRadius;
		inflated.max.x += settings_.agentRadius;
		inflated.min.z -= settings_.agentRadius;
		inflated.max.z += settings_.agentRadius;
		inflatedObstacleAABBs_.push_back(inflated);
	}
	obstacleCacheSourceCount_ = worldAABBs_->size();
	obstacleCacheAgentRadius_ = settings_.agentRadius;
}

bool EnemyAStarNavigator::IsSegmentBlockedByObstacle(
	const K4E::Vector3& from,
	const K4E::Vector3& to,
	float sampleY,
	int* outBlockedObstacleIndex) const
{
	UpdateInflatedObstacleCache();
	for (size_t i = 0; i < inflatedObstacleAABBs_.size(); ++i)
	{
		const auto& aabb = inflatedObstacleAABBs_[i];
		if (sampleY < aabb.min.y || sampleY > aabb.max.y) { continue; }
		if (SegmentIntersectsAABBXZ(from, to, aabb))
		{
			if (outBlockedObstacleIndex) { *outBlockedObstacleIndex = static_cast<int>(i); }
			return true;
		}
	}
	return false;
}

bool EnemyAStarNavigator::FindNearestWalkableCell(int centerX, int centerZ, float sampleY, int maxRadius, int& outX, int& outZ) const
{
	if (IsWalkableCell(centerX, centerZ, sampleY))
	{
		outX = centerX;
		outZ = centerZ;
		return true;
	}

	for (int r = 1; r <= maxRadius; ++r)
	{
		for (int dz = -r; dz <= r; ++dz)
		{
			for (int dx = -r; dx <= r; ++dx)
			{
				if (std::abs(dx) != r && std::abs(dz) != r) continue;

				const int x = centerX + dx;
				const int z = centerZ + dz;
				if (!IsWalkableCell(x, z, sampleY)) continue;

				outX = x;
				outZ = z;
				return true;
			}
		}
	}

	return false;
}

K4E::Vector3 EnemyAStarNavigator::CellToWorld(int x, int z, float y) const
{
	return {
		static_cast<float>(x) * settings_.cellSize,
		y,
		static_cast<float>(z) * settings_.cellSize
	};
}

void EnemyAStarNavigator::WorldToCell(const K4E::Vector3& p, int& outX, int& outZ) const
{
	outX = static_cast<int>(std::round(p.x / settings_.cellSize));
	outZ = static_cast<int>(std::round(p.z / settings_.cellSize));
}
