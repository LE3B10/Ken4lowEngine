#include "PostEffectManager.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "ShaderManager.h"
#include "LogString.h"
#include "Object3DCommon.h"
#include "Camera.h"
#include "ResourceManager.h"
#include "ParameterManager.h"

#include <cassert>


/// -------------------------------------------------------------
///				　	　シングルトンインスタンス
/// -------------------------------------------------------------
PostEffectManager* PostEffectManager::GetInstance()
{
	static PostEffectManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void PostEffectManager::Initialieze(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

	// パイプラインを生成
	CreatePipelineState("NormalEffect");

	// グレイスケールのパイプラインを生成
	CreatePipelineState("GrayScaleEffect");

	// ヴィネットのパイプラインを生成
	CreatePipelineState("VignetteEffect");
	InitializeVignette();

	// スムージングのパイプラインを生成
	CreatePipelineState("SmoothingEffect");
	InitializeSmoothing();

	// ガウシアンフィルタのパイプラインを生成
	CreatePipelineState("GaussianFilterEffect");
	InitializeGaussianFilter();

	// アウトラインのパイプラインを生成
	CreatePipelineState("LuminanceOutline");
	InitializeLuminanceOutline();

	// レンダーテクスチャの生成
	renderResource_ = CreateRenderTextureResource(WinApp::kClientWidth, WinApp::kClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTextureClearColor_);

	// 深度バッファの生成
	depthResource_ = CreateRenderTextureResource(WinApp::kClientWidth, WinApp::kClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTextureClearColor_);

	// RTVとSRVの確保
	AllocateRTVAndSRV();

	// ビューポート矩形とシザリング矩形の設定
	SetViewportAndScissorRect();

	//// ガウシアンフィルターのパラメータ
	//ParameterManager::GetInstance()->CreateGroup("VignettePower");
	//ParameterManager::GetInstance()->AddItem("VignettePower", "intensity", gaussianFilterSetting_->intensity);
	//ParameterManager::GetInstance()->AddItem("VignettePower", "threshold", gaussianFilterSetting_->threshold);
	//ParameterManager::GetInstance()->AddItem("VignettePower", "sigma", gaussianFilterSetting_->sigma);

	// アウトラインのパラメータ
	ParameterManager::GetInstance()->CreateGroup("LuminanceOutline");
	ParameterManager::GetInstance()->AddItem("LuminanceOutline", "edgeStrength", luminanceOutlineSetting_->edgeStrength);
	ParameterManager::GetInstance()->AddItem("LuminanceOutline", "threshold", luminanceOutlineSetting_->threshold);
}


/// -------------------------------------------------------------
///				　			描画開始処理
/// -------------------------------------------------------------
void PostEffectManager::BeginDraw()
{
	auto commandList = dxCommon_->GetCommandList();

	// 🔹 **バリア処理を適用**
	SetBarrier(D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// DSVの取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(0);

	// レンダーターゲットを設定
	commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle);

	// クリアカラー
	float clearColor[] = { kRenderTextureClearColor_.x, kRenderTextureClearColor_.y, kRenderTextureClearColor_.z, kRenderTextureClearColor_.w };

	// 画面のクリア
	commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);

	// 深度バッファのクリア
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	//// ガウシアンフィルタのパラメータ
	//gaussianFilterSetting_->intensity = ParameterManager::GetInstance()->GetValue<float>("VignettePower", "intensity");
	//gaussianFilterSetting_->threshold = ParameterManager::GetInstance()->GetValue<float>("VignettePower", "threshold");
	//gaussianFilterSetting_->sigma = ParameterManager::GetInstance()->GetValue<float>("VignettePower", "sigma");

	// アウトラインのパラメータ
	luminanceOutlineSetting_->edgeStrength = ParameterManager::GetInstance()->GetValue<float>("LuminanceOutline", "edgeStrength");
	luminanceOutlineSetting_->threshold = ParameterManager::GetInstance()->GetValue<float>("LuminanceOutline", "threshold");
}


