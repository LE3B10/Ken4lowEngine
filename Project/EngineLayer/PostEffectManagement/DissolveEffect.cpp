#include "DissolveEffect.h"
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
#include <TextureManager.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void DissolveEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネイチャ（コンピュート用）
	computeRootSignature_ = builder->CreateComputeRootSignature();

	// パイプライン生成（コンピュート用）
	computePipelineState_ = builder->CreateComputePipeline(ShaderCompiler::GetShaderPath(L"DissolveEffect", L".CS.hlsl"), computeRootSignature_.Get());

	// ディゾルブの設定
	std::string filePath = "Mask/Noise.png";
	TextureManager::GetInstance()->LoadTexture(filePath);

	// UAVヒープ側のインデックスを確保
	dissolveMaskSrvIndexOnUAV_ = UAVManager::GetInstance()->Allocate();

	// リソース＆メタデータを取得
	ID3D12Resource* texture = TextureManager::GetInstance()->GetResource(filePath);
	const auto& metaData = TextureManager::GetInstance()->GetMetaData(filePath);

	// UAVを作成（ディゾルブマスク用）
	UAVManager::GetInstance()->CreateSRVForTexture2DOnThisHeap(dissolveMaskSrvIndexOnUAV_, texture, metaData.format, metaData.mipLevels);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveSetting_));

	// ディゾルブの設定
	dissolveSetting_->threshold = 0.5f;
	dissolveSetting_->edgeThickness = 0.05f;
	dissolveSetting_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void DissolveEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	// ① コンピュート用のルートシグネチャとPSOを設定
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());

	// ② SRVとUAVを設定（ディスクリプタテーブル）
	commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));  // t0
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex)); // u0

	// ③ CBVを設定（b0）
	commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress()); // b0
	commandList->SetComputeRootDescriptorTable(3, UAVManager::GetInstance()->GetGPUDescriptorHandle(dissolveMaskSrvIndexOnUAV_)); // t1

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
void DissolveEffect::DrawImGui()
{
	ImGui::SliderFloat("Dissolve Threshold", &dissolveSetting_->threshold, 0.0f, 1.0f);
	ImGui::SliderFloat("Edge Thickness", &dissolveSetting_->edgeThickness, 0.0f, 1.0f);
	ImGui::ColorEdit4("Edge Color", &dissolveSetting_->edgeColor.x);
}
