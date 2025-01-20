#include "Object3D.h"
#include "ImGuiManager.h"
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include "ResourceManager.h"

#include "ModelManager.h"
#include "Model.h"

#include "Object3DCommon.h"

#include "ParameterManager.h"

/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void Object3D::Initialize(Object3DCommon* object3dCommon, const std::string& fileName)
{
	dxCommon = DirectXCommon::GetInstance();

	object3dCommon_ = object3dCommon;
	
	// モデル読み込み
	modelData = ModelManager::LoadModelFile("Resources", fileName);

	// .objファイルの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);

	// 読み込んだテクスチャ番号を取得
	modelData.material.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath);

	transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	cameraTransform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-15.0f} };

	camera_ = object3dCommon_->GetDefaultCamera();

	preInitialize(dxCommon);
}


/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void Object3D::Update()
{
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 worldViewProjectionMatrix;

	if (camera_)
	{
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	}
	else
	{
		worldViewProjectionMatrix = worldMatrix;
	}

	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = worldMatrix;
}


/// -------------------------------------------------------------
///					　		ImGuiの描画
/// -------------------------------------------------------------
void Object3D::DrawImGui()
{
	if (ImGui::TreeNode("Transform"))
	{
		ImGui::DragFloat3("Position", &transform.translate.x, 0.01f); // 座標変更
		ImGui::DragFloat3("Rotation", &transform.rotate.x, 0.01f);   // 回転変更
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);       // スケール変更
		ImGui::TreePop();
	}
}


void Object3D::CameraImGui()
{
	model_->CameraImGui();
}


/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void Object3D::Draw()
{
	// 定数バッファビュー (CBV) とディスクリプタテーブルの設定
	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // モデル用VBV
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

	// ディスクリプタテーブルの設定
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, modelData.material.gpuHandle);

	// モデルの描画
	dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

	//model_->DrawCall(commandList);
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


void Object3D::preInitialize(DirectXCommon* dxCommon)
{
	InitializeMaterial(dxCommon);
	InitializeTransfomation(dxCommon);
	ParallelLightSorce(dxCommon);
	InitializeVertexBufferData(dxCommon);
}

/// -------------------------------------------------------------
///					　マテリアルの初期化処理
/// -------------------------------------------------------------
void Object3D::InitializeMaterial(DirectXCommon* dxCommon)
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
void Object3D::InitializeTransfomation(DirectXCommon* dxCommon)
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
void Object3D::ParallelLightSorce(DirectXCommon* dxCommon)
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


/// -------------------------------------------------------------
///				　頂点バッファデータの初期化
/// -------------------------------------------------------------
void Object3D::InitializeVertexBufferData(DirectXCommon* dxCommon)
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
		float lat = -pi / 2.0f + kLatEvery * latIndex; // θ
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

	// アンマップ
	vertexResource->Unmap(0, nullptr);
}
