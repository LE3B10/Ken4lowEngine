#include "SkyBox.h"
#include "DirectXCommon.h"
#include <ResourceManager.h>
#include "TextureManager.h"
#include "CameraManager.h"
#include "DebugCamera.h"
#include <SkyBoxManager.h>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         初期化処理
	/// -------------------------------------------------------------
	void SkyBox::Initialize(const std::string& filePath)
	{
		// 描画やリソース生成に使う共通クラスを取得する
		dxCommon_ = DirectXCommon::GetInstance();

		// 既定の描画カメラを取得する
		camera_ = CameraManager::GetInstance()->GetMainCamera();

		// 環境テクスチャを読み込む
		SetTexture(filePath);

		// SkyBox は十分大きなキューブとして配置する
		worldTransform_.scale_ = { 10000.0f, 10000.0f, 10000.0f };
		worldTransform_.rotate_ = { 0.0f, 0.0f, 0.0f };
		worldTransform_.translate_ = { 0.0f, 0.0f, 0.0f };

		// Material 定数バッファを初期化する
		InitializeMaterial();

		// 頂点バッファを初期化する
		InitializeVertexBufferData();

		// インデックスバッファを初期化する
		InitializeIndexData();

		// WVP 用定数バッファを生成する
		wvpResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));

		// CPU から書き込めるようにマップする
		wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

		// 初期状態は単位行列を書いておく
		wvpData->World = Matrix4x4::MakeIdentity();
		wvpData->WVP = Matrix4x4::MakeIdentity();
	}

	/// -------------------------------------------------------------
	///                           更新処理
	/// -------------------------------------------------------------
	void SkyBox::Update()
	{
		// アクティブカメラ位置へ追従
		worldTransform_.translate_ = CameraManager::GetInstance()->GetActiveCameraPosition();

		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);

		Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();

		worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjection);

		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;
	}

	/// -------------------------------------------------------------
	///                           描画処理
	/// -------------------------------------------------------------
	void SkyBox::Draw()
	{
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

		// SkyBox 用の共通描画設定を反映する
		SkyBoxManager::GetInstance()->SetRenderSetting();

		// 使用テクスチャ index を Material 側へ反映する
		materialData_->textureIndex = textureIndex_;

		// 頂点 / インデックスバッファを設定する
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&indexBufferView);

		// Material と WVP の定数バッファを設定する
		commandList->SetGraphicsRootConstantBufferView(0, materialResource.Get()->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

		// キューブをインデックス描画する
		commandList->DrawIndexedInstanced(kNumIndex, 1, 0, 0, 0);
	}

	void SkyBox::SetTexture(const std::string& filePath, bool reloadTexture)
	{
		if (filePath.empty())
		{
			return;
		}

		TextureManager* textureManager = TextureManager::GetInstance();
		if (reloadTexture)
		{
			textureManager->ReloadTexture(filePath);
		}
		else
		{
			textureManager->LoadTexture(filePath);
		}
		texturePath_ = filePath;
		textureIndex_ = textureManager->GetSrvIndex(filePath);
		gpuHandle_ = textureManager->GetSrvHandleGPU(filePath);
	}

	void SkyBox::SetColor(const Vector4& color)
	{
		if (materialData_)
		{
			materialData_->color = color;
		}
	}

	/// -------------------------------------------------------------
	///                マテリアルデータの初期化処理
	/// -------------------------------------------------------------
	void SkyBox::InitializeMaterial()
	{
		// Material 用定数バッファを生成する
		materialResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));

		// CPU から書き込めるようにマップする
		materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

		// 初期値を設定する
		materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialData_->uvTransform = Matrix4x4::MakeIdentity();
		materialData_->textureIndex = textureIndex_;
	}

	/// -------------------------------------------------------------
	///                頂点データの初期化処理
	/// -------------------------------------------------------------
	void SkyBox::InitializeVertexBufferData()
	{
		// SkyBox 用頂点バッファを生成する
		vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * kNumVertex);

		// 頂点バッファビューを設定する
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = sizeof(VertexData) * kNumVertex;
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		// CPU から書き込めるようにマップする
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
	}

	/// -------------------------------------------------------------
	///               インデックスデータの初期化処理
	/// -------------------------------------------------------------
	void SkyBox::InitializeIndexData()
	{
		// インデックスバッファを生成する
		indexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * kNumIndex);

		// インデックスバッファビューを設定する
		indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = sizeof(uint32_t) * kNumIndex;
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		// CPU から書き込めるようにマップする
		indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

		// 各面 2 三角形ぶんのインデックスを設定する

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
	}
} // namespace Ken4lowEngine