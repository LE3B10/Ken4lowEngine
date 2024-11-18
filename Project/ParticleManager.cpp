#include "ParticleManager.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <MatrixMath.h>
#include <LogString.h>


/// -------------------------------------------------------------
///				    シングルトンインスタンス
/// -------------------------------------------------------------
ParticleManager* ParticleManager::GetInstance()
{
    static ParticleManager instance;
    return &instance;
}


/// -------------------------------------------------------------
///				           初期化処理
/// -------------------------------------------------------------
void ParticleManager::Initialize(DirectXCommon* dxCommon)
{
    // ランダムエンジンの初期化
	std::random_device seedGenerator;
	std::mt19937 randomEngine(seedGenerator());

	// ⊿t を定義。とりあえず60fps固定してあるが、実時間を計測して可変fpsで動かせるようにする
	const float kDeltaTime = 1.0f / 60.0f;

	// 描画するインスタンス数
	uint32_t numInstance = 0;

	// エミッター
	Emitter emitter{};
	emitter.count = 3;
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;

	emitter.transform = {
		{1.0f, 1.0f, 1.0f },
		{0.0f, 0.0f, 0.0f },
		{0.0f, 0.0f, 0.0f }
	};

	// パーティクルをリストで管理
	std::list<Particle> particles;


	// パイプライン生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// ルートシグネチャ
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータ
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリに変換
	ComPtr<ID3DBlob> signatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリをもとにルートシグネチャ生成
	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

	// InputLayoutの設定を行う
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOOD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // 頂点データの初期化（座標など）
    
	// 頂点リソース生成
    ComPtr<ID3D12Resource> instanceResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(ParticleForGPU));
    
	// 頂点バッファビュー（VBV）の作成
    

    // 書き込むためのアドレスを取得
	// 頂点リソースに頂点データを書き込む
    ParticleForGPU* instanceData = nullptr;
    instanceResource->Map(0, nullptr, reinterpret_cast<void**>(&instanceData));

    // 単位行列を書き込んでおく
    for (uint32_t index = 0; index < kNumMaxInstance; ++index)
    {
        instanceData[index].WVP = MakeIdentity();
        instanceData[index].World = MakeIdentity();
    }
}
