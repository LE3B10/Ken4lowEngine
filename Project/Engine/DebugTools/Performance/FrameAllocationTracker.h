#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

namespace Ken4lowEngine
{
	struct FrameAllocationStats
	{
		uint64_t allocationCount = 0;
		uint64_t allocatedBytes = 0;
		uint64_t peakAllocationCount = 0;
		uint64_t peakAllocatedBytes = 0;
	};

	/// <summary>
	/// Debug CRTのallocation hookを利用して、完了済み1フレームのCPU allocation回数と要求バイト数を計測します。
	/// </summary>
	class FrameAllocationTracker
	{
	public:
		static FrameAllocationTracker* GetInstance();

		void Initialize();
		void Finalize();
		void BeginFrame();
		void EndFrame();

		FrameAllocationStats GetLastFrameStats() const;
		bool IsSupported() const { return supported_; }

	private:
		FrameAllocationTracker() = default;
		~FrameAllocationTracker() = default;
		FrameAllocationTracker(const FrameAllocationTracker&) = delete;
		FrameAllocationTracker& operator=(const FrameAllocationTracker&) = delete;

#ifdef _DEBUG
		static int __cdecl AllocationHook(
			int allocationType,
			void* userData,
			size_t size,
			int blockType,
			long requestNumber,
			const unsigned char* fileName,
			int lineNumber);
		_CRT_ALLOC_HOOK previousHook_ = nullptr;
#endif

		std::atomic<uint64_t> currentAllocationCount_{ 0 };
		std::atomic<uint64_t> currentAllocatedBytes_{ 0 };
		std::atomic<uint64_t> lastAllocationCount_{ 0 };
		std::atomic<uint64_t> lastAllocatedBytes_{ 0 };
		std::atomic<uint64_t> peakAllocationCount_{ 0 };
		std::atomic<uint64_t> peakAllocatedBytes_{ 0 };
		bool initialized_ = false;
		bool supported_ = false;
	};
} // namespace Ken4lowEngine
