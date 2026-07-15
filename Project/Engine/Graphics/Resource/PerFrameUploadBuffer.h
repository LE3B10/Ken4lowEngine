#pragma once

#include "DX12Include.h"
#include "ResourceManager.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	template<class T>
	class PerFrameUploadBuffer
	{
	public:
		void Initialize(ID3D12Device* device, uint32_t frameCount)
		{
			Finalize();
			if (!device || frameCount == 0) frameCount = 1;

			frames_.resize(frameCount);
			for (FrameBuffer& frame : frames_)
			{
				frame.resource = ResourceManager::CreateBufferResource(device, sizeof(T));
				if (!frame.resource) continue;
				const HRESULT hr = frame.resource->Map(0, nullptr, reinterpret_cast<void**>(&frame.mappedData));
				assert(SUCCEEDED(hr));
				if (SUCCEEDED(hr) && frame.mappedData)
				{
					*frame.mappedData = T{}; // 各FrameResourceを独立した初期値で開始し、GPU参照中の上書きを防ぐ。
				}
			}
		}

		void Finalize()
		{
			for (FrameBuffer& frame : frames_)
			{
				if (frame.resource && frame.mappedData)
				{
					frame.resource->Unmap(0, nullptr);
				}
				frame.mappedData = nullptr;
				frame.resource.Reset();
			}
			frames_.clear();
		}

		void WriteFrame(uint32_t frameIndex, const T& value)
		{
			T* mappedData = GetMappedData(frameIndex);
			if (mappedData) *mappedData = value;
		}

		void WriteAll(const T& value)
		{
			for (FrameBuffer& frame : frames_)
			{
				if (frame.mappedData) *frame.mappedData = value;
			}
		}

		T* GetMappedData(uint32_t frameIndex)
		{
			return frames_.empty() ? nullptr : frames_[ResolveIndex(frameIndex)].mappedData;
		}

		const T* GetMappedData(uint32_t frameIndex) const
		{
			return frames_.empty() ? nullptr : frames_[ResolveIndex(frameIndex)].mappedData;
		}

		D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(uint32_t frameIndex) const
		{
			if (frames_.empty()) return 0;
			const auto& resource = frames_[ResolveIndex(frameIndex)].resource;
			return resource ? resource->GetGPUVirtualAddress() : 0;
		}

		uint32_t GetFrameCount() const { return static_cast<uint32_t>(frames_.size()); }

	private:
		struct FrameBuffer
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			T* mappedData = nullptr;
		};

		std::size_t ResolveIndex(uint32_t frameIndex) const
		{
			return frames_.empty() ? 0 : static_cast<std::size_t>(frameIndex % frames_.size());
		}

		std::vector<FrameBuffer> frames_;
	};
} // namespace Ken4lowEngine
