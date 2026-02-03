#include "RadialBlurEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <UAVManager.h>
#include <ShaderCompiler.h>
#include <WinApp.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>

namespace Ken4lowEngine
{
#endif // USE_IMGUI

/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void RadialBlurEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成（コンピュート用）
	computeRootSignature_ = builder->CreateComputeRootSignature();

	// パイプラインの生成（コンピュート用）
	computePipelineState_ = builder->CreateComputePipeline(ShaderCompiler::GetShaderPath(L"RadialBlurEffect", L".CS.hlsl"), computeRootSignature_.Get());

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(RadialBlurSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurSetting_));

	// ラジアルブラーの設定
	radialBlurSetting_->center = Vector2(0.5f, 0.5f); // 中心座標
	radialBlurSetting_->blurStrength = 0.3f; // ブラー強度
	radialBlurSetting_->sampleCount = 16.0f; // サンプル数

	// 名前の設定
	constantBuffer_->SetName(L"RadialBlurEffect::ConstantBuffer");
	computePipelineState_->SetName(L"RadialBlurEffect::ComputePipelineState");
	computeRootSignature_->SetName(L"RadialBlurEffect::ComputeRootSignature");
}

void RadialBlurEffect::Finalize()
{
	// Mapして保持している生ポインタを無効化（Unmapは安全のため）
	if (constantBuffer_ && radialBlurSetting_) {
		constantBuffer_->Unmap(0, nullptr);
		radialBlurSetting_ = nullptr;
	}
	else {
		radialBlurSetting_ = nullptr;
	}

	// D3Dリソース解放
	constantBuffer_.Reset();
	computePipelineState_.Reset();
	computeRootSignature_.Reset();

	// 借り物参照を切る
	dxCommon_ = nullptr;
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void RadialBlurEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	(void)dsvIndex; // 未使用

	// コンピュート用のルートシグネチャとPSOを設定
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());

	// SRVとUAVを設定（ディスクリプタテーブル）
	commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex)); // t0
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex)); // u0

	// CBVを設定（b0）
	commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress()); // b0

	// スレッドグループの数を計算して Dispatch
	const uint32_t threadGroupSizeX = 8;
	const uint32_t threadGroupSizeY = 8;

	// レンダーターゲットの解像度（仮に 1280x720）
	uint32_t width = dxCommon_->GetClientWidth(); // ウィンドウの幅
	uint32_t height = dxCommon_->GetClientHeight(); // ウィンドウの高さ

	uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
	uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

	// ディスパッチの実行
	commandList->Dispatch(groupCountX, groupCountY, 1);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void RadialBlurEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::SliderFloat("Blur Strength", &radialBlurSetting_->blurStrength, 0.0f, 5.0f);
	ImGui::SliderFloat("Sample Count", &radialBlurSetting_->sampleCount, 1.0f, 64.0f);
	ImGui::SliderFloat2("Center", &radialBlurSetting_->center.x, 0.0f, 1.0f);
#endif // USE_IMGUI
}

} // namespace Ken4lowEngine
