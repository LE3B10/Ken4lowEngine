#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <array>

namespace Ken4lowEngine
{
	/// ---------- シェーダーバイナリの構造体 ---------- ///
	struct ShaderBinary
	{
		Microsoft::WRL::ComPtr<IDxcBlob> blob; // シェーダーバイナリデータ
	};

	/// ---------- シェーダー ---------- ///
	struct ShaderProgram
	{
		ShaderBinary vertexShader;	 // 頂点シェーダーバイナリ
		ShaderBinary pixelShader;    // ピクセルシェーダーバイナリ
		ShaderBinary geometryShader; // ジオメトリシェーダバイナリ
		ShaderBinary hullShader;	 // ハルシェーダバイナリ
		ShaderBinary domainShader;	 // ドメインシェーダバイナリ
		ShaderBinary computeShader;  // コンピュートシェーダバイナリ
	};

	/// ---------- パイプラインバンドル ---------- ///
	struct PipelineBundle
	{
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature; // ルートシグネチャ
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState; // パイプラインステート

		void Reset()
		{
			rootSignature.Reset();
			pipelineState.Reset();
		}
	};

	/// ---------- グラフィックスパイプライン ---------- ///
	struct GraphicsPipelineDesc
	{
		std::wstring debugName;

		ShaderProgram shaders{};

		D3D12_INPUT_LAYOUT_DESC inputLayout{};
		D3D12_BLEND_DESC blendState{};
		D3D12_RASTERIZER_DESC rasterizerState{};
		D3D12_DEPTH_STENCIL_DESC depthStencilState{};

		std::array<DXGI_FORMAT, 8> rtvFormats{};
		UINT numRenderTargets = 1;
		DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		UINT sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		UINT sampleCount = 1;
	};
}