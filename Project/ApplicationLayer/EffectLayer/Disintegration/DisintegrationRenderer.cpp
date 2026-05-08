#define NOMINMAX
#include "DisintegrationRenderer.h"

#include "BlendStateFactory.h"
#include "CameraManager.h"
#include "DebugCamera.h"
#include "DirectXCommon.h"
#include "PipelineStatePresets.h"
#include "BlendModeType.h"
#include "ResourceManager.h"
#include "SRVManager.h"
#include "ShaderCompiler.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

namespace
{
	constexpr UINT kRootViewProjection = 0;
	constexpr UINT kRootInstances = 1;

	K4E::Matrix4x4 MakeWorldMatrix(const DisintegrationParticle& particle)
	{
		const K4E::Vector3 scale = particle.scale * particle.size;
		return K4E::Matrix4x4::MakeAffineMatrix(scale, particle.rotation, particle.position);
	}
}

DisintegrationRenderer::~DisintegrationRenderer()
{
	ReleaseSrv();
}

void DisintegrationRenderer::Draw(const std::vector<DisintegrationParticle>& particles)
{
	if (particles.empty()) { return; }
	if (!initialized_) { Initialize(); }

	size_t visibleCount = 0;
	for (const auto& particle : particles)
	{
		if (particle.alive && particle.alpha > 0.0f) { ++visibleCount; }
	}
	if (visibleCount == 0) { return; }

	EnsureInstanceCapacity(visibleCount);
	UpdateViewProjection();

	size_t writeIndex = 0;
	for (const auto& particle : particles)
	{
		if (!particle.alive || particle.alpha <= 0.0f) { continue; }

		K4E::Vector4 color = particle.color;
		color.w *= particle.alpha;
		instanceData_[writeIndex].world = MakeWorldMatrix(particle);
		instanceData_[writeIndex].color = color;
		++writeIndex;
	}

	auto* dxCommon = K4E::DirectXCommon::GetInstance();
	auto* commandList = dxCommon->GetCommandManager()->GetCommandList();
	K4E::SRVManager::GetInstance()->PreDraw();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(kRootViewProjection, viewProjectionBuffer_->GetGPUVirtualAddress());
	K4E::SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(kRootInstances, srvIndex_);
	commandList->DrawIndexedInstanced(indexCount_, static_cast<UINT>(visibleCount), 0, 0, 0);
}

void DisintegrationRenderer::Initialize()
{
	CreatePipeline();
	CreateCubeMesh();

	auto* device = K4E::DirectXCommon::GetInstance()->GetDevice();
	viewProjectionBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(ViewProjectionData));
	viewProjectionBuffer_->SetName(L"DisintegrationRenderer_ViewProjectionBuffer");
	viewProjectionBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjectionData_));

	EnsureInstanceCapacity(1);
	initialized_ = true;
}

