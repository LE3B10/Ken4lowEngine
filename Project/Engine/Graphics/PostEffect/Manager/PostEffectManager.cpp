#define NOMINMAX
#include "PostEffectManager.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include <CameraManager.h>
#include <Camera.h>
#include <DebugCamera.h>
#include "SRVManager.h"
#include <UAVManager.h>
#include <DSVManager.h>
#include <RTVManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

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
#include <PixelateEffect.h>
#include "Effects/PlayerHealthPostEffect/PlayerHealthPostEffect.h"

namespace Ken4lowEngine
{

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
	void PostEffectManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		pipelineBuilder_ = std::make_unique<PostEffectPipelineBuilder>(); // パイプラインビルダーの生成
		pipelineBuilder_->Initialize(dxCommon); // パイプラインビルダーの初期化
		pipelineBuilder_->BuildCopyPipeline(); // コピー用パイプラインのビルド

		renderTargets_.resize(2); // ポストエフェクトのping-pongと最終GameRenderTargetに使う
		// RT0はScene/GameViewport、RT1はPostEffect出力として名前を固定し、DebugLayerで対象を特定する。
		renderTargets_[0].debugName = L"SceneRenderTarget_GameViewportRenderTarget";
		renderTargets_[1].debugName = L"PostEffectRenderTarget";

		// 初期GameViewportRenderTargetは既存UI/Sprite互換の基準解像度から開始する。
		renderTargetWidth_ = kDefaultGameRenderTargetWidth_;
		renderTargetHeight_ = kDefaultGameRenderTargetHeight_;

		// RTVとSRVの確保
		AllocateRTV_DSV_SRV_UAV();

		// ビューポート矩形とシザリング矩形の設定
		SetViewportAndScissorRect(renderTargetWidth_, renderTargetHeight_);

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
			{ "DepthOutLineEffect",		{ [] { return std::make_unique<DepthOutlineEffect>(CameraManager::GetInstance()->GetMainCamera()); }, false, 10, "Visual"} },
			{ "PixelateEffect",			{ [] { return std::make_unique<PixelateEffect>(); },		 false, 11, "Visual" } },
			{ "PlayerHealthPostEffect",	{ [] { return std::make_unique<PlayerHealthPostEffect>(); }, false, 12, "Color" } },
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

