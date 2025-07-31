#define NOMINMAX
#include "PostEffectManager.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include <Object3DCommon.h>
#include "SRVManager.h"

#include <cassert>
#include <imgui.h>

#include <NormalEffect.h>
#include <GrayScaleEffect.h>
#include <VignetteEffect.h>
#include <SmoothingEffect.h>
#include <GaussianFilterEffect.h>
#include <LuminanceOutlineEffect.h>
#include <RadialBlurEffect.h>
#include <DissolveEffect.h>
#include <RandomEffect.h>
#include <AbsorbEffect.h>
#include <DepthOutlineEffect.h>


auto Transition = [&](PostEffectManager::RenderTarget& rt, D3D12_RESOURCE_STATES newState)
	{
		auto dxCommon_ = DirectXCommon::GetInstance();
		if (rt.state == newState) return;                    // 二重バリア防止
		dxCommon_->ResourceTransition(rt.resource.Get(), rt.state, newState);
		rt.state = newState;                                 // ★状態を必ず同期
	};

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

	pipelineBuilder_ = std::make_unique<PostEffectPipelineBuilder>(); // パイプラインビルダーの生成
	pipelineBuilder_->Initialize(dxCommon); // パイプラインビルダーの初期化
	pipelineBuilder_->BuildCopyPipeline(); // コピー用パイプラインのビルド

	renderTargets_.resize(2); // レンダーターゲットの数を1に設定

	// RTVとSRVの確保
	AllocateRTV_DSV_SRV_UAV();

	// ビューポート矩形とシザリング矩形の設定
	SetViewportAndScissorRect();

	// エフェクトの初期化と生成
	std::unordered_map<std::string, EffectEntry> effectTable = {
		{ "NormalEffect",			{ [] { return std::make_unique<NormalEffect>(); },		     true,  0, "Visual" } },
		{ "GrayScaleEffect",	    { [] { return std::make_unique<GrayScaleEffect>(); },        false, 1, "Color"  } },
		{ "VignetteEffect",		    { [] { return std::make_unique<VignetteEffect>(); },         false, 2, "Color"  } },
		{ "SmoothingEffect",	    { [] { return std::make_unique<SmoothingEffect>(); },        false, 3, "Blur"   } },
		{ "GaussianFilterEffect",   { [] { return std::make_unique<GaussianFilterEffect>(); },   false, 4, "Blur"   } },
		{ "LuminanceOutlineEffect", { [] { return std::make_unique<LuminanceOutlineEffect>(); }, false, 5, "Visual" } },
		{ "RadialBlurEffect",		{ [] { return std::make_unique<RadialBlurEffect>(); },       false, 6, "Blur"   } },
		{ "DissolveEffect",			{ [] { return std::make_unique<DissolveEffect>(); },         false, 7, "Visual" } },
		{ "RandomEffect",			{ [] { return std::make_unique<RandomEffect>(); },			 false, 8, "Visual" } },
		{ "AbsorbEffect",			{ [] { return std::make_unique<AbsorbEffect>(); },           false, 9, "Visual" } },
		{"DepthOutLineEffect"     , { [] { return std::make_unique<DepthOutlineEffect>(Object3DCommon::GetInstance()->GetDefaultCamera()); }, false, 10, "Visual"} },
	};

	// ファクトリー関数を使ってエフェクトを生成
	for (const auto& [name, entry] : effectTable)
	{
		postEffects_[name] = entry.creator();
		postEffects_[name]->Initialize(dxCommon_, pipelineBuilder_.get());
		effectEnabled_[name] = entry.enabled;
		effectOrder_.emplace_back(name, entry.order);
		effectCategory_[name] = entry.category;
	}
}


/// -------------------------------------------------------------
///				　	ポストエフェクトの更新処理
/// -------------------------------------------------------------
void PostEffectManager::Update()
{
	for (auto& [name, effect] : postEffects_)
	{
		if (effectEnabled_[name])
		{
			effect->Update(); // 各エフェクトのパラメータ更新処理（仮想関数）
		}
	}
}


