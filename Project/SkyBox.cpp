#include "SkyBox.h"
#include "DirectXCommon.h"
#include <ResourceManager.h>
#include "TextureManager.h"


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void SkyBox::Initialize(const std::string& filePath)
{
	const std::string FilePath = "Resources/" + filePath;
	TextureManager::GetInstance()->LoadTexture(FilePath);

	dxCommon_ = DirectXCommon::GetInstance();

	gpuHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU(FilePath);

	worldTransform_.Initialize();
	worldTransform_.scale_ = { 1.0f,1.0f,1.0f };
	worldTransform_.rotate_ = { 0.0f,0.0f,0.0f };
	worldTransform_.translate_ = { 0.0f,0.0f,0.0f };

	// マテリアルデータの初期化
	InitializeMaterial();

	// 頂点データの初期化処理
	InitializeVertexBufferData();

	// インデックスデータの初期化
	InitializeIndexData();
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void SkyBox::Update()
{
	// 右面 (+X)
	vertexData[0].position = { 1.0f,  1.0f,  1.0f, 1.0f };
	vertexData[1].position = { 1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[2].position = { 1.0f, -1.0f,  1.0f, 1.0f };
	vertexData[3].position = { 1.0f, -1.0f, -1.0f, 1.0f };

	// 左面 (-X)
	vertexData[4].position = { -1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[5].position = { -1.0f,  1.0f,  1.0f, 1.0f };
	vertexData[6].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[7].position = { -1.0f, -1.0f,  1.0f, 1.0f };

	// 前面 (+Z)
	vertexData[8].position = { -1.0f,  1.0f,  1.0f, 1.0f };
	vertexData[9].position = { 1.0f,  1.0f,  1.0f, 1.0f };
	vertexData[10].position = { -1.0f, -1.0f,  1.0f, 1.0f };
	vertexData[11].position = { 1.0f, -1.0f,  1.0f, 1.0f };

	// 後面 (-Z)
	vertexData[12].position = { 1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[13].position = { -1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[14].position = { 1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[15].position = { -1.0f, -1.0f, -1.0f, 1.0f };

	// 上面 (+Y)
	vertexData[16].position = { -1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[17].position = { 1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[18].position = { -1.0f,  1.0f,  1.0f, 1.0f };
	vertexData[19].position = { 1.0f,  1.0f,  1.0f, 1.0f };

	// 下面 (-Y)
	vertexData[20].position = { -1.0f, -1.0f,  1.0f, 1.0f };
	vertexData[21].position = { 1.0f, -1.0f,  1.0f, 1.0f };
	vertexData[22].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[23].position = { 1.0f, -1.0f, -1.0f, 1.0f };

	// 頂点データ更新
	memcpy(vertexData, &vertexData[0], sizeof(VertexData) * kNumVertex);

	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
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

	// ディスクリプタテーブルの設定
	commandList->SetGraphicsRootDescriptorTable(2, gpuHandle_);

	//commandList->DrawIndexedInstanced(kNumVertex, 1, 0, 0, 0);
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

	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// TransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	transformationMatrixResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));
	// 座標変換行列リソースにデータを書き込むためのアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

	// 単位行列を書き込んでおく
	transformationMatrixData->World = Matrix4x4::MakeIdentity();
	transformationMatrixData->WVP = Matrix4x4::MakeIdentity();
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

	// インデックスデータの設定（1面 = 2つの三角形 = 6インデックス）
	uint32_t indices[] =
	{
		0, 1, 2,  2, 1, 3,  // 右面 (+X)
		4, 5, 6,  6, 5, 7,  // 左面 (-X)
		8, 9, 10, 10, 9, 11, // 前面 (+Z)
		12, 13, 14, 14, 13, 15, // 後面 (-Z)
		16, 17, 18, 18, 17, 19, // 上面 (+Y)
		20, 21, 22, 22, 21, 23  // 下面 (-Y)
	};

	memcpy(indexData, indices, sizeof(uint32_t) * kNumIndex);
}