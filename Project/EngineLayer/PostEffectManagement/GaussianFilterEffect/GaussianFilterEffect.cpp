#include "GaussianFilterEffect.h"
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
#endif // USE_IMGUI


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void GaussianFilterEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成（コンピュート用）
	computeRootSignature_ = builder->CreateComputeRootSignature();

	// パイプラインの生成（コンピュート用）
	computePipelineState_ = builder->CreateComputePipeline(ShaderCompiler::GetShaderPath(L"GaussianFilterEffect", L".CS.hlsl"), computeRootSignature_.Get());

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(GaussianFilterSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&gaussianFilterSetting_));

	// ガウシアンフィルタの設定
	gaussianFilterSetting_->kernelType = 1;		 // カーネルサイズ
	gaussianFilterSetting_->intensity = 1.0f;	 // 強度
	gaussianFilterSetting_->threshold = 0.0f;	 // 閾値
	gaussianFilterSetting_->sigma = 1.0f;		 // ガウス関数の標準偏差
	gaussianFilterSetting_->isHorizontal = true; // 水平方向か垂直方向か

	// 名前の設定
	constantBuffer_->SetName(L"GaussianFilterEffect ConstantBuffer");
	computePipelineState_->SetName(L"GaussianFilterEffect ComputePipelineState");
	computeRootSignature_->SetName(L"GaussianFilterEffect ComputeRootSignature");
}

void GaussianFilterEffect::Finalize()
{
	// Mapしているポインタを無効化（Unmapは安全のため）
	if (constantBuffer_) {
		constantBuffer_->Unmap(0, nullptr);
	}
	gaussianFilterSetting_ = nullptr;

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
void GaussianFilterEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
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

	// Dispatch の実行
	commandList->Dispatch(groupCountX, groupCountY, 1);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void GaussianFilterEffect::DrawImGui()
{
#ifdef USE_IMGUI
	const char* kernelOptions[] = { "3x3", "5x5", "7x7", "9x9" };
	ImGui::Combo("Kernel Size", &gaussianFilterSetting_->kernelType, kernelOptions, IM_ARRAYSIZE(kernelOptions));
	ImGui::SliderFloat("Intensity", &gaussianFilterSetting_->intensity, 0.0f, 5.0f);
	ImGui::SliderFloat("Sigma", &gaussianFilterSetting_->sigma, 0.1f, 5.0f);
	ImGui::SliderFloat("Threshold", &gaussianFilterSetting_->threshold, 0.0f, 1.0f);
	ImGui::Checkbox("Horizontal", &gaussianFilterSetting_->isHorizontal);
#endif // USE_IMGUI
}
