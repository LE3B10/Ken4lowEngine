#include "AnimationModelManager.h"
#include <LogString.h>
#include <DirectXCommon.h>
#include <ResourceManager.h>
#include "Camera.h"
#include "Wireframe.h"
#include "ShaderManager.h"
#include "Material.h"


/// -------------------------------------------------------------
///				　シングルトンインスタンス
/// -------------------------------------------------------------
AnimationModelManager* AnimationModelManager::GetInstance()
{
	static AnimationModelManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void AnimationModelManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

	// パイプラインを生成
	CreatePSO();
}


/// -------------------------------------------------------------
///				　	ルートシグネチャの生成
/// -------------------------------------------------------------
void AnimationModelManager::CreateRootSignature()
{
	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;            // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;		   // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;						   // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;								   // レジスタ番号0
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	   // ピクセルシェーダーで使用
	descriptionRootSignature.pStaticSamplers = staticSamplers;			   // サンプラの設定
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers); // サンプラの数

	// DescriptorRangeの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 追加する SRV の DescriptorRange 設定
	D3D12_DESCRIPTOR_RANGE srvDescriptorRange{};
	srvDescriptorRange.BaseShaderRegister = 1; // register(t0) に対応
	srvDescriptorRange.NumDescriptors = 1;
	srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートシグネチャの生成
	D3D12_ROOT_PARAMETER rootParameters[8] = {};

	// マテリアル用のルートシグパラメータの設定
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // 定数バッファビュー
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[0].Descriptor.ShaderRegister = 0;                    // レジスタ番号0

	// TransformationMatrix用のルートシグネチャの設定
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // 定数バッファビュー
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // バーテックスシェーダーで使用
	rootParameters[1].Descriptor.ShaderRegister = 0;					 // レジスタ番号0

	// テクスチャのディスクリプタテーブル
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;      // ディスクリプタテーブル
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 	           // ピクセルシェーダーで使用
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;             // ディスクリプタテーブルの設定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // ディスクリプタテーブルの数

	// カメラ用のルートシグネチャの設定
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[3].Descriptor.ShaderRegister = 1; 					// レジスタ番号1

	// 平行光源用のルートシグネチャの設定
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[4].Descriptor.ShaderRegister = 2; 					// レジスタ番号2

	// ポイントライト用のルートシグネチャの設定
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[5].Descriptor.ShaderRegister = 3; 					// レジスタ番号3

	// スポットライトのルートシグネチャの設定
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[6].Descriptor.ShaderRegister = 4; 					// レジスタ番号4

	// SRV の設定（バーテックスシェーダーで使用）
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VS で使う
	rootParameters[7].DescriptorTable.pDescriptorRanges = &srvDescriptorRange;
	rootParameters[7].DescriptorTable.NumDescriptorRanges = 1;

	// ルートシグネチャの設定
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	ComPtr <ID3DBlob> signatureBlob = nullptr;
	ComPtr <ID3DBlob> errorBlob = nullptr;

	// シリアライズしてバイナリに変換
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリをもとにルートシグネチャ生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///				　パイプラインを生成する
/// -------------------------------------------------------------
void AnimationModelManager::CreatePSO()
{
	HRESULT hr{};

	// ルートシグネチャを生成する
	CreateRootSignature();

	// グラフィックスパイプラインステートの設定
	std::array<D3D12_INPUT_ELEMENT_DESC, 5> inputElementDescs{};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[3] = { "WEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[4] = { "INDEX",    0, DXGI_FORMAT_R32G32B32A32_SINT,	1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs.data();
	inputLayoutDesc.NumElements = static_cast<UINT>(inputElementDescs.size());

	// BlendStateの設定
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
	blendDesc.BlendEnable = false;
	// すべての色要素を書き込む
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// 各ブレンドモードの設定を行う
	switch (cuurenttype)
	{
		// ブレンドモードなし
	case BlendMode::kBlendModeNone:

		blendDesc.BlendEnable = false;
		break;

		// 通常αブレンドモード
	case BlendMode::kBlendModeNormal:

		// ノーマル
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		break;

		// 加算ブレンドモード
	case BlendMode::kBlendModeAdd:

		// 加算
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_ONE;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		  // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	  // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	  // アルファのデスティネーションは無視
		break;

		// 減算ブレンドモード
	case BlendMode::kBlendModeSubtract:

		// 減算
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		blendDesc.DestBlend = D3D12_BLEND_ONE;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		 // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	 // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	 // アルファのデスティネーションは無
		break;

		// 乗算ブレンドモード
	case BlendMode::kBlendModeMultiply:

		// 乗算
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_ZERO;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_SRC_COLOR;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		 // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	 // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	 // アルファのデスティネーションは無視
		break;

		// スクリーンブレンドモード
	case BlendMode::kBlendModeScreen:

		// スクリーン
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_ONE;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		 // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	 // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	 // アルファのデスティネーションは無視
		break;

		// 無効なブレンドモード
	default:
		// 無効なモードの処理
		assert(false && "Invalid Blend Mode");
		break;
	}

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴンを塗りつぶす
	//rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // 裏面カリングを有効にする
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // 裏面カリングを無効にする
	rasterizerDesc.FrontCounterClockwise = FALSE;	 // 時計回りの面を表面とする（カリング方向の設定）

	//Shaderをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> vertexShaderBlob = ShaderManager::CompileShader(L"Resources/Shaders/SkinningObject3d.VS.hlsl", L"vs_6_0", dxCommon_->GetDXCCompilerManager());
	assert(vertexShaderBlob != nullptr);

	//Pixelをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> pixelShaderBlob = ShaderManager::CompileShader(L"Resources/Shaders/SkinningObject3d.PS.hlsl", L"ps_6_0", dxCommon_->GetDXCCompilerManager());
	assert(pixelShaderBlob != nullptr);

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// パイプラインステートディスクリプタの初期化
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();												// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;													// InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };	// VertexDhader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };	// PixelShader
	graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;											// BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;													// RasterizeerState

	//レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//利用するトポロジー（形態）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;					// プリミティブトポロジーの設定

	// サンプルマスクとサンプル記述子の設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilステートの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// パイプラインステートオブジェクトの生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}
