#include "AnimationModelManager.h"
#include "DirectXCommon.h"
#include "Wireframe.h"
#include <ResourceManager.h>
#include "ShaderManager.h"


/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
AnimationModelManager* AnimationModelManager::GetInstance()
{
	static AnimationModelManager instance;
	return &instance;
}



/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void AnimationModelManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	wireframe_ = Wireframe::GetInstance();

	//// マテリアルデータの初期化
	//InitializeMaterialData();

	//// 頂点データの初期化
	//InitializeVertexData();

	//// カメラデータの初期化
	//InitializeCameraData();

	//// インデックスデータの初期化
	//InitializeIndexData();

#pragma region マテリアル用のリソースを作成しそのリソースにデータを書き込む処理を行う
// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 今回は赤を書き込んでみる
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true; // 平行光源を有効にする
	materialData->shininess = 1.0f;
	materialData->uvTransform = Matrix4x4::MakeIdentity();
#pragma endregion


#pragma region 頂点バッファデータの開始位置サイズおよび各頂点のデータ構造を指定
	// 頂点バッファビューを作成する
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * (modelData.vertices.size()));
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * (modelData.vertices.size()));						 // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);														 // 1頂点あたりのサイズ

	VertexData* vertexData = nullptr;																			 // 頂点リソースにデータを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));										 // 書き込むためのアドレスを取得

	// モデルデータの頂点データをコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// アンマップ
	vertexResource->Unmap(0, nullptr);
#pragma endregion


	//// カメラ用のリソースを作る
	//cameraResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
	//// 書き込むためのアドレスを取得
	//cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	//// カメラの初期位置
	//cameraData->worldPosition = camera_->GetTranslate();

	//// インデックスデータを作成
	//indexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * modelData.indices.size());
	//indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	//indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	//indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	//uint32_t* indexData = nullptr;
	//indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	//std::memcpy(indexData, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());

	//// パイプラインを生成
	//CreatePSO();
}


/// -------------------------------------------------------------
///				ルートシグネチャを生成する処理
/// -------------------------------------------------------------
void AnimationModelManager::CreateRootSignature()
{
	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;            // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		   // 0～1の範囲外をリピート
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;		   // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;						   // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;								   // レジスタ番号0
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	   // ピクセルシェーダーで使用
	descriptionRootSignature.pStaticSamplers = staticSamplers;			   // サンプラの設定
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers); // サンプラの数

	// DescriptorRangeの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 追加する SRV の DescriptorRange 設定
	D3D12_DESCRIPTOR_RANGE srvDescriptorRange{};
	srvDescriptorRange.BaseShaderRegister = 1; // register(t0) に対応
	srvDescriptorRange.NumDescriptors = 1;
	srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートシグネチャの生成
	D3D12_ROOT_PARAMETER rootParameters[8] = {};

	// マテリアル用のルートシグパラメータの設定
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // 定数バッファビュー
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[0].Descriptor.ShaderRegister = 0;                    // レジスタ番号0

	// TransformationMatrix用のルートシグネチャの設定
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // 定数バッファビュー
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // バーテックスシェーダーで使用
	rootParameters[1].Descriptor.ShaderRegister = 0;					 // レジスタ番号0

	// テクスチャのディスクリプタテーブル
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;      // ディスクリプタテーブル
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 	           // ピクセルシェーダーで使用
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;             // ディスクリプタテーブルの設定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // ディスクリプタテーブルの数

	// カメラ用のルートシグネチャの設定
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用
	rootParameters[3].Descriptor.ShaderRegister = 1; 					// レジスタ番号1

	// 平行光源用のルートシグネチャの設定
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[4].Descriptor.ShaderRegister = 2; 					// レジスタ番号2

	// ポイントライト用のルートシグネチャの設定
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[5].Descriptor.ShaderRegister = 3; 					// レジスタ番号3

	// スポットライトのルートシグネチャの設定
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 定数バッファビュー
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // バーテックスシェーダーで使用
	rootParameters[6].Descriptor.ShaderRegister = 4; 					// レジスタ番号4

	// SRV の設定（バーテックスシェーダーで使用）
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VS で使う
	rootParameters[7].DescriptorTable.pDescriptorRanges = &srvDescriptorRange;
	rootParameters[7].DescriptorTable.NumDescriptorRanges = 1;

	// ルートシグネチャの設定
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	ComPtr <ID3DBlob> signatureBlob = nullptr;
	ComPtr <ID3DBlob> errorBlob = nullptr;

	// シリアライズしてバイナリに変換
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリをもとにルートシグネチャ生成
	/*hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));*/
}


/// -------------------------------------------------------------
///						パイプラインを生成
/// -------------------------------------------------------------
void AnimationModelManager::CreatePSO()
{
	// ルートシグネチャの生成
	CreateRootSignature();
}


/// -------------------------------------------------------------
///					マテリアルデータの初期化処理
/// -------------------------------------------------------------
void AnimationModelManager::InitializeMaterialData()
{
#pragma region マテリアル用のリソースを作成しそのリソースにデータを書き込む処理を行う
	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 今回は赤を書き込んでみる
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true; // 平行光源を有効にする
	materialData->shininess = 1.0f;
	materialData->uvTransform = Matrix4x4::MakeIdentity();
#pragma endregion
}


/// -------------------------------------------------------------
///						頂点データの初期化処理
/// -------------------------------------------------------------
void AnimationModelManager::InitializeVertexData()
{
#pragma region 頂点バッファデータの開始位置サイズおよび各頂点のデータ構造を指定
	// 頂点バッファビューを作成する
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * (modelData.vertices.size()));
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * (modelData.vertices.size()));						 // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);														 // 1頂点あたりのサイズ

	VertexData* vertexData = nullptr;																			 // 頂点リソースにデータを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));										 // 書き込むためのアドレスを取得

	// モデルデータの頂点データをコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// アンマップ
	vertexResource->Unmap(0, nullptr);
#pragma endregion
}


/// -------------------------------------------------------------
///					　カメラデータの初期化処理
/// -------------------------------------------------------------
void AnimationModelManager::InitializeCameraData()
{
	// カメラ用のリソースを作る
	cameraResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
	// 書き込むためのアドレスを取得
	//cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// カメラの初期位置
	//cameraData->worldPosition = camera_->GetTranslate();
}


/// -------------------------------------------------------------
///					インデックスデータの初期化処理
/// -------------------------------------------------------------
void AnimationModelManager::InitializeIndexData()
{
	// インデックスデータを作成
	indexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	uint32_t* indexData = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	std::memcpy(indexData, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());
}
