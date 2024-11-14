#include "Model.h"
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include "ResourceManager.h"

void Model::Initialize()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// マテリアルの初期化処理
	InitializeMaterial(dxCommon);

	// 座標変換行列の初期化処理
	InitializeTransfomation(dxCommon);

	// 平行光源の初期化処理
	ParalllelLightSorce(dxCommon);
}

/// -------------------------------------------------------------
///					　マテリアルの初期化処理
/// -------------------------------------------------------------
void Model::InitializeMaterial(DirectXCommon* dxCommon)
{
#pragma region マテリアル用のリソースを作成しそのリソースにデータを書き込む処理を行う
	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 今回は赤を書き込んでみる
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true;
	// UVTramsform行列を単位行列で初期化
	materialData->uvTransform = MakeIdentity();
#pragma endregion
}


/// -------------------------------------------------------------
///					　座標変換行列の初期化処理
/// -------------------------------------------------------------
void Model::InitializeTransfomation(DirectXCommon* dxCommon)
{
#pragma region WVP行列データを格納するバッファリソースを生成し初期値として単位行列を設定
	//WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	wvpResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix));

	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込んでおく
	wvpData->World = MakeIdentity();
	wvpData->WVP = MakeIdentity();
#pragma endregion
}


/// -------------------------------------------------------------
///					　平行光源の初期化処理
/// -------------------------------------------------------------
void Model::ParalllelLightSorce(DirectXCommon* dxCommon)
{
#pragma region 平行光源のプロパティ 色 方向 強度 を格納するバッファリソースを生成しその初期値を設定
	//平行光源用のリソースを作る
	directionalLightResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(DirectionalLight));
	//書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	directionalLightData->color = { 1.0f,1.0f,1.0f ,1.0f };
	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	directionalLightData->intensity = 1.0f;
#pragma endregion
}
