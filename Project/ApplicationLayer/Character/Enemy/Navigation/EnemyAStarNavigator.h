#pragma once

#include "AABB.h"
#include "Vector3.h"

#include <vector>
#include <cstdint>
#include <string>

namespace K4E = ::Ken4lowEngine;

class EnemyAStarNavigator
{
public:
	struct Settings
	{
		float cellSize = 1.5f;
		float agentRadius = 0.7f;
		int searchRangeCells = 28;
		int maxExpandedNodes = 1200;
		float waypointReachDistance = 0.6f;
		float repathIntervalSec = 0.25f;
		bool disableCornerCutting = true;
	};
	struct TemporaryBlockedArea
	{
		K4E::Vector3 center{};
		float radius = 1.0f;
		float remainingSec = 0.0f;
		std::string reason{};
	};

public:
	void SetWorldAABBs(const std::vector<K4E::AABB>* worldAABBs) { worldAABBs_ = worldAABBs; }
	void SetSettings(const Settings& settings) { settings_ = settings; }
	const Settings& GetSettings() const { return settings_; }

	void Reset();
	bool GetNextWaypoint(
		const K4E::Vector3& current,
		const K4E::Vector3& goal,
		float sampleY,
		float deltaTime,
		K4E::Vector3& outWaypoint);
	bool IsSegmentBlockedByObstacle(
		const K4E::Vector3& from,
		const K4E::Vector3& to,
		float sampleY,
		int* outBlockedObstacleIndex = nullptr) const;
	void TickTemporaryBlocks(float deltaTime);
	void AddTemporaryBlockedArea(const K4E::Vector3& center, float radius, float durationSec, const char* reason);
	void ClearTemporaryBlockedAreas();
	const std::vector<K4E::AABB>& GetInflatedObstacleAABBs() const { return inflatedObstacleAABBs_; }
	const std::vector<K4E::Vector3>& GetCurrentPath() const { return path_; }
	const std::vector<TemporaryBlockedArea>& GetTemporaryBlockedAreas() const { return temporaryBlockedAreas_; }
	int GetCurrentPathIndex() const { return currentPathIndex_; }
	float GetRepathTimer() const { return repathTimer_; }

private:
	struct GridNode
	{
		int x = 0;
		int z = 0;
		float g = 0.0f;
		float h = 0.0f;
		int parentIndex = -1;
		bool closed = false;
		bool opened = false;
	};

	bool RebuildPath(const K4E::Vector3& current, const K4E::Vector3& goal, float sampleY);
	bool IsWalkableCell(int x, int z, float sampleY) const;
	void UpdateInflatedObstacleCache() const;
	bool FindNearestWalkableCell(int centerX, int centerZ, float sampleY, int maxRadius, int& outX, int& outZ) const;
	K4E::Vector3 CellToWorld(int x, int z, float y) const;
	void WorldToCell(const K4E::Vector3& p, int& outX, int& outZ) const;

private:
	const std::vector<K4E::AABB>* worldAABBs_ = nullptr;
	Settings settings_{};

	std::vector<K4E::Vector3> path_{};
	mutable std::vector<K4E::AABB> inflatedObstacleAABBs_{};
	mutable size_t obstacleCacheSourceCount_ = 0;
	mutable float obstacleCacheAgentRadius_ = -1.0f;
	std::vector<TemporaryBlockedArea> temporaryBlockedAreas_{};
	int currentPathIndex_ = 0;
	float repathTimer_ = 0.0f;
	int lastGoalX_ = 0;
	int lastGoalZ_ = 0;
	bool hasLastGoal_ = false;
};
