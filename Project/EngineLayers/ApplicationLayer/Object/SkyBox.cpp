#include "SkyBox.h"
#include "DirectXCommon.h"
#include <ResourceManager.h>
#include "TextureManager.h"
#include "Object3DCommon.h"
#include "DebugCamera.h"


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void SkyBox::Initialize(const std::string& filePath)
{
	const std::string FilePath = "Resources/" + filePath;
	TextureManager::GetInstance()->LoadTexture(FilePath);

	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	dxCommon_ = DirectXCommon::GetInstance();

	gpuHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU(FilePath);

	worldTransform_.scale_ = { 50.0f, 50.0f, 50.0f };
	worldTransform_.rotate_ = { 0.0f, 0.0f, 0.0f };
	worldTransform_.translate_ = { 0.0f, 0.0f, 0.0f };

	// マテリアルデータの初期化
	InitializeMaterial();

	// 頂点データの初期化処理
	InitializeVertexBufferData();

	// インデックスデータの初期化
	InitializeIndexData();

	// TransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	wvpResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));
	// 座標変換行列リソースにデータを書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

	// 単位行列を書き込んでおく
	wvpData->World = Matrix4x4::MakeIdentity();
	wvpData->WVP = Matrix4x4::MakeIdentity();
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void SkyBox::Update()
{

	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
	if (isDebugCamera_)
	{
#ifdef _DEBUG
		debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMatrix();
		camera_->SetViewProjectionMatrix(debugViewProjectionMatrix_);
		worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, debugViewProjectionMatrix_);
#endif // _DEBUG
	}
	else
	{
		viewProjectionMatrix_ = Matrix4x4::Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());
		camera_->SetViewProjectionMatrix(viewProjectionMatrix_);
		worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix_);
	}

	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = worldMatrix;
}


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void SkyBox::Draw()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // スプライト用VBV
	commandList->IASetIndexBuffer(&indexBufferView); // IBVの設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource.Get()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

	// ディスクリプタテーブルの設定
	commandList->SetGraphicsRootDescriptorTable(2, gpuHandle_);

	commandList->DrawIndexedInstanced(kNumVertex, 1, 0, 0, 0);
}


/// -------------------------------------------------------------
///				　マテリアルデータの初期化処理
/// -------------------------------------------------------------
void SkyBox::InitializeMaterial()
{
	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->uvTransform = Matrix4x4::MakeIdentity();
}