	void PostEffectManager::Finalize()
	{
		if (!dxCommon_) { return; }

		// まずGPU待ち（これ大事）
		dxCommon_->GetCommandManager()->ExecuteAndWait();

		for (auto& [name, effect] : postEffects_) {
			if (effect) { effect->Finalize(); }
		}

		pipelineBuilder_->Finalize();

		// エフェクト破棄
		postEffects_.clear();
		effectEnabled_.clear();
		effectEnableFlags_.clear();
		effectOrder_.clear();
		effectCategory_.clear();
		pipelineBuilder_.reset();

		// レンダーターゲット破棄
		for (auto& rt : renderTargets_) {
			rt.resource.Reset();
			rt.rtvHandle = {};
			rt.currentState_ = RenderTarget::kInitialState;
			// srvIndex/uavIndex を Free できる設計ならここでFree（後述）
		}
		renderTargets_.clear();

		// 深度破棄
		depthResource_.Reset();
		dsvHandle = {};
		depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		// dsvSrvIndex_ を Free できる設計ならここでFree（後述）

		signatureBlob_.Reset();
		errorBlob_.Reset();

		camera_ = nullptr;
		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///				　		ResourceState遷移
	/// -------------------------------------------------------------
	void PostEffectManager::TransitionTo(RenderTarget& renderTarget, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES nextState)
	{
		if (!commandList || !renderTarget.resource || renderTarget.currentState_ == nextState)
		{
			return;
		}

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = renderTarget.resource.Get();
		barrier.Transition.StateBefore = renderTarget.currentState_;
		barrier.Transition.StateAfter = nextState;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// GameRenderTarget自身の状態を元にBarrierを張り、BackBufferの状態遷移と混同しないようにする。
		commandList->ResourceBarrier(1, &barrier);
		renderTarget.currentState_ = nextState;
	}

	void PostEffectManager::CopyRenderTarget(RenderTarget& srcRT, RenderTarget& dstRT, ID3D12GraphicsCommandList* commandList)
	{
		assert(srcRT.resource.Get() != dstRT.resource.Get());

		// 最終表示先のSRVを固定するため、既存のフルスクリーンコピーPSOでRT間コピーを行う。
		SRVManager::GetInstance()->PreDraw();
		TransitionTo(srcRT, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		TransitionTo(dstRT, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->OMSetRenderTargets(1, &dstRT.rtvHandle, false, nullptr);
		commandList->SetPipelineState(pipelineBuilder_->GetCopyPipelineState().Get());
		commandList->SetGraphicsRootSignature(pipelineBuilder_->GetCopyRootSignature().Get());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, srcRT.srvIndex);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}


	/// -------------------------------------------------------------
	///				　	ポストエフェクトの更新処理
	/// -------------------------------------------------------------
	void PostEffectManager::Update()
	{
		// 各エフェクトの更新処理
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
		auto& rt = renderTargets_[0];

		// SceneRenderTarget_GameViewportRenderTargetを描画先にする直前でSRVからRTVへ戻す。
		TransitionTo(rt, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// DEPTH_WRITE 状態に戻す → ClearDepthStencilView 用
		if (depthResource_ && depthState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			dxCommon_->ResourceTransition(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

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
		// Outline等で使うために、depthResource を PIXEL_SHADER_RESOURCE に遷移
		if (depthResource_ && depthState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		{
			dxCommon_->ResourceTransition(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			depthState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		auto& rt = renderTargets_[0];
		// Main ViewportのImGui::Imageで読めるよう描画後にSRVへ戻す。
		TransitionTo(rt, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// GPU が完了するのを待つ (デバッグ用)
		dxCommon_->GetCommandManager()->ExecuteAndWait();
	}

	/// -------------------------------------------------------------
	///				　			リサイズ処理
	/// -------------------------------------------------------------
	void PostEffectManager::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			// Main Viewportが折りたたまれている間は0サイズRenderTargetを作らない。
			return;
		}

		if (renderTargetWidth_ == width && renderTargetHeight_ == height && depthResource_)
		{
			return;
		}

		// GameRenderTargetの現在サイズをMain Viewportの実ピクセルサイズとして記録する。
		renderTargetWidth_ = width;
		renderTargetHeight_ = height;

		SetViewportAndScissorRect(width, height);
		UpdateCameraAspectRatio(width, height);

		// RTを作り直して、同じdescriptor indexに上書き
		for (uint32_t i = 0; i < static_cast<uint32_t>(renderTargets_.size()); ++i)
		{
			auto& rt = renderTargets_[i];
			rt.resource.Reset();
			rt.resource = CreateRenderTextureResource(width, height,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTextureClearColor_);
			// Resize後もRenderTarget名を張り直し、Unnamed Resourceの再発を防ぐ。
			rt.resource->SetName(rt.debugName);

			RTVManager::GetInstance()->CreateRTVForTexture2D(rt.rtvIndex, rt.resource.Get());
			rt.rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(rt.rtvIndex);

			SRVManager::GetInstance()->CreateSRVForTexture2D(rt.srvIndex, rt.resource.Get(),
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

			UAVManager::GetInstance()->CreateUAVForTexture2D(rt.uavIndex, rt.resource.Get(),
				DXGI_FORMAT_R8G8B8A8_UNORM, 0);

			UAVManager::GetInstance()->CreateSRVForTexture2DOnThisHeap(rt.srvIndexOnUavHeap, rt.resource.Get(),
				DXGI_FORMAT_R8G8B8A8_UNORM, 1);

			rt.currentState_ = RenderTarget::kInitialState;
		}

		// depth作り直し
		depthResource_.Reset();
		depthResource_ = CreateDepthBufferResource(width, height);
		// PostEffect用深度もResize後に名前を付け直してDebugLayerで追跡可能にする。
		depthResource_->SetName(L"PostEffectManager DepthBuffer");
		DSVManager::GetInstance()->CreateDSVForTexture2D(depthDsvIndex_, depthResource_.Get());
		dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(depthDsvIndex_);

		SRVManager::GetInstance()->CreateSRVForTexture2D(dsvSrvIndex_, depthResource_.Get(),
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);

		depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}


	/// -------------------------------------------------------------
	///				　	Camera Aspect比更新
	/// -------------------------------------------------------------
	void PostEffectManager::UpdateCameraAspectRatio(uint32_t width, uint32_t height)
	{
		if (height == 0)
		{
			return;
		}

		const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		auto* cameraManager = CameraManager::GetInstance();
		if (Camera* mainCamera = cameraManager->GetMainCamera())
		{
			// Main CameraのProjectionをGameViewportRenderTargetのAspect比へ追従させる。
			mainCamera->SetAspectRatio(aspectRatio);
		}
		if (DebugCamera* debugCamera = cameraManager->GetDebugCamera())
		{
			// Debug Camera使用時も同じ描画AspectでFrustumを更新する。
			debugCamera->SetAspectRatio(aspectRatio);
		}
	}


	/// -------------------------------------------------------------
	///					　	ポストエフェクトの描画適用処理
	/// -------------------------------------------------------------
	void PostEffectManager::RenderPostEffect()
	{
		// SRV ヒープ / VP / Scissor をセット
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);

		// レンダーテクスチャが1枚しかない場合
		if (renderTargets_.size() < 2)
		{
			auto& rt = renderTargets_[0];

			// GameRenderTargetはBackBufferへコピーせずMain ViewportのImGui::Imageから直接参照する
			TransitionTo(rt, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			return;
		}

		// 複数枚がある場合

		// 順序でソート
		std::sort(effectOrder_.begin(), effectOrder_.end(),
			[](const auto& a, const auto& b) { return a.second < b.second; });

		uint32_t src = 0; // ソースのインデックス
		uint32_t dst = 1; // デスティネーションのインデックス
		bool appliedAnyEffect = false; // 最終alpha補正コピーが必要か判定するため適用有無を保持する

		// ポストエフェクトの描画
		for (const auto& [name, _] : effectOrder_)
		{
			if (!(effectEnabled_[name] || effectEnableFlags_[name])) continue;  // エフェクトが無効ならスキップ

			appliedAnyEffect = true;
			auto& inRT = renderTargets_[src]; // 入力レンダーテクスチャ
			auto& outRT = renderTargets_[dst]; // 出力レンダーテクスチャ

			// 入力SRVと出力RTV/UAVが同一Resourceにならないことを保証して自己参照描画を防ぐ。
			assert(inRT.resource.Get() != outRT.resource.Get());

			if (name == "GrayScaleEffect" || name == "RandomEffect" || name == "DissolveEffect" || name == "VignetteEffect" || name == "GaussianFilterEffect" || name == "RadialBlurEffect" || name == "LuminanceOutlineEffect" || name == "SmoothingEffect" || name == "PixelateEffect" || name == "PlayerHealthPostEffect")
			{
				TransitionTo(inRT, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				TransitionTo(outRT, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				// UAV ヒープをセット
				UAVManager::GetInstance()->PreDispatch();

				postEffects_[name]->Apply(commandList, inRT.srvIndexOnUavHeap, outRT.uavIndex, dsvSrvIndex_);

				TransitionTo(outRT, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			else
			{
				// SRVヒープをバインド（PSで使うため）
				SRVManager::GetInstance()->PreDraw();

				// 書き込み
				TransitionTo(inRT, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				TransitionTo(outRT, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
				commandList->OMSetRenderTargets(1, &outRT.rtvHandle, false, &dsvHandle);

				// エフェクト適用
				postEffects_[name]->Apply(commandList, inRT.srvIndex, outRT.uavIndex, dsvSrvIndex_);

				commandList->OMSetRenderTargets(0, nullptr, false, nullptr); // 出力レンダーテクスチャを解除
				TransitionTo(outRT, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}

			// ping-pong するためにインデックスを入れ替え
			std::swap(src, dst);
		}

		// 最後の出力を固定のGameRenderTarget(renderTargets_[0])へ戻してImGuiへ渡すSRVを安定させる
		auto& finalRT = renderTargets_[src];
		auto& gameRT = renderTargets_[0];

		if (src != 0)
		{
			CopyRenderTarget(finalRT, gameRT, commandList);
		}
		else if (appliedAnyEffect && renderTargets_.size() >= 2)
		{
			// 偶数個のPostEffectでGameRenderTargetへ戻った場合もalpha=1のコピーPSOを通して最終合成を不透明化する。
			CopyRenderTarget(gameRT, renderTargets_[1], commandList);
			CopyRenderTarget(renderTargets_[1], gameRT, commandList);
		}

		// Main ViewportのImGui::Imageが読めるよう最終GameRenderTargetをSRV状態にしておく
		TransitionTo(gameRT, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	}

	void PostEffectManager::BeginGameRenderTargetOverlay()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		auto& gameRT = renderTargets_[0];

		// SceneRenderTarget_GameViewportRenderTargetへ2Dを重ねる直前でSRVからRTVへ戻す。
		TransitionTo(gameRT, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->OMSetRenderTargets(1, &gameRT.rtvHandle, false, nullptr);
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);
	}

	void PostEffectManager::EndGameRenderTargetOverlay()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		auto& gameRT = renderTargets_[0];

		// ImGui::Imageがこの後SRVとして読むためGameRenderTargetを読み取り状態へ戻す
		TransitionTo(gameRT, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void PostEffectManager::BindSceneRenderTarget()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		auto& rt = renderTargets_[0];

		// SceneRenderTarget_GameViewportRenderTargetを再バインドする直前でRTV状態を保証する。
		TransitionTo(rt, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// 深度を描画可能状態へ戻す
		if (depthResource_ && depthState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			dxCommon_->ResourceTransition(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		commandList->OMSetRenderTargets(1, &rt.rtvHandle, false, &dsvHandle);
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);
	}


	/// -------------------------------------------------------------
	///				　	　		ImGui描画
	/// -------------------------------------------------------------
	void PostEffectManager::ImGuiRender(bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのPost Effect Settings表示フラグが閉じている間は後処理UIを生成しない
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		ImGui::Begin("Post Effect Settings", pOpen);

		for (const auto& [name, category] : effectCategory_)
		{
			ImGui::Checkbox(name.c_str(), &effectEnabled_[name]);
			if (effectEnabled_[name])
			{
				postEffects_[name]->DrawImGui();
			}
		}
		ImGui::End();
#else
		(void)pOpen;
#endif // USE_IMGUI
	}

	/// -------------------------------------------------------------
	///				　	エフェクトの取得
	/// -------------------------------------------------------------
	IPostEffect* PostEffectManager::GetEffect(const std::string& effectName)
	{
		auto it = postEffects_.find(effectName);
		if (it == postEffects_.end()) {
			return nullptr;
		}
		return it->second.get();
	}

	/// -------------------------------------------------------------
	///				　GameRenderTargetのSRV取得
	/// -------------------------------------------------------------
	uint32_t PostEffectManager::GetGameRenderTargetSrvIndex() const
	{
		// Main Viewportへ渡すSRVは固定のGameRenderTarget(renderTargets_[0])に集約する
		return renderTargets_.empty() ? UINT32_MAX : renderTargets_[0].srvIndex;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE PostEffectManager::GetGameRenderTargetSrvHandleGPU() const
	{
		const uint32_t srvIndex = GetGameRenderTargetSrvIndex();
		if (srvIndex == UINT32_MAX)
		{
			return {};
		}

		// ImGui::ImageにはSRVManagerのGPUハンドルをImTextureIDとして渡す
		return SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
	}

	void PostEffectManager::RequestGameRenderTargetResize(uint32_t width, uint32_t height)
	{
		// Main Viewportの表示可能サイズが変わった時だけGameViewportRenderTargetを作り直す。
		Resize(width, height);
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
			RenderTarget::kInitialState,		// currentState_ と同じ初期状態から管理を開始する
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
		HRESULT hr = S_FALSE;
		hr = dxCommon_->GetDevice()->CreateCommittedResource(
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
			rt.resource = CreateRenderTextureResource(renderTargetWidth_, renderTargetHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTextureClearColor_);
			// Scene/Game/PostEffectの役割名を付け、D3D12 DebugLayerのUnnamed Resourceをなくす。
			rt.resource->SetName(rt.debugName);
			rt.currentState_ = RenderTarget::kInitialState;

			// RTVの生成
			rt.rtvIndex = RTVManager::GetInstance()->Allocate();
			RTVManager::GetInstance()->CreateRTVForTexture2D(rt.rtvIndex, rt.resource.Get());
			rt.rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(rt.rtvIndex);

			// SRVの生成
			rt.srvIndex = SRVManager::GetInstance()->Allocate();
			SRVManager::GetInstance()->CreateSRVForTexture2D(rt.srvIndex, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

			// UAVの生成
			rt.uavIndex = UAVManager::GetInstance()->Allocate();
			UAVManager::GetInstance()->CreateUAVForTexture2D(rt.uavIndex, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 0); // UAVはTexture2Dとして生成

			// UAVヒープ側にも“入力用SRV”を複製
			rt.srvIndexOnUavHeap = UAVManager::GetInstance()->Allocate();
			UAVManager::GetInstance()->CreateSRVForTexture2DOnThisHeap(rt.srvIndexOnUavHeap, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
		}

		// 深度バッファの生成
		depthResource_ = CreateDepthBufferResource(renderTargetWidth_, renderTargetHeight_);
		depthResource_->SetName(L"PostEffectManager DepthBuffer");

		// SRVの確保（深度用）
		depthDsvIndex_ = DSVManager::GetInstance()->Allocate();
		DSVManager::GetInstance()->CreateDSVForTexture2D(depthDsvIndex_, depthResource_.Get());
		dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(depthDsvIndex_);

		// SRVの確保（深度用）
		dsvSrvIndex_ = SRVManager::GetInstance()->Allocate();
		SRVManager::GetInstance()->CreateSRVForTexture2D(dsvSrvIndex_, depthResource_.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
	}


	/// -------------------------------------------------------------
	///				ビューポート矩形とシザリング矩形の設定
	/// -------------------------------------------------------------
	void PostEffectManager::SetViewportAndScissorRect(uint32_t width, uint32_t height)
	{
		// Scene描画範囲をGameViewportRenderTargetの実ピクセルサイズと必ず一致させる
		viewport = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);

		// ScissorもGameViewportRenderTargetの実ピクセルサイズと必ず一致させる
		scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	}

} // namespace Ken4lowEngine