void DisintegrationRenderer::CreatePipeline()
{
	auto* dxCommon = K4E::DirectXCommon::GetInstance();
	auto* device = dxCommon->GetDevice();

	std::array<D3D12_DESCRIPTOR_RANGE, 1> ranges{};
	ranges[0].BaseShaderRegister = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	std::array<D3D12_ROOT_PARAMETER, 2> rootParameters{};
	rootParameters[kRootViewProjection].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kRootViewProjection].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[kRootViewProjection].Descriptor.ShaderRegister = 0;

	rootParameters[kRootInstances].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kRootInstances].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kRootInstances].DescriptorTable.pDescriptorRanges = ranges.data();
	rootParameters[kRootInstances].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
	rootSignatureDesc.pParameters = rootParameters.data();

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	const K4E::ShaderDescriptor vsDesc{ L"DisintegrationBlockVS", L"Resources/Shaders/Disintegration/DisintegrationBlock.VS.hlsl", L"main", L"vs_6_0", K4E::ShaderStage::Vertex, K4E::RootSignatureType::Object3D };
	const K4E::ShaderDescriptor psDesc{ L"DisintegrationBlockPS", L"Resources/Shaders/Disintegration/DisintegrationBlock.PS.hlsl", L"main", L"ps_6_0", K4E::ShaderStage::Pixel, K4E::RootSignatureType::Object3D };
	auto vertexShader = K4E::ShaderCompiler::CompileShader(vsDesc, dxCommon->GetDXCCompilerManager());
	auto pixelShader = K4E::ShaderCompiler::CompileShader(psDesc, dxCommon->GetDXCCompilerManager());

	D3D12_INPUT_ELEMENT_DESC inputElement{};
	inputElement.SemanticName = "POSITION";
	inputElement.SemanticIndex = 0;
	inputElement.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElement.InputSlot = 0;
	inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { &inputElement, 1 };
	psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	psoDesc.BlendState.RenderTarget[0] = K4E::BlendStateFactory::GetInstance()->GetBlendDesc(K4E::BlendMode::kBlendModeNormal);
	psoDesc.RasterizerState = K4E::PipelineStatePresets::MakeRasterizerCullNone();
	psoDesc.DepthStencilState = K4E::PipelineStatePresets::MakeDepthReadOnly();
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void DisintegrationRenderer::CreateCubeMesh()
{
	static const std::array<CubeVertex, 8> vertices{
		CubeVertex{ { -0.5f, -0.5f, -0.5f } }, CubeVertex{ { -0.5f,  0.5f, -0.5f } },
		CubeVertex{ {  0.5f,  0.5f, -0.5f } }, CubeVertex{ {  0.5f, -0.5f, -0.5f } },
		CubeVertex{ { -0.5f, -0.5f,  0.5f } }, CubeVertex{ { -0.5f,  0.5f,  0.5f } },
		CubeVertex{ {  0.5f,  0.5f,  0.5f } }, CubeVertex{ {  0.5f, -0.5f,  0.5f } },
	};
	static const std::array<uint32_t, 36> indices{
		0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
		4, 5, 1, 4, 1, 0, 3, 2, 6, 3, 6, 7,
		1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7,
	};

	auto* device = K4E::DirectXCommon::GetInstance()->GetDevice();
	vertexBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(CubeVertex) * vertices.size());
	vertexBuffer_->SetName(L"DisintegrationRenderer_CubeVertexBuffer");
	void* vertexMapped = nullptr;
	vertexBuffer_->Map(0, nullptr, &vertexMapped);
	std::memcpy(vertexMapped, vertices.data(), sizeof(CubeVertex) * vertices.size());

	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(CubeVertex) * vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(CubeVertex);

	indexBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(uint32_t) * indices.size());
	indexBuffer_->SetName(L"DisintegrationRenderer_CubeIndexBuffer");
	void* indexMapped = nullptr;
	indexBuffer_->Map(0, nullptr, &indexMapped);
	std::memcpy(indexMapped, indices.data(), sizeof(uint32_t) * indices.size());

	indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
	indexCount_ = static_cast<UINT>(indices.size());
}

void DisintegrationRenderer::EnsureInstanceCapacity(size_t requiredCapacity)
{
	if (requiredCapacity <= instanceCapacity_) { return; }

	if (instanceBuffer_)
	{
		retiredInstanceBuffers_.push_back(instanceBuffer_);
	}
	if (srvIndex_ != UINT32_MAX)
	{
		retiredSrvIndices_.push_back(srvIndex_);
		srvIndex_ = UINT32_MAX;
	}
	instanceCapacity_ = std::max(requiredCapacity, instanceCapacity_ * 2 + 1);
	auto* device = K4E::DirectXCommon::GetInstance()->GetDevice();
	instanceBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(InstanceData) * instanceCapacity_);
	instanceBuffer_->SetName(L"DisintegrationRenderer_InstanceBuffer");
	instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

	srvIndex_ = K4E::SRVManager::GetInstance()->Allocate();
	K4E::SRVManager::GetInstance()->CreateSRVForStructureBuffer(srvIndex_, instanceBuffer_.Get(), static_cast<UINT>(instanceCapacity_), sizeof(InstanceData));
}

void DisintegrationRenderer::UpdateViewProjection()
{
	viewProjectionData_->viewProjection = K4E::CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
}

void DisintegrationRenderer::ReleaseSrv()
{
	if (srvIndex_ != UINT32_MAX)
	{
		K4E::SRVManager::GetInstance()->Free(srvIndex_);
		srvIndex_ = UINT32_MAX;
	}
	for (uint32_t retiredSrvIndex : retiredSrvIndices_)
	{
		K4E::SRVManager::GetInstance()->Free(retiredSrvIndex);
	}
	retiredSrvIndices_.clear();
	retiredInstanceBuffers_.clear();
	instanceData_ = nullptr;
	instanceBuffer_.Reset();
	instanceCapacity_ = 0;
}