/// -------------------------------------------------------------
///				　			描画終了処理
/// -------------------------------------------------------------
void PostEffectManager::EndDraw()
{
	auto commandList = dxCommon_->GetCommandList();

	// 🔹 **バリアを適用**
	SetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	// 🔹 GPU が完了するのを待つ (デバッグ用)
	dxCommon_->WaitCommand();
}


/// -------------------------------------------------------------
///				　	ポストエフェクトの描画適用処理
/// -------------------------------------------------------------
void PostEffectManager::RenderPostEffect()
{
	auto commandList = dxCommon_->GetCommandList();

	// 🔹 スワップチェインのバックバッファを取得
	uint32_t backBufferIndex = dxCommon_->GetSwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();
	ComPtr<ID3D12Resource> backBuffer = dxCommon_->GetBackBuffer(backBufferIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = dxCommon_->GetBackBufferRTV(backBufferIndex);

	// 🔹 ポストエフェクト専用のビューポートとシザー矩形を設定
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	// 🔹 ポストエフェクトの設定
	SetPostEffect("NormalEffect");
	//SetPostEffect("GrayScaleEffect");
	//SetPostEffect("VignetteEffect");
	//SetPostEffect("SmoothingEffect");
	//SetPostEffect("GaussianFilterEffect");
	//SetPostEffect("LuminanceOutline");

	// 🔹 SRV (シェーダーリソースビュー) をセット
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(rtvSrvIndex_));
	// commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_));

	// 🔹 スワップチェインのバッファを描画ターゲットにする
	commandList->OMSetRenderTargets(1, &backBufferRTV, false, nullptr);

	// 🔹 フルスクリーンクアッドを描画
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}


/// -------------------------------------------------------------
///				　			バリアの設定
/// -------------------------------------------------------------
void PostEffectManager::SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
	dxCommon_->TransitionResource(renderResource_.Get(), stateBefore, stateAfter);
}


/// -------------------------------------------------------------
///				　レンダーテクスチャリソースを生成
/// -------------------------------------------------------------
ComPtr<ID3D12Resource> PostEffectManager::CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
{
	// テクスチャの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2Dテクスチャ
	resourceDesc.Width = width;									  // テクスチャの幅
	resourceDesc.Height = height;								  // テクスチャの高さ
	resourceDesc.DepthOrArraySize = 1;							  // 配列サイズ
	resourceDesc.MipLevels = 1;									  // ミップマップレベル
	resourceDesc.Format = format;								  // フォーマット
	resourceDesc.SampleDesc.Count = 1;							  // サンプル数
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // レンダーターゲットとして使う

	// ヒープの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	// クリア値
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

	ComPtr<ID3D12Resource> resource;

	// リソースの生成
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties,					// ヒープの設定
		D3D12_HEAP_FLAG_NONE,				// ヒープの特殊な設定
		&resourceDesc,						// リソースの設定
		D3D12_RESOURCE_STATE_COMMON,		// リソースの初期状態、レンダーターゲットとして使う
		&clearValue,						// クリア値の設定
		IID_PPV_ARGS(&resource));			// 生成したリソースのポインタへのポインタを取得

	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create render texture resource.\n");
		assert(false);
	}

	return resource;
}


