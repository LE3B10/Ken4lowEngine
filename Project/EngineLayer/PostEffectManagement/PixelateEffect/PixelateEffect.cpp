#include "PixelateEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <ShaderCompiler.h>
#include <WinApp.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void PixelateEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// CS 用ルートシグネチャ & パイプライン 
	computeRootSignature_ = builder->CreateComputeRootSignature();
	computePipelineState_ = builder->CreateComputePipeline(ShaderCompiler::GetShaderPath(L"PixelateEffect", L".CS.hlsl"), computeRootSignature_.Get());

	// 定数バッファ
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(PixelateSetting));
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&pixelateSetting_));

	// デフォルト設定
	pixelateSetting_->screenSize = { (float)WinApp::kClientWidth,(float)WinApp::kClientHeight };
	pixelateSetting_->blockSize = 8.0f;  // 8x8 ピクセルブロック
	pixelateSetting_->strength = 1.0f;  // 最初はフルピクセル化

	// 名前の設定
	constantBuffer_->SetName(L"PixelateEffect ConstantBuffer");
	computePipelineState_->SetName(L"PixelateEffect PipelineState");
	computeRootSignature_->SetName(L"PixelateEffect RootSignature");
}

void PixelateEffect::Finalize()
{
	// Mapしているポインタを無効化（Unmapは安全のため）
	if (constantBuffer_) {
		constantBuffer_->Unmap(0, nullptr);
	}
	pixelateSetting_ = nullptr;

	// D3Dリソース解放
	constantBuffer_.Reset();
	computePipelineState_.Reset();
	computeRootSignature_.Reset();

	// 借り物参照を切る
	dxCommon_ = nullptr;
}

/// -------------------------------------------------------------
///				　		SRV/UAV/DSV適用処理
/// -------------------------------------------------------------
void PixelateEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	(void)dsvIndex;

	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());

	// t0: 入力 (SRV), u0: 出力 (UAV), b0: CBV
	commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
	commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress());

	// 画面サイズに合わせて Dispatch（GrayScale と同じ）
	const uint32_t threadGroupSizeX = 8;
	const uint32_t threadGroupSizeY = 8;

	uint32_t width = WinApp::kClientWidth;
	uint32_t height = WinApp::kClientHeight;

	uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
	uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

	commandList->Dispatch(groupCountX, groupCountY, 1);
}

/// -------------------------------------------------------------
///				　		　　ImGui描画処理
/// -------------------------------------------------------------
void PixelateEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::SliderFloat("Block Size", &pixelateSetting_->blockSize, 1.0f, 128.0f);
	ImGui::SliderFloat("Strength", &pixelateSetting_->strength, 0.0f, 1.0f);
#endif
}
