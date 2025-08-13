#include "GrayScaleEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <UAVManager.h>
#include <ShaderCompiler.h>
#include <WinApp.h>

#include <cassert>
#include <imgui.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void GrayScaleEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成（コンピュート用）
	computeRootSignature_ = builder->CreateComputeRootSignature();

	// パイプラインの生成（コンピュート用）
	computePipelineState_ = builder->CreateComputePipeline(ShaderCompiler::GetShaderPath(L"GrayScaleEffect", L".CS.hlsl"), computeRootSignature_.Get());

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(GrayScaleSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&grayScaleSetting_));

	// グレイスケールエフェクトの設定
	grayScaleSetting_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 強度
}


/// -------------------------------------------------------------
///				コンピュートシェーダーによる適用処理
/// -------------------------------------------------------------
void GrayScaleEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	// ① コンピュート用のルートシグネチャとPSOを設定
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());

	// ② SRVとUAVを設定（ディスクリプタテーブル）
	commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));  // t0
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex)); // u0

	// ③ CBVを設定（b0）
	commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress()); // b0

	// ④ スレッドグループの数を計算して Dispatch
	const uint32_t threadGroupSizeX = 8;
	const uint32_t threadGroupSizeY = 8;

	// レンダーターゲットの解像度（仮に 1280x720）
	uint32_t width = WinApp::kClientWidth; // ウィンドウの幅
	uint32_t height = WinApp::kClientHeight; // ウィンドウの高さ

	uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
	uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

	commandList->Dispatch(groupCountX, groupCountY, 1);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void GrayScaleEffect::DrawImGui()
{
	ImGui::ColorEdit4("GrayScale Color", &grayScaleSetting_->color.x);
	ImGui::Text("GrayScale Effect");
	ImGui::Separator();
	ImGui::Text("Intensity: %f", grayScaleSetting_->color.x);
	ImGui::Separator();
}
