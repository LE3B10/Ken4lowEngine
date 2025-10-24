#include "LuminanceOutlineEffect.h"
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
void LuminanceOutlineEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成（コンピュート用）
	computeRootSignature_ = builder->CreateComputeRootSignature();

	// パイプラインの生成（コンピュート用）
	computePipelineState_ = builder->CreateComputePipeline(ShaderCompiler::GetShaderPath(L"LuminanceOutlineEffect", L".CS.hlsl"), computeRootSignature_.Get());

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LuminanceOutlineSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&luminanceOutlineSetting_));

	// アウトラインの設定
	luminanceOutlineSetting_->color = Vector4(0.0f, 0.0f, 0.0f, 1.0f); // アウトラインの色（黒色）
	luminanceOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(WinApp::kClientWidth), 1.0f / static_cast<float>(WinApp::kClientHeight));
	luminanceOutlineSetting_->edgeStrength = 1.0f; // エッジ強度
	luminanceOutlineSetting_->threshold = 0.5f; // 閾値
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void LuminanceOutlineEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	(void)dsvIndex; // 未使用

	// コンピュート用のルートシグネチャとPSOを設定
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());

	// SRVとUAVを設定（ディスクリプタテーブル）
	commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));  // t0
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex)); // u0

	// CBVを設定（b0）
	commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress()); // b0

	// スレッドグループの数を計算して Dispatch
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
void LuminanceOutlineEffect::DrawImGui()
{
	ImGui::Text("Luminance Outline Effect Settings");
	ImGui::ColorEdit4("Outline Color", &luminanceOutlineSetting_->color.x);
	ImGui::SliderFloat("Edge Strength", &luminanceOutlineSetting_->edgeStrength, 0.0f, 5.0f);
	ImGui::SliderFloat("Threshold", &luminanceOutlineSetting_->threshold, 0.0f, 1.0f);
}
