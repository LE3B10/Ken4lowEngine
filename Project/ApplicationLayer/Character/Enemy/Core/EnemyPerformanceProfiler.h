#pragma once

#include <array>
#include <cstddef>

#ifdef _DEBUG
#include <chrono>
#endif

// 敵Updateの負荷原因を特定するため、AI・移動・当たり判定ごとに処理時間を計測する。
class EnemyPerformanceProfiler
{
public:
	enum class EnemyType : size_t { Melee, MidRange, Count };
	enum class Section : size_t { AI, Move, Attack, Collision, Navigation, Transform, BulletSpawnAttackCheck, Count };

	struct TypeStats
	{
		double totalMs = 0.0;
		std::array<double, static_cast<size_t>(Section::Count)> sectionMs{};
		int enemyCount = 0;
	};

	struct FrameStats
	{
		std::array<TypeStats, static_cast<size_t>(EnemyType::Count)> types{};
		std::array<double, static_cast<size_t>(Section::Count)> globalSectionMs{};
		double GetTotalMs() const { return types[0].totalMs + types[1].totalMs; }
		int GetEnemyCount() const { return types[0].enemyCount + types[1].enemyCount; }
		double GetSectionMs(Section section) const
		{
			const size_t index = static_cast<size_t>(section);
			return types[0].sectionMs[index] + types[1].sectionMs[index] + globalSectionMs[index];
		}
	};

	static void BeginFrame() { stats_ = {}; }
	static const FrameStats& GetFrameStats() { return stats_; }
	static void AddFrameSectionMs(Section section, double milliseconds)
	{
#ifdef _DEBUG
		stats_.globalSectionMs[static_cast<size_t>(section)] += milliseconds;
#else
		(void)section;
		(void)milliseconds;
#endif
	}

	class EnemyUpdateScope
	{
	public:
		explicit EnemyUpdateScope(EnemyType type) : previousType_(currentType_), previousUpdating_(isUpdating_)
		{
			currentType_ = type;
			isUpdating_ = true;
#ifdef _DEBUG
			start_ = Clock::now();
#endif
		}
		~EnemyUpdateScope()
		{
#ifdef _DEBUG
			auto& typeStats = stats_.types[static_cast<size_t>(currentType_)];
			typeStats.totalMs += ToMs(Clock::now() - start_);
			++typeStats.enemyCount;
#endif
			currentType_ = previousType_;
			isUpdating_ = previousUpdating_;
		}
	private:
		EnemyType previousType_;
		bool previousUpdating_;
#ifdef _DEBUG
		using Clock = std::chrono::steady_clock;
		Clock::time_point start_{};
#endif
	};

	class SectionScope
	{
	public:
		explicit SectionScope(Section section) : section_(section)
		{
#ifdef _DEBUG
			active_ = isUpdating_;
			if (active_) start_ = Clock::now();
#endif
		}
		~SectionScope()
		{
#ifdef _DEBUG
			if (active_) stats_.types[static_cast<size_t>(currentType_)].sectionMs[static_cast<size_t>(section_)] += ToMs(Clock::now() - start_);
#endif
		}
	private:
		Section section_;
#ifdef _DEBUG
		using Clock = std::chrono::steady_clock;
		Clock::time_point start_{};
		bool active_ = false;
#endif
	};

	static bool IsEnemyUpdating() { return isUpdating_; }

private:
#ifdef _DEBUG
	template<class Duration>
	static double ToMs(Duration duration) { return std::chrono::duration<double, std::milli>(duration).count(); }
#endif
	static FrameStats stats_;
	inline static thread_local EnemyType currentType_ = EnemyType::Melee;
	inline static thread_local bool isUpdating_ = false;
};

inline EnemyPerformanceProfiler::FrameStats EnemyPerformanceProfiler::stats_{};