/// -------------------------------------------------------------
///				　	　ルートシグネチャの生成
/// -------------------------------------------------------------
void PostEffectManager::CreateRootSignature(const std::string& effectName)
{
	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;            // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;		   // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;						   // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;								   // レジスタ番号0 s0
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	   // ピクセルシェーダーで使用

	staticSamplers[1] = staticSamplers[0]; // 同じ設定を使う場合
	staticSamplers[1].ShaderRegister = 1; // レジスタ番号1 s1
	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // ポイントフィルタ

	descriptionRootSignature.pStaticSamplers = staticSamplers;			   // サンプラの設定
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers); // サンプラの数 = 2

	// DescriptorRangeの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRange[0].NumDescriptors = 1;	   // ディスクリプタ数
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 深度バッファの設定
	D3D12_DESCRIPTOR_RANGE descriptorRangeDepth[1] = {};
	descriptorRangeDepth[0].BaseShaderRegister = 1;
	descriptorRangeDepth[0].NumDescriptors = 1;
	descriptorRangeDepth[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeDepth[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートシグネチャの生成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// テクスチャの設定
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;      // ディスクリプタテーブル
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 	           // ピクセルシェーダーで使用
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;             // ディスクリプタテーブルの設定 t0
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // ディスクリプタテーブルの数

	// 定数バッファ (CBV)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // 定数バッファビュー
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[1].Descriptor.ShaderRegister = 0;					 // レジスタ番号 b0

	// 定数バッファ (CBV)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // 定数バッファビュー
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[2].Descriptor.ShaderRegister = 1;					 // レジスタ番号 b1

	// 深度バッファテクスチャ
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;      // ディスクリプタテーブル
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 	           // ピクセルシェーダーで使用
	rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangeDepth;             // ディスクリプタテーブルの設定 t1
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeDepth); // ディスクリプタテーブルの数

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
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignatures_[effectName]));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///				　		パイプラインを生成
/// -------------------------------------------------------------
void PostEffectManager::CreatePipelineState(const std::string& effectName)
{
	HRESULT hr{};

	// ルートシグネチャを生成
	CreateRootSignature(effectName);

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};
	inputLayoutDesc_.pInputElementDescs = nullptr;
	inputLayoutDesc_.NumElements = 0;

	// ブレンドステートの設定
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// ラスタライザーの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// シェーダーのコンパイル先
	std::wstring PixelShaderPath = L"Resources/Shaders/" + ConvertString(effectName) + L".PS.hlsl";

	// Shaderをコンパイル
	ComPtr <IDxcBlob> vertexShaderBlob = ShaderManager::CompileShader(L"Resources/Shaders/FullScreen.VS.hlsl", L"vs_6_0", dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
	assert(vertexShaderBlob != nullptr);

	// Pixelをコンパイル
	ComPtr <IDxcBlob> pixelShaderBlob = ShaderManager::CompileShader(PixelShaderPath, L"ps_6_0", dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
	assert(pixelShaderBlob != nullptr);

	// 深度ステンシルステート
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = false;

	// パイプラインステートディスクリプタの初期化
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignatures_[effectName].Get();								// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;													// InputLayout
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
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates_[effectName]));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///				　		ポストエフェクトを設定
/// -------------------------------------------------------------
void PostEffectManager::SetPostEffect(const std::string& effectName)
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// 🔹 ポストエフェクトのパイプラインを設定
	commandList->SetPipelineState(graphicsPipelineStates_[effectName].Get());

	// 🔹 ルートシグネチャを設定
	commandList->SetGraphicsRootSignature(rootSignatures_[effectName].Get());

	// ノーマルエフェクトかグレースケールエフェクトかで処理を分岐
	if (effectName == "NormalEffect" || effectName == "GrayScaleEffect")
	{
		return; // 何もしない
	}
	else if (effectName == "VignetteEffect")
	{
		commandList->SetGraphicsRootConstantBufferView(1, vignetteResource_->GetGPUVirtualAddress());
	}
	else if (effectName == "SmoothingEffect")
	{
		commandList->SetGraphicsRootConstantBufferView(1, smoothingResource_->GetGPUVirtualAddress());
	}
	else if (effectName == "GaussianFilterEffect")
	{
		commandList->SetGraphicsRootConstantBufferView(1, gaussianResource_->GetGPUVirtualAddress());
	}
	else if (effectName == "LuminanceOutline")
	{
		commandList->SetGraphicsRootConstantBufferView(1, luminanceOutlineResource_->GetGPUVirtualAddress());

		// SRVのバインド：t0 は render texture、t1 は depth texture
		//commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(rtvSrvIndex_));
		//commandList->SetGraphicsRootDescriptorTable(2, SRVManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_));
	}
	else
	{
		assert(false);
	}
}


