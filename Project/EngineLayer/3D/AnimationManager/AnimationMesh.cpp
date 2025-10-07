#include "AnimationMesh.h"
#include <ResourceManager.h>

void AnimationMesh::Initialize(ID3D12Device* device, const ModelData& modelData)
{
	subMeshes_.clear(); // 既存データをクリア
	subMeshes_.reserve(modelData.subMeshes.size()); // 必要な分だけメモリを確保

	for (const auto& subMesh : modelData.subMeshes)
	{
		SubMeshGPU gpu = {};

		// 頂点バッファの作成
		size_t vertexCount = sizeof(VertexData) * subMesh.vertices.size();
		gpu.vertexResource_ = ResourceManager::CreateBufferResource(device, vertexCount);
		gpu.vertexBufferView_.BufferLocation = gpu.vertexResource_->GetGPUVirtualAddress();
		gpu.vertexBufferView_.SizeInBytes = UINT(vertexCount);
		gpu.vertexBufferView_.StrideInBytes = sizeof(VertexData);

		// 頂点データのマッピング
		{
			VertexData* vertexData = nullptr;
			gpu.vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
			std::span<VertexData> vertexSpan{ vertexData, subMesh.vertices.size() };
			std::copy(subMesh.vertices.begin(), subMesh.vertices.end(), vertexSpan.begin());
		}

		// インデックスバッファの作成
		size_t indexCount = sizeof(uint32_t) * subMesh.indices.size();
		gpu.indexResource_ = ResourceManager::CreateBufferResource(device, indexCount);
		gpu.indexBufferView_.BufferLocation = gpu.indexResource_->GetGPUVirtualAddress();
		gpu.indexBufferView_.SizeInBytes = UINT(indexCount);
		gpu.indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

		// インデックスデータのマッピング
		{
			uint32_t* indexData = nullptr;
			gpu.indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
			std::span<uint32_t> indexSpan{ indexData, subMesh.indices.size() };
			std::copy(subMesh.indices.begin(), subMesh.indices.end(), indexSpan.begin());
		}

		subMeshes_.emplace_back(std::move(gpu)); // 作成したサブメッシュを追加
	}
}
