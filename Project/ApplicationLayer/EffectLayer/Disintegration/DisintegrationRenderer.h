#pragma once
#include "DisintegrationParticle.h"
#include "DX12Include.h"
#include "Matrix4x4.h"

#include <vector>

/// -------------------------------------------------------------
/// 既存Particleに依存しない崩壊エフェクト専用レンダラー
/// -------------------------------------------------------------
class DisintegrationRenderer
{
public:
	~DisintegrationRenderer();

	void Draw(const std::vector<DisintegrationParticle>& particles, float globalAlpha = 1.0f);

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
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> retiredInstanceBuffers_;
	std::vector<uint32_t> retiredSrvIndices_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	InstanceData* instanceData_ = nullptr;
	ViewProjectionData* viewProjectionData_ = nullptr;
	size_t instanceCapacity_ = 0;
	UINT indexCount_ = 0;
};
