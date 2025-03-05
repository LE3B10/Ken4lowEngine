#include "SkyBox.h"
#include "DirectXCommon.h"
#include <ResourceManager.h>
#include "TextureManager.h"
#include "Object3DCommon.h"


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
	// ワールド行列の計算
	WorldTransform worldTransform_;
	worldTransform_.scale_ = { 40.0f, 40.0f, 40.0f };
	worldTransform_.rotate_ = { 0.0f, 0.0f, 0.0f };
	worldTransform_.translate_ = { 0.0f, 0.0f, 0.0f };

	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);

	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
	Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, Matrix4x4::Multiply(viewMatrix, projectionMatrix));

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

	// スカイボックスの頂点データ（8頂点）
	VertexData vertexData[] = {
		// 前面
		{{-1.0f,  1.0f, -1.0f, 1.0f}, {-1.0f,  1.0f, -1.0f}}, // 左上
		{{ 1.0f,  1.0f, -1.0f, 1.0f}, { 1.0f,  1.0f, -1.0f}}, // 右上
		{{-1.0f, -1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f}}, // 左下
		{{ 1.0f, -1.0f, -1.0f, 1.0f}, { 1.0f, -1.0f, -1.0f}}, // 右下

		// 背面
		{{-1.0f,  1.0f,  1.0f, 1.0f}, {-1.0f,  1.0f,  1.0f}}, // 左上
		{{ 1.0f,  1.0f,  1.0f, 1.0f}, { 1.0f,  1.0f,  1.0f}}, // 右上
		{{-1.0f, -1.0f,  1.0f, 1.0f}, {-1.0f, -1.0f,  1.0f}}, // 左下
		{{ 1.0f, -1.0f,  1.0f, 1.0f}, { 1.0f, -1.0f,  1.0f}}, // 右下
	};

	// 頂点データ更新
	memcpy(vertexData, &vertexData, sizeof(VertexData) * kNumVertex);

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
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

	// スカイボックスのインデックスデータ（36インデックス）
	uint32_t indexData[] = {
		// 前面
		0, 1, 2, 2, 1, 3,
		// 背面
		5, 4, 7, 7, 4, 6,
		// 左面
		4, 0, 6, 6, 0, 2,
		// 右面
		1, 5, 3, 3, 5, 7,
		// 上面
		4, 5, 0, 0, 5, 1,
		// 下面
		2, 3, 6, 6, 3, 7
	};


	memcpy(indexData, indexData, sizeof(uint32_t) * kNumIndex);
}