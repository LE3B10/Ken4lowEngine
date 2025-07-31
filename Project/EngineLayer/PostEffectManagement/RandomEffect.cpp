#include "RandomEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>	
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>
#include <WinApp.h>

#include <cassert>
#include <imgui.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void RandomEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// コンピュート用ルートシグネチャの生成
	computeRootSignature_ = builder->CreateComputeRootSignature();

	// コンピュートパイプラインステートの生成
	computePipelineState_ = builder->CreateComputePipeline(ShaderCompiler::GetShaderPath(L"RandomEffect", L".CS.hlsl"), computeRootSignature_.Get());

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(RandomSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&randomSetting_));

	// ランダムエフェクトの設定
	randomSetting_->time = 0.0f; // 時間
	randomSetting_->useMultiply = false; // 乗算を使用するかどうか
	randomSetting_->textureSize = Vector2(WinApp::kClientWidth, WinApp::kClientHeight); // テクスチャのサイズ
}


/// -------------------------------------------------------------
///						　更新処理
/// -------------------------------------------------------------
void RandomEffect::Update()
{
	// 時間の更新
	randomSetting_->time += 1.0f / 120.0f; // 適当な値で更新	
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void RandomEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	// ① コンピュート用のルートシグネチャとPSOを設定
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());

	// ② SRVとUAVを設定（ディスクリプタテーブル）
	commandList->SetComputeRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));  // t0
	commandList->SetComputeRootDescriptorTable(1, SRVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex)); // u0

	// ③ CBVを設定（b0）
	commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress()); // b0

	// ④ スレッドグループの数を計算して Dispatch
	const uint32_t threadGroupSizeX = 8;
	const uint32_t threadGroupSizeY = 8;

	// レンダーターゲットの解像度（仮に 1280x720）
	uint32_t width = static_cast<uint32_t>(randomSetting_->textureSize.x); // ウィンドウの幅
	uint32_t height = static_cast<uint32_t>(randomSetting_->textureSize.y); // ウィンドウの高さ

	uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
	uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

	commandList->Dispatch(groupCountX, groupCountY, 1);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void RandomEffect::DrawImGui()
{
	if (ImGui::Button(randomSetting_->useMultiply ? "No Multiply" : "Apply Multiply")) {
		randomSetting_->useMultiply = !randomSetting_->useMultiply;
	}
}
