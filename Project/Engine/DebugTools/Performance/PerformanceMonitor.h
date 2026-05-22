#pragma once

#include <array>
#include <cstddef>

namespace Ken4lowEngine
{
	struct PerformanceStats
	{
		float fps = 0.0f;
		float frameTimeMs = 0.0f;
		float cpuUsagePercent = 0.0f;
		float processCpuUsagePercent = 0.0f;
		float memoryUsageMB = 0.0f;
	};

	class PerformanceMonitor
	{
	public:
		static constexpr size_t kHistorySize = 240;

		void Update(float deltaSeconds, float fps);
		const PerformanceStats& GetStats() const { return stats_; }
		const std::array<float, kHistorySize>& GetFpsHistory() const { return fpsHistory_; }
		const std::array<float, kHistorySize>& GetFrameTimeHistory() const { return frameTimeHistory_; }

	private:
		void UpdateSystemStats();
		float ComputeCpuUsagePercent();
		float ComputeProcessCpuUsagePercent();

	private:
		PerformanceStats stats_{};
		std::array<float, kHistorySize> fpsHistory_{};
		std::array<float, kHistorySize> frameTimeHistory_{};
		size_t historyWriteIndex_ = 0;

		double statsRefreshAccumulator_ = 0.0;

		unsigned long long prevSystemIdle_ = 0;
		unsigned long long prevSystemKernel_ = 0;
		unsigned long long prevSystemUser_ = 0;
		unsigned long long prevProcessKernel_ = 0;
		unsigned long long prevProcessUser_ = 0;
	};
}
