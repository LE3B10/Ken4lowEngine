#pragma once
#include "DX12Include.h"
#include "VertexData.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　メッシュデータクラス
	/// -------------------------------------------------------------
	class Mesh
	{
	public:
		void Initialize(const std::vector<VertexData>& modelVertices, const std::vector<uint32_t>& modelIndices);
		void Draw();
		void DrawInstanced(UINT instanceCount, UINT startInstanceLocation = 0);

		uint32_t GetIndexCount() const { return static_cast<uint32_t>(indices.size()); }
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(vertices.size()); }

		uint64_t GetEstimatedCpuMemoryBytes() const
		{
			// vectorのcapacityを使い、Meshが保持し続けるCPU側頂点・Index領域を概算する。
			return static_cast<uint64_t>(vertices.capacity()) * sizeof(VertexData) +
				static_cast<uint64_t>(indices.capacity()) * sizeof(uint32_t);
		}

		uint64_t GetEstimatedGpuMemoryBytes() const
		{
			uint64_t bytes = 0;
			if (vertexResource) bytes += vertexResource->GetDesc().Width;
			if (indexResource) bytes += indexResource->GetDesc().Width;
			return bytes; // Committed Bufferの論理サイズを集計し、Heap alignmentやdriver residencyは含めない。
		}

	private:
		ComPtr<ID3D12Resource> vertexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
		std::vector<VertexData> vertices = {};
		ComPtr<ID3D12Resource> indexResource = nullptr;
		D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
		std::vector<uint32_t> indices = {};
	};
} // namespace Ken4lowEngine