/// -------------------------------------------------------------
///				　		RTVとSRVの確保
/// -------------------------------------------------------------
void PostEffectManager::AllocateRTVAndSRV()
{
	// RTVの確保
	uint32_t rtvIndex = RTVManager::GetInstance()->Allocate();
	RTVManager::GetInstance()->CreateRTVForTexture2D(rtvIndex, renderResource_.Get());
	rtvHandle_ = RTVManager::GetInstance()->GetCPUDescriptorHandle(rtvIndex);

	// SRVの確保
	rtvSrvIndex_ = SRVManager::GetInstance()->Allocate();
	SRVManager::GetInstance()->CreateSRVForTexture2D(rtvSrvIndex_, renderResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

	// DSVの確保
	/*uint32_t dsvSrvIndex_ = DSVManager::GetInstance()->Allocate();
	DSVManager::GetInstance()->CreateDSVForDepthBuffer(depthSrvIndex_, renderResource_.Get());
	depthSrvHandle_ = DSVManager::GetInstance()->GetCPUDescriptorHandle(dsvSrvIndex_);*/
}


/// -------------------------------------------------------------
///				ビューポート矩形とシザリング矩形の設定
/// -------------------------------------------------------------
void PostEffectManager::SetViewportAndScissorRect()
{
	// ビューポート矩形の設定
	viewport = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight), 0.0f, 1.0f);

	// シザリング矩形の設定
	scissorRect = { 0, 0, static_cast<LONG>(WinApp::kClientWidth), static_cast<LONG>(WinApp::kClientHeight) };
}


/// -------------------------------------------------------------
///				　		 ヴィネットの初期化
/// -------------------------------------------------------------
void PostEffectManager::InitializeVignette()
{
	// リソースの生成
	vignetteResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VignetteSetting));

	// データの設定
	vignetteResource_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteSetting_));

	// ヴィグネットの設定
	vignetteSetting_.power = 1.0f;
	vignetteSetting_.range = 0.5f;
}


/// -------------------------------------------------------------
///				　		 スムージングの初期化
/// -------------------------------------------------------------
void PostEffectManager::InitializeSmoothing()
{
	// リソースの生成
	smoothingResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(SmoothingSetting));

	// データの設定
	smoothingResource_->Map(0, nullptr, reinterpret_cast<void**>(&smoothingSetting_));

	// スムージングの設定
	smoothingSetting_->intensity = 0.5f;
	smoothingSetting_->threshold = 0.5f;
	smoothingSetting_->sigma = 0.0f;
}


/// -------------------------------------------------------------
///				　	 ガウシアンフィルタの初期化
/// -------------------------------------------------------------
void PostEffectManager::InitializeGaussianFilter()
{
	// リソースの生成
	gaussianResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(GaussianFilterSetting));

	// データの設定
	gaussianResource_->Map(0, nullptr, reinterpret_cast<void**>(&gaussianFilterSetting_));

	// ガウシアンフィルタの設定
	gaussianFilterSetting_->intensity = 1.0f;
	gaussianFilterSetting_->threshold = 0.5f;
	gaussianFilterSetting_->sigma = 1.0f;
}


/// -------------------------------------------------------------
///				　		アウトラインの初期化
/// -------------------------------------------------------------
void PostEffectManager::InitializeLuminanceOutline()
{
	luminanceOutlineResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LuminanceOutlineSetting));
	luminanceOutlineResource_->Map(0, nullptr, reinterpret_cast<void**>(&luminanceOutlineSetting_));

	// 解像度から texelSize を求める
	luminanceOutlineSetting_->texelSize = {
		1.0f / static_cast<float>(WinApp::kClientWidth),
		1.0f / static_cast<float>(WinApp::kClientHeight)
	};
	luminanceOutlineSetting_->edgeStrength = 5.0f;  // 初期値
	luminanceOutlineSetting_->threshold = 0.2f;     // 初期値
}
