#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "DebugCamera.h"
#include <LogString.h>
#include <BlendStateFactory.h>
#include <ShaderCompiler.h>


/// -------------------------------------------------------------
///				　	シングルトンインスタンス
/// -------------------------------------------------------------
Object3DCommon* Object3DCommon::GetInstance()
{
	static Object3DCommon instance;
	return &instance;
}


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void Object3DCommon::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	isDebugCamera_ = false;

	CreatePSO();

	// ライトマネージャの生成と初期化
	LightManager::GetInstance()->Initialize(dxCommon_);
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void Object3DCommon::Update()
{
	if (isDebugCamera_)
	{
#ifdef _DEBUG
		debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMatrix();
		defaultCamera_->SetViewProjectionMatrix(debugViewProjectionMatrix_);
		activeCameraPosition_ = DebugCamera::GetInstance()->GetTranslate();
#endif // _DEBUG
	}
	else
	{
		viewProjectionMatrix_ = Matrix4x4::Multiply(defaultCamera_->GetViewMatrix(), defaultCamera_->GetProjectionMatrix());
		defaultCamera_->SetViewProjectionMatrix(viewProjectionMatrix_);
		activeCameraPosition_ = defaultCamera_->GetTranslate(); // 通常カメラの位置
	}
}

void Object3DCommon::DrawImGui()
{
}


/// -------------------------------------------------------------
///				　		共通描画処理設定
/// -------------------------------------------------------------
void Object3DCommon::SetRenderSetting()
{
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ライトの設定
	LightManager::GetInstance()->BindPunctualLights(5, 6);
}


void Object3DCommon::CreateRootSignature()
{
	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	//staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;            // バイリニアフィルタ
	//staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	//staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	//staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	//staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;		   // 比較しない
	//staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;						   // ありったけのMipmapを使う

	// 変更後（ドット絵用）
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // ←にじみ防止
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].MaxLOD = 0.0f; // ←ベースレベル固定でミップを実質OFF

	staticSamplers[0].ShaderRegister = 0;								   // レジスタ番号0
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	   // ピクセルシェーダーで使用
	descriptionRootSignature.pStaticSamplers = staticSamplers;			   // サンプラの設定
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers); // サンプラの数

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};

	// SRV0: テクスチャ用のディスクリプタテーブル
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// キューブマップ用SRV範囲（t1）
	D3D12_DESCRIPTOR_RANGE cubeMapRange{};
	cubeMapRange.BaseShaderRegister = 1; // t1
	cubeMapRange.NumDescriptors = 1;
	cubeMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	cubeMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ライト配列用SRV範囲（t2）
	D3D12_DESCRIPTOR_RANGE lightArrayRange{};
	lightArrayRange.BaseShaderRegister = 2; // t2
	lightArrayRange.NumDescriptors = 1;     // 配列1個分
	lightArrayRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV
	lightArrayRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 自動設定

	// ルートシグネチャの生成
	D3D12_ROOT_PARAMETER rootParameters[7] = {};

	// マテリアル用のルートシグパラメータの設定 （b0）
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // 定数バッファビュー
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[0].Descriptor.ShaderRegister = 0;                    // レジスタ番号0

	// TransformationMatrix用のルートシグネチャの設定 （b0）
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // 定数バッファビュー
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // バーテックスシェーダーで使用
	rootParameters[1].Descriptor.ShaderRegister = 0;					 // レジスタ番号0

	// テクスチャのディスクリプタテーブル （t0）
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;      // ディスクリプタテーブル
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 	           // ピクセルシェーダーで使用
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;             // ディスクリプタテーブルの設定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // ディスクリプタテーブルの数

	// カメラ用のルートシグネチャの設定 （b1）
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[3].Descriptor.ShaderRegister = 1; 					// レジスタ番号1

	// キューブマップのルートシグネチャの設定 （t1）
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].DescriptorTable.pDescriptorRanges = &cubeMapRange;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;

	// ライト数 CBV（b2）
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // 定数バッファビュー
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[5].Descriptor.ShaderRegister = 2;                    // レジスタ番号2

	// ライト配列 SRV（t2）
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; 	// ディスクリプタテーブル
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 	        // ピクセルシェーダーで使用
	rootParameters[6].DescriptorTable.pDescriptorRanges = &lightArrayRange;         //　ディスクリプタテーブルの設定
	rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;                      //　ディスクリプタテーブルの数

	// ルートシグネチャの設定
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// シリアライズしてバイナリに変換
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}

	// バイナリをもとにルートシグネチャ生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3DCommon::CreatePSO()
{
	HRESULT hr{};

	// ルートシグネチャを生成
	CreateRootSignature();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	inputLayoutDesc_.pInputElementDescs = inputElementDescs;
	inputLayoutDesc_.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	const D3D12_RENDER_TARGET_BLEND_DESC& blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴンを塗りつぶす
	//rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // 裏面カリングを有効にする
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // 裏面カリングを無効にする
	rasterizerDesc.FrontCounterClockwise = FALSE;	 // 時計回りの面を表面とする（カリング方向の設定）

	// Shaderをコンパイル
	ComPtr <IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(L"Resources/Shaders/Object3D/Object3D.VS.hlsl", L"vs_6_0", dxCommon_->GetDXCCompilerManager());
	assert(vertexShaderBlob != nullptr);

	// Pixelをコンパイル
	ComPtr <IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(L"Resources/Shaders/Object3D/Object3D.PS.hlsl", L"ps_6_0", dxCommon_->GetDXCCompilerManager());
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
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();													// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;														// InputLayout
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
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}
