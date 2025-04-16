#include "Object3D.h"
#include "ImGuiManager.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"

#include "ModelManager.h"
#include "Model.h"

#include "Object3DCommon.h"

#include "ParameterManager.h"


/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void Object3D::Initialize(const std::string& fileName)
{
	dxCommon = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = ModelManager::GetInstance()->LoadModelFile("Resources", fileName);

	// .objファイルの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);

	// 読み込んだテクスチャ番号を取得
	modelData.material.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath);

	worldTransform.Initialize();

	// マテリアルデータの初期化処理
	InitializeMaterial();

	// 頂点データの初期化
	InitializeVertexBufferData();

	// カメラデータの初期化処理
	InitializeCameraResource();

}


/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void Object3D::Update()
{
	worldTransform.Update();
}


/// -------------------------------------------------------------
///					　		ImGuiの描画
/// -------------------------------------------------------------
void Object3D::DrawImGui()
{
	ImGui::DragFloat3("Position", &worldTransform.translate_.x, 0.01f); // 座標変更
	ImGui::DragFloat3("Rotation", &worldTransform.rotate_.x, 0.01f);   // 回転変更
	ImGui::DragFloat3("Scale", &worldTransform.scale_.x, 0.01f);       // スケール変更

	// カメラの設定
	if (ImGui::CollapsingHeader("Camera Settings"))
	{
		static float cameraPosition[3] = { cameraData->worldPosition.x, cameraData->worldPosition.y, cameraData->worldPosition.z };
		if (ImGui::SliderFloat3("Camera Position", cameraPosition, -20.0f, 20.0f))
		{
			cameraData->worldPosition = { cameraPosition[0], cameraPosition[1], cameraPosition[2] };
		}
	}
}


/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void Object3D::Draw()
{
	// 定数バッファビュー (CBV) とディスクリプタテーブルの設定
	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // モデル用VBV
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	worldTransform.SetPipeline();

	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, modelData.material.gpuHandle);
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

	// モデルの描画
	dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}


/// -------------------------------------------------------------
///					　モデルの設定
/// -------------------------------------------------------------
void Object3D::SetModel(const std::string& filePath)
{
	// モデルを検索してセットする (例: 方法1)
	model_ = std::move(ModelManager::GetInstance()->FindModel(filePath));

	// モデルがセットされた後に初期化が必要な場合
	if (model_) {
		model_->Initialize("Resources", filePath);
	}
}


/// -------------------------------------------------------------
///					　マテリアルの初期化処理
/// -------------------------------------------------------------
void Object3D::InitializeMaterial()
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
	materialData->enableLighting = true; // 平行光源を有効にする
	materialData->shininess = 1.0f;
	materialData->uvTransform = Matrix4x4::MakeIdentity();
#pragma endregion
}


void Object3D::InitializeCameraResource()
{
	// カメラ用のリソースを作る
	cameraResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(CameraForGPU));
	// 書き込むためのアドレスを取得
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// カメラの初期位置
	cameraData->worldPosition = camera_->GetTranslate();
}


/// -------------------------------------------------------------
///				　頂点バッファデータの初期化
/// -------------------------------------------------------------
void Object3D::InitializeVertexBufferData()
{
#pragma region 頂点バッファデータの開始位置サイズおよび各頂点のデータ構造を指定
	// 頂点バッファビューを作成する
	vertexResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * (modelData.vertices.size() + TotalVertexCount));
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * (modelData.vertices.size() + TotalVertexCount));	 // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);														 // 1頂点あたりのサイズ
#pragma endregion

	VertexData* vertexData = nullptr;																			 // 頂点リソースにデータを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));										 // 書き込むためのアドレスを取得

	// モデルデータの頂点データをコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// アンマップ
	vertexResource->Unmap(0, nullptr);

	// 球体の頂点データをコピー
	VertexData* sphereVertexData = vertexData + modelData.vertices.size();
	auto calculateVertex = [](float lat, float lon, float u, float v) {
		VertexData vertex;
		vertex.position = { cos(lat) * cos(lon), sin(lat), cos(lat) * sin(lon), 1.0f };
		vertex.texcoord = { u, v };
		vertex.normal = { vertex.position.x, vertex.position.y, vertex.position.z };
		return vertex;
		};

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex; // θ
		float nextLat = lat + kLatEvery;

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			float u = float(lonIndex) / float(kSubdivision);
			float v = 1.0f - float(latIndex) / float(kSubdivision);
			float lon = lonIndex * kLonEvery; // Φ
			float nextLon = lon + kLonEvery;

			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;

			// 6つの頂点を計算
			sphereVertexData[start + 0] = calculateVertex(lat, lon, u, v);
			sphereVertexData[start + 1] = calculateVertex(nextLat, lon, u, v - 1.0f / float(kSubdivision));
			sphereVertexData[start + 2] = calculateVertex(lat, nextLon, u + 1.0f / float(kSubdivision), v);
			sphereVertexData[start + 3] = calculateVertex(nextLat, nextLon, u + 1.0f / float(kSubdivision), v - 1.0f / float(kSubdivision));
			sphereVertexData[start + 4] = calculateVertex(lat, nextLon, u + 1.0f / float(kSubdivision), v);
			sphereVertexData[start + 5] = calculateVertex(nextLat, lon, u, v - 1.0f / float(kSubdivision));
		}
	}
}
