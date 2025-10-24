#include "SpriteManager.h"
#include "LogString.h"
#include "ShaderCompiler.h"
#include <BlendStateFactory.h>


/// -------------------------------------------------------------
///				　	シングルトンインスタンス
/// -------------------------------------------------------------
SpriteManager* SpriteManager::GetInstance()
{
	static SpriteManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void SpriteManager::Initialize(DirectXCommon* dxCommon)
{
	// 引数をメンバ変数に設定
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	CreatePSO();
}

/// -------------------------------------------------------------
///				　		背景用の共通描画設定
/// -------------------------------------------------------------
void SpriteManager::SetRenderSetting_Background()
{
	// コマンドリストの取得
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

	// ルートシグネチャとパイプラインステートの設定
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_Background_.Get());

	// プリミティブトポロジーの設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}


/// -------------------------------------------------------------
///				　		UI用の共通描画設定
/// -------------------------------------------------------------
void SpriteManager::SetRenderSetting_UI()
{
	// コマンドリストの取得
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

	// ルートシグネチャとパイプラインステートの設定
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_UI_.Get());

	// プリミティブトポロジーの設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}


/// -------------------------------------------------------------
///				　		ルートシグネチャの生成
/// -------------------------------------------------------------
void SpriteManager::CreateRootSignature()
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
	descriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRange[0].NumDescriptors = 1;	   // ディスクリプタ数
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 自動設定

	// ルートシグネチャの生成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// マテリアル用のルートシグパラメータの設定
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // 定数バッファビュー
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[0].Descriptor.ShaderRegister = 0;                    // レジスタ番号0

	// b1 : ReloadProgress
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[1].Descriptor.ShaderRegister = 1;					// レジスタ番号1
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用

	// TransformationMatrix用のルートシグネチャの設定
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // 定数バッファビュー
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // バーテックスシェーダーで使用
	rootParameters[2].Descriptor.ShaderRegister = 0;					 // レジスタ番号0

	// テクスチャのディスクリプタテーブル
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;      // ディスクリプタテーブル
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 	           // ピクセルシェーダーで使用
	rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange;             // ディスクリプタテーブルの設定
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // ディスクリプタテーブルの数

	// ルートシグネチャの設定
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// シリアライズしてバイナリに変換
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr))
	{
		// エラー内容をログに出力
		Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}

	// バイナリをもとにルートシグネチャ生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///				　		パイプラインの生成
/// -------------------------------------------------------------
void SpriteManager::CreatePSO()
{
	HRESULT hr{};

	// ルートシグネチャを生成
	CreateRootSignature();

	// 入力レイアウトの設定
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };


	// 入力レイアウト
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	const D3D12_RENDER_TARGET_BLEND_DESC& blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴンを塗りつぶす
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // 裏面カリングを無効にする
	rasterizerDesc.FrontCounterClockwise = FALSE;	 // 時計回りの面を表面とする（カリング方向の設定）

	// Vertexをコンパイル
	ComPtr <IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(L"Resources/Shaders/Sprite/Sprite.VS.hlsl", L"vs_6_0", dxCommon_->GetDXCCompilerManager());
	assert(vertexShaderBlob != nullptr);

	// Pixelをコンパイル
	ComPtr <IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(L"Resources/Shaders/Sprite/Sprite.PS.hlsl", L"ps_6_0", dxCommon_->GetDXCCompilerManager());
	assert(pixelShaderBlob != nullptr);

	// --- 背景用（Zバッファ書き込みあり） ---
	{
		// デプスステンシルステートの設定
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;  // 書き込みあり
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		// グラフィックスパイプラインの設定
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature_.Get();											   // ルートシグネチャ
		desc.InputLayout = inputLayoutDesc;													   // 入力レイアウト
		desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // バーテックスシェーダー
		desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };   // ピクセルシェーダー
		desc.BlendState.RenderTarget[0] = blendDesc;										   // ブレンドステート
		desc.RasterizerState = rasterizerDesc;												   // ラスタライザーステート
		desc.NumRenderTargets = 1;															   // レンダーターゲットの数
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;								   // レンダーターゲットのフォーマット
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;				   // プリミティブトポロジー
		desc.SampleDesc.Count = 1;															   // サンプル数
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;										   // サンプルマスク
		desc.DepthStencilState = depthStencilDesc;											   // デプスステンシルステート
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;										   // デプスステンシルビューのフォーマット

		// パイプラインステートの生成
		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&graphicsPipelineState_Background_));
		assert(SUCCEEDED(hr));
	}

	// --- UI用（Zバッファ書き込みなし） ---
	{
		// デプスステンシルステートの設定
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 書き込みなし
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 比較はあり

		// グラフィックスパイプラインの設定
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature_.Get();											   // ルートシグネチャ
		desc.InputLayout = inputLayoutDesc;													   // 入力レイアウト
		desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // バーテックスシェーダー
		desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };   // ピクセルシェーダー
		desc.BlendState.RenderTarget[0] = blendDesc;										   // ブレンドステート
		desc.RasterizerState = rasterizerDesc;												   // ラスタライザーステート
		desc.NumRenderTargets = 1;															   // レンダーターゲットの数
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;								   // レンダーターゲットのフォーマット
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;				   // プリミティブトポロジー
		desc.SampleDesc.Count = 1;															   // サンプル数
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;										   // サンプルマスク
		desc.DepthStencilState = depthStencilDesc;											   // デプスステンシルステート
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;										   // デプスステンシルビューのフォーマット

		// パイプラインステートの生成
		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&graphicsPipelineState_UI_));
		assert(SUCCEEDED(hr));
	}
}
