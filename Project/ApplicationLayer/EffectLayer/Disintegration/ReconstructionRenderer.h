#pragma once
#include "ReconstructionBlock.h"
#include "DX12Include.h"
#include "Matrix4x4.h"

#include <vector>

/// -------------------------------------------------------------
/// 既存Particleに依存しない再構築エフェクト専用レンダラー
/// -------------------------------------------------------------
class ReconstructionRenderer
{
public:
	~ReconstructionRenderer();

	void Draw(const std::vector<ReconstructionBlock>& particles);

private:
	struct CubeVertex
	{
		K4E::Vector3 position{};
	};

	struct InstanceData
	{
		K4E::Matrix4x4 world{};
		K4E::Vector4 color{};
	};

	struct ViewProjectionData
	{
		K4E::Matrix4x4 viewProjection{};
	};

	void Initialize();
	void CreatePipeline();
	void CreateCubeMesh();
	void EnsureInstanceCapacity(size_t requiredCapacity);
	void UpdateViewProjection();
	void ReleaseSrv();

	bool initialized_ = false;
	uint32_t srvIndex_ = UINT32_MAX;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	InstanceData* instanceData_ = nullptr;
	ViewProjectionData* viewProjectionData_ = nullptr;
	size_t instanceCapacity_ = 0;
	UINT indexCount_ = 0;
};
