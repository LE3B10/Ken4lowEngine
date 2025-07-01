#pragma once
#include "DX12Include.h"
#include "ModelData.h"

#include <memory>


class AnimationMesh
{
public:
	void Initialize(ID3D12Device* device, const ModelData& modelData);

	void DrawSkippingJoint(int skipJointIndex);

	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
};

