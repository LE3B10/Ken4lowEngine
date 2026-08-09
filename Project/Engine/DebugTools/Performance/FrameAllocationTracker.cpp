#include "FrameAllocationTracker.h"

#include <algorithm>

namespace Ken4lowEngine
{
	FrameAllocationTracker* FrameAllocationTracker::GetInstance()
	{
		static FrameAllocationTracker instance;
		return &instance;
	}

	void FrameAllocationTracker::Initialize()
	{
		if (initialized_) return;

#ifdef _DEBUG
		previousHook_ = _CrtSetAllocHook(&FrameAllocationTracker::AllocationHook);
		supported_ = true; // Debug CRTで発生するC++ allocationをフレーム単位で追跡する。
#else
		supported_ = false;
#endif
		initialized_ = true;
	}

	void FrameAllocationTracker::Finalize()
	{
		if (!initialized_) return;

#ifdef _DEBUG
		_CrtSetAllocHook(previousHook_);
		previousHook_ = nullptr;
#endif
		initialized_ = false;
		supported_ = false;
	}

	void FrameAllocationTracker::BeginFrame()
	{
		currentAllocationCount_.store(0, std::memory_order_relaxed);
		currentAllocatedBytes_.store(0, std::memory_order_relaxed);
	}

	void FrameAllocationTracker::EndFrame()
	{
		const uint64_t allocationCount = currentAllocationCount_.load(std::memory_order_relaxed);
		const uint64_t allocatedBytes = currentAllocatedBytes_.load(std::memory_order_relaxed);
		lastAllocationCount_.store(allocationCount, std::memory_order_relaxed);
		lastAllocatedBytes_.store(allocatedBytes, std::memory_order_relaxed);

		uint64_t peakCount = peakAllocationCount_.load(std::memory_order_relaxed);
		while (allocationCount > peakCount &&
			!peakAllocationCount_.compare_exchange_weak(peakCount, allocationCount, std::memory_order_relaxed))
		{
		}

		uint64_t peakBytes = peakAllocatedBytes_.load(std::memory_order_relaxed);
		while (allocatedBytes > peakBytes &&
			!peakAllocatedBytes_.compare_exchange_weak(peakBytes, allocatedBytes, std::memory_order_relaxed))
		{
		}
	}

	FrameAllocationStats FrameAllocationTracker::GetLastFrameStats() const
	{
		FrameAllocationStats stats{};
		stats.allocationCount = lastAllocationCount_.load(std::memory_order_relaxed);
		stats.allocatedBytes = lastAllocatedBytes_.load(std::memory_order_relaxed);
		stats.peakAllocationCount = peakAllocationCount_.load(std::memory_order_relaxed);
		stats.peakAllocatedBytes = peakAllocatedBytes_.load(std::memory_order_relaxed);
		return stats;
	}

#ifdef _DEBUG
	int __cdecl FrameAllocationTracker::AllocationHook(
		int allocationType,
		void* userData,
		size_t size,
		int blockType,
		long requestNumber,
		const unsigned char* fileName,
		int lineNumber)
	{
		FrameAllocationTracker* tracker = GetInstance();
		if (blockType != _CRT_BLOCK && (allocationType == _HOOK_ALLOC || allocationType == _HOOK_REALLOC))
		{
			tracker->currentAllocationCount_.fetch_add(1, std::memory_order_relaxed);
			tracker->currentAllocatedBytes_.fetch_add(static_cast<uint64_t>(size), std::memory_order_relaxed);
		}

		if (tracker->previousHook_)
		{
			return tracker->previousHook_(allocationType, userData, size, blockType, requestNumber, fileName, lineNumber);
		}
		return 1;
	}
#endif
} // namespace Ken4lowEngine
