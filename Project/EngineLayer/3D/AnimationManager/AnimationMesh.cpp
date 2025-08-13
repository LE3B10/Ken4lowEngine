#include "AnimationMesh.h"
#include <ResourceManager.h>

void AnimationMesh::Initialize(ID3D12Device* device, const ModelData& modelData)
{
    // 頂点バッファの作成
    vertexResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点データのマッピング
    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::span<VertexData> vertexSpan{ vertexData, modelData.vertices.size() };
    std::copy(modelData.vertices.begin(), modelData.vertices.end(), vertexSpan.begin());

    // インデックスバッファの作成
    indexResource_ = ResourceManager::CreateBufferResource(device, sizeof(uint32_t) * modelData.indices.size());
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// インデックスデータのマッピング
    uint32_t* indexData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    std::span<uint32_t> indexSpan{ indexData, modelData.indices.size() };
    std::copy(modelData.indices.begin(), modelData.indices.end(), indexSpan.begin());
}