/// -------------------------------------------------------------
///				　	頂点データの初期化処理
/// -------------------------------------------------------------
void SkyBox::InitializeVertexBufferData()
{
	// 頂点リソースを作る
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * kNumVertex);

	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * kNumVertex;
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	// 右面（+X）
	vertexData_[0] = { {  1.0f,  1.0f,  1.0f, 1.0f }, {  1.0f,  1.0f,  1.0f } };
	vertexData_[1] = { {  1.0f,  1.0f, -1.0f, 1.0f }, {  1.0f,  1.0f, -1.0f } };
	vertexData_[2] = { {  1.0f, -1.0f,  1.0f, 1.0f }, {  1.0f, -1.0f,  1.0f } };
	vertexData_[3] = { {  1.0f, -1.0f, -1.0f, 1.0f }, {  1.0f, -1.0f, -1.0f } };

	// 左面（-X）
	vertexData_[4] = { { -1.0f,  1.0f, -1.0f, 1.0f }, { -1.0f,  1.0f, -1.0f } };
	vertexData_[5] = { { -1.0f,  1.0f,  1.0f, 1.0f }, { -1.0f,  1.0f,  1.0f } };
	vertexData_[6] = { { -1.0f, -1.0f, -1.0f, 1.0f }, { -1.0f, -1.0f, -1.0f } };
	vertexData_[7] = { { -1.0f, -1.0f,  1.0f, 1.0f }, { -1.0f, -1.0f,  1.0f } };

	// 前面（+Z）
	vertexData_[8] = { { -1.0f,  1.0f,  1.0f, 1.0f }, { -1.0f,  1.0f,  1.0f } };
	vertexData_[9] = { {  1.0f,  1.0f,  1.0f, 1.0f }, {  1.0f,  1.0f,  1.0f } };
	vertexData_[10] = { { -1.0f, -1.0f,  1.0f, 1.0f }, { -1.0f, -1.0f,  1.0f } };
	vertexData_[11] = { {  1.0f, -1.0f,  1.0f, 1.0f }, {  1.0f, -1.0f,  1.0f } };

	// 背面（-Z）
	vertexData_[12] = { { -1.0f,  1.0f, -1.0f, 1.0f }, { -1.0f,  1.0f, -1.0f } };
	vertexData_[13] = { {  1.0f,  1.0f, -1.0f, 1.0f }, {  1.0f,  1.0f, -1.0f } };
	vertexData_[14] = { { -1.0f, -1.0f, -1.0f, 1.0f }, { -1.0f, -1.0f, -1.0f } };
	vertexData_[15] = { {  1.0f, -1.0f, -1.0f, 1.0f }, {  1.0f, -1.0f, -1.0f } };

	// 上面（+Y）
	vertexData_[16] = { { -1.0f,  1.0f, -1.0f, 1.0f }, { -1.0f,  1.0f, -1.0f } };
	vertexData_[17] = { {  1.0f,  1.0f, -1.0f, 1.0f }, {  1.0f,  1.0f, -1.0f } };
	vertexData_[18] = { { -1.0f,  1.0f,  1.0f, 1.0f }, { -1.0f,  1.0f,  1.0f } };
	vertexData_[19] = { {  1.0f,  1.0f,  1.0f, 1.0f }, {  1.0f,  1.0f,  1.0f } };

	// 下面（-Y）
	vertexData_[20] = { { -1.0f, -1.0f,  1.0f, 1.0f }, { -1.0f, -1.0f,  1.0f } };
	vertexData_[21] = { {  1.0f, -1.0f,  1.0f, 1.0f }, {  1.0f, -1.0f,  1.0f } };
	vertexData_[22] = { { -1.0f, -1.0f, -1.0f, 1.0f }, { -1.0f, -1.0f, -1.0f } };
	vertexData_[23] = { {  1.0f, -1.0f, -1.0f, 1.0f }, {  1.0f, -1.0f, -1.0f } };

	// 頂点データ更新
	memcpy(vertexData_, &vertexData_[0], sizeof(VertexData) * kNumVertex);
}


/// -------------------------------------------------------------
///				　インデックスデータの初期化処理
/// -------------------------------------------------------------
void SkyBox::InitializeIndexData()
{
	// インデックスバッファ用のリソースを作成
	indexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * kNumIndex);

	// インデックスバッファビューの設定
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * kNumIndex;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// インデックスデータのマッピング
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

	// インデックスバッファの順番を修正（時計回り）

	// 右面（+X）
	indexData_[0] = 2; indexData_[1] = 1; indexData_[2] = 0;
	indexData_[3] = 3; indexData_[4] = 1; indexData_[5] = 2;

	// 左面（-X）
	indexData_[6] = 6; indexData_[7] = 5; indexData_[8] = 4;
	indexData_[9] = 7; indexData_[10] = 5; indexData_[11] = 6;

	// 前面（+Z）
	indexData_[12] = 10; indexData_[13] = 9; indexData_[14] = 8;
	indexData_[15] = 11; indexData_[16] = 9; indexData_[17] = 10;

	// 後面（-Z）
	indexData_[18] = 12; indexData_[19] = 13; indexData_[20] = 14;
	indexData_[21] = 14; indexData_[22] = 13; indexData_[23] = 15;

	// 上面（+Y）
	indexData_[24] = 17; indexData_[25] = 16; indexData_[26] = 18;
	indexData_[27] = 17; indexData_[28] = 18; indexData_[29] = 19;

	// 下面（-Y）
	indexData_[30] = 21; indexData_[31] = 20; indexData_[32] = 22;
	indexData_[33] = 21; indexData_[34] = 22; indexData_[35] = 23;

	memcpy(indexData_, &indexData_[0], sizeof(uint32_t) * kNumIndex);
}