/// -------------------------------------------------------------
///				　			描画開始処理
/// -------------------------------------------------------------
void PostEffectManager::BeginDraw()
{
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

	// 🔷 必ず DEPTH_WRITE 状態に戻す → ClearDepthStencilView 用
	if (depthResource_ && depthState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		dxCommon_->ResourceTransition(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	// "A" を構造体経由に置換
	auto& rt = renderTargets_[0];
	Transition(rt, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// レンダーターゲットを設定
	commandList->OMSetRenderTargets(1, &rt.rtvHandle, false, &dsvHandle);

	// クリアカラー
	float clearColor[] = { rt.clearColor.x, rt.clearColor.y, rt.clearColor.z, rt.clearColor.w };

	// 画面のクリア
	commandList->ClearRenderTargetView(rt.rtvHandle, clearColor, 0, nullptr);

	// 深度バッファのクリア
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
}


/// -------------------------------------------------------------
///				　			描画終了処理
/// -------------------------------------------------------------
void PostEffectManager::EndDraw()
{
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

	// 🔷 Outline等で使うために、depthResource を PIXEL_SHADER_RESOURCE に遷移
	if (depthResource_ && depthState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		dxCommon_->ResourceTransition(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		depthState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	auto& rt = renderTargets_[0];
	Transition(rt, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// GPU が完了するのを待つ (デバッグ用)
	dxCommon_->GetCommandManager()->ExecuteAndWait();
}


/// -------------------------------------------------------------
///				　	ポストエフェクトの描画適用処理
/// -------------------------------------------------------------
void PostEffectManager::RenderPostEffect()
{
	// SRV ヒープ / VP / Scissor をセット
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
	SRVManager::GetInstance()->PreDraw();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	// RenderTarget が１枚しか無い → 旧来どおりバックバッファへ
	if (renderTargets_.size() < 2)
	{
		auto& rt = renderTargets_[0]; // A

		// バックバッファRTV
		uint32_t backBufferIndex = dxCommon_->GetSwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();
		ComPtr<ID3D12Resource> backBuffer = dxCommon_->GetBackBuffer(backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = dxCommon_->GetBackBufferRTV(backBufferIndex);

		// ★ ①-1 PRESENT → RENDER_TARGET へ遷移
		dxCommon_->ResourceTransition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		commandList->OMSetRenderTargets(1, &backBufferRTV, false, &dsvHandle);

		// ① PSO / ルートシグネチャ
		commandList->SetPipelineState(pipelineBuilder_->GetCopyPipelineState().Get());
		commandList->SetGraphicsRootSignature(pipelineBuilder_->GetCopyRootSignature().Get());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, rt.srvIndex);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0); // フルスクリーンクアッドを描画

		dxCommon_->ResourceTransition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT); // ★ ②-1 RENDER_TARGET → PRESENT へ遷移

		return; // これで終了
	}

	// 複数枚がある場合

	// 順序でソート
	std::sort(effectOrder_.begin(), effectOrder_.end(),
		[](const auto& a, const auto& b) { return a.second < b.second; });

	uint32_t src = 0; // ソースのインデックス
	uint32_t dst = 1; // デスティネーションのインデックス

	// 🔹 ポストエフェクトの描画
	for (const auto& [name, _] : effectOrder_)
	{
		if (!(effectEnabled_[name] || effectEnableFlags_[name])) continue;  // エフェクトが無効ならスキップ

		auto& inRT = renderTargets_[src]; // 入力レンダーテクスチャ
		auto& outRT = renderTargets_[dst]; // 出力レンダーテクスチャ

		if (name == "GrayScaleEffect" || name == "RandomEffect" || name == "DissolveEffect" || name == "VignetteEffect" || name == "GaussianFilterEffect" || name == "RadialBlurEffect" || name == "LuminanceOutlineEffect" || name == "SmoothingEffect")
		{
			Transition(inRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			Transition(outRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			postEffects_[name]->Apply(commandList, inRT.srvIndex, outRT.uavIndex, dsvSrvIndex_);

			Transition(outRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		else
		{
			// 書き込み
			Transition(inRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			Transition(outRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			commandList->OMSetRenderTargets(1, &outRT.rtvHandle, false, &dsvHandle);

			// エフェクト適用
			postEffects_[name]->Apply(commandList, inRT.srvIndex, outRT.uavIndex, dsvSrvIndex_);

			commandList->OMSetRenderTargets(0, nullptr, false, nullptr); // 出力レンダーテクスチャを解除
			Transition(outRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		// ping-pong するためにインデックスを入れ替え
		std::swap(src, dst);
	}

	// 最後の出力レンダーテクスチャをバックバッファに描画する
	auto& finalRT = renderTargets_[src]; // 最後の出力レンダーテクスチャ

	// バックバッファの取得
	uint32_t backBufferIndex = dxCommon_->GetSwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = dxCommon_->GetBackBufferRTV(backBufferIndex);

	commandList->OMSetRenderTargets(1, &backBufferRTV, false, &dsvHandle);

	// コピー用 PSO / ルートシグネチャ
	commandList->SetPipelineState(pipelineBuilder_->GetCopyPipelineState().Get());
	commandList->SetGraphicsRootSignature(pipelineBuilder_->GetCopyRootSignature().Get());
	SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, finalRT.srvIndex);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}


/// -------------------------------------------------------------
///				　	　		ImGui描画
/// -------------------------------------------------------------
void PostEffectManager::ImGuiRender()
{
	ImGui::Begin("Post Effect Settings");

	// ImGui:
	for (const auto& [name, category] : effectCategory_)
	{
		ImGui::Checkbox(name.c_str(), &effectEnabled_[name]);
		if (effectEnabled_[name])
		{
			postEffects_[name]->DrawImGui();
		}
	}

	ImGui::End();
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
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // レンダーターゲットとして使う

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
///				　深度バッファリソースを生成
/// -------------------------------------------------------------
ComPtr<ID3D12Resource> PostEffectManager::CreateDepthBufferResource(uint32_t width, uint32_t height)
{
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc.Count = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil = { 1.0f, 0 };

	D3D12_HEAP_PROPERTIES heapProp = {
		D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

	ComPtr<ID3D12Resource> depth;
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&depth));

	assert(SUCCEEDED(hr));
	return depth;
}


/// -------------------------------------------------------------
///				　		RTVとSRVの確保
/// -------------------------------------------------------------
void PostEffectManager::AllocateRTV_DSV_SRV_UAV()
{
	const uint32_t rtCount = static_cast<uint32_t>(renderTargets_.size()); // レンダーテクスチャの数を取得

	for (uint32_t i = 0; i < rtCount; ++i)
	{
		auto& rt = renderTargets_[i];

		// レンダーテクスチャリソースの生成
		rt.resource = CreateRenderTextureResource(WinApp::kClientWidth, WinApp::kClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTextureClearColor_);

		// RTVの生成
		uint32_t rtvIndex = RTVManager::GetInstance()->Allocate();
		RTVManager::GetInstance()->CreateRTVForTexture2D(rtvIndex, rt.resource.Get());
		rt.rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(rtvIndex);

		// SRVの生成
		rt.srvIndex = SRVManager::GetInstance()->Allocate();
		SRVManager::GetInstance()->CreateSRVForTexture2D(rt.srvIndex, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

		// UAVの生成
		rt.uavIndex = SRVManager::GetInstance()->Allocate();
		SRVManager::GetInstance()->CreateUAVForTexture2D(rt.uavIndex, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 0); // UAVはTexture2Dとして生成
	}

	// 深度バッファの生成
	depthResource_ = CreateDepthBufferResource(WinApp::kClientWidth, WinApp::kClientHeight);

	// SRVの確保（深度用）
	uint32_t dsvIndex = DSVManager::GetInstance()->Allocate();
	DSVManager::GetInstance()->CreateDSVForTexture2D(dsvIndex, depthResource_.Get());
	dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(dsvIndex);

	// SRVの確保（深度用）
	dsvSrvIndex_ = SRVManager::GetInstance()->Allocate();
	SRVManager::GetInstance()->CreateSRVForTexture2D(dsvSrvIndex_, depthResource_.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
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
