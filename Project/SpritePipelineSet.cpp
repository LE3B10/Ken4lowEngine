#include "SpritePipelineSet.h"
#include "PipelineFactory.h"
#include "PipelineStatePresets.h"
#include "ShaderCompiler.h"
#include "SpriteShaderManifest.h"
#include "DXCCompilerManager.h"
#include "SRVManager.h"
#include "ShaderManifestTypes.h"
#include <array>
#include <cassert>

namespace Ken4lowEngine
{
	namespace
	{
		std::array<D3D12_INPUT_ELEMENT_DESC, 2> MakeSpriteInputLayout()
		{
			return {
				D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};
		}
	}

	D3D12_ROOT_SIGNATURE_DESC MakeSpriteRootSignatureDesc(D3D12_DESCRIPTOR_RANGE* descriptorRange, D3D12_ROOT_PARAMETER* rootParameters, D3D12_STATIC_SAMPLER_DESC* staticSampler)
	{
		assert(descriptorRange != nullptr);
		assert(rootParameters != nullptr);
		assert(staticSampler != nullptr);

		descriptorRange[0] = {};
		descriptorRange[0].BaseShaderRegister = 0;
		descriptorRange[0].NumDescriptors = SRVManager::GetInstance()->GetkMaxSRVCount();
		descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// b0 : Material (PS)
		rootParameters[0] = {};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// b1 : ReloadProgress など (PS)
		rootParameters[1] = {};
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].Descriptor.ShaderRegister = 1;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// b0 : TransformationMatrix (VS)
		rootParameters[2] = {};
		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[2].Descriptor.ShaderRegister = 0;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		// t0 : SRV table (PS)
		rootParameters[3] = {};
		rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange;
		rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		*staticSampler = {};
		staticSampler->Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		staticSampler->AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler->AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		staticSampler->ShaderRegister = 0;
		staticSampler->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		staticSampler->MaxLOD = D3D12_FLOAT32_MAX;

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = 4;
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = staticSampler;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return desc;
	}

	GraphicsPipelineDesc MakeBaseSpriteDesc(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, const D3D12_INPUT_LAYOUT_DESC& inputLayout)
	{
		GraphicsPipelineDesc desc{};
		desc.blendState = PipelineStatePresets::MakeBlendAlpha();
		desc.rasterizerState = PipelineStatePresets::MakeRasterizerCullNone();
		desc.numRenderTargets = 1;
		desc.rtvFormats[0] = rtvFormat;
		desc.dsvFormat = dsvFormat;
		desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		desc.sampleCount = 1;
		desc.inputLayout = inputLayout;
		return desc;
	}

	void SpritePipelineSet::Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
	{
		assert(dxcManager != nullptr);

		auto inputElements = MakeSpriteInputLayout();

		// 入力レイアウトの記述を作成
		D3D12_INPUT_LAYOUT_DESC inputLayout{};
		inputLayout.pInputElementDescs = inputElements.data();
		inputLayout.NumElements = static_cast<UINT>(inputElements.size());

		// シェーダーのコンパイル
		const ShaderDescriptor& vsDesc = SpriteShaderManifest::Get(SpriteShaderId::SpriteVS);
		const ShaderDescriptor& psDesc = SpriteShaderManifest::Get(SpriteShaderId::SpritePS);

		// シェーダーディスクリプタの契約を検証
		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);
		assert(vsDesc.rootSignature == RootSignatureType::Sprite);
		assert(psDesc.rootSignature == RootSignatureType::Sprite);

		// シェーダーのコンパイル
		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(vsDesc, dxcManager);
		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(psDesc, dxcManager);

		// ルートシグネチャの作成
		D3D12_DESCRIPTOR_RANGE descriptorRange{};
		D3D12_ROOT_PARAMETER rootParameters[4]{};
		D3D12_STATIC_SAMPLER_DESC staticSampler{};

		// ルートシグネチャの記述を作成
		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = MakeSpriteRootSignatureDesc(&descriptorRange, rootParameters, &staticSampler);

		// Background
		{
			GraphicsPipelineDesc desc = MakeBaseSpriteDesc(rtvFormat, dsvFormat, inputLayout);
			desc.debugName = L"Sprite.Background";
			desc.shaders.vertexShader.blob = vertexShaderBlob;
			desc.shaders.pixelShader.blob = pixelShaderBlob;
			desc.depthStencilState = PipelineStatePresets::MakeDepthDisable();
			backgroundPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

			if (backgroundPipeline_.rootSignature) {
				backgroundPipeline_.rootSignature->SetName(L"Sprite.Background.RootSignature");
			}
			if (backgroundPipeline_.pipelineState) {
				backgroundPipeline_.pipelineState->SetName(L"Sprite.Background.PSO");
			}
		}

		// UI
		{
			GraphicsPipelineDesc desc = MakeBaseSpriteDesc(rtvFormat, dsvFormat, inputLayout);
			desc.debugName = L"Sprite.UI";
			desc.shaders.vertexShader.blob = vertexShaderBlob;
			desc.shaders.pixelShader.blob = pixelShaderBlob;
			desc.depthStencilState = PipelineStatePresets::MakeDepthDisable();

			// UI は従来コードでも DepthWrite=ZERO 相当の意図だったが、
			// そもそも DepthEnable=false なので今は明示差分を持たなくてよい。
			uiPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

			if (uiPipeline_.rootSignature) {
				uiPipeline_.rootSignature->SetName(L"Sprite.UI.RootSignature");
			}
			if (uiPipeline_.pipelineState) {
				uiPipeline_.pipelineState->SetName(L"Sprite.UI.PSO");
			}
		}
	}
	void SpritePipelineSet::Finalize()
	{
		backgroundPipeline_.Reset();
		uiPipeline_.Reset();
	}
}