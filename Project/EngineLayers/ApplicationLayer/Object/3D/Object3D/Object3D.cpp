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

	worldTransform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	camera_ = object3dCommon_->GetDefaultCamera();

	preInitialize(dxCommon);
}


/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void Object3D::Update()
{
	Matrix4x4 worldMatrix = MakeAffineMatrix(worldTransform.scale, worldTransform.rotate, worldTransform.translate);
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
	wvpData->WorldInversedTranspose = Transpose(Inverse(worldMatrix));
}


/// -------------------------------------------------------------
///					　		ImGuiの描画
/// -------------------------------------------------------------
void Object3D::DrawImGui()
{
	ImGui::DragFloat3("Position", &worldTransform.translate.x, 0.01f); // 座標変更
	ImGui::DragFloat3("Rotation", &worldTransform.rotate.x, 0.01f);   // 回転変更
	ImGui::DragFloat3("Scale", &worldTransform.scale.x, 0.01f);       // スケール変更

	// カメラの設定
	if (ImGui::CollapsingHeader("Camera Settings"))
	{
		static float cameraPosition[3] = { cameraData->worldPosition.x, cameraData->worldPosition.y, cameraData->worldPosition.z };
		if (ImGui::SliderFloat3("Camera Position", cameraPosition, -20.0f, 20.0f))
		{
			cameraData->worldPosition = { cameraPosition[0], cameraPosition[1], cameraPosition[2] };
		}
	}

	// 平行光源の設定
	if (ImGui::CollapsingHeader("Directional Light Settings"))
	{
		if (ImGui::SliderFloat3("Directional Light Direction", &directionalLightData->direction.x, -1.0f, 1.0f))
		{
			directionalLightData->direction = Normalize(directionalLightData->direction);
		}
		ImGui::SliderFloat("Directional Light Intensity", &directionalLightData->intensity, 0.0f, 10.0f);
	}

	// 点光源の設定
	if (ImGui::CollapsingHeader("Point Light Settings"))
	{
		static float pointPosition[3] = { pointLightData->position.x, pointLightData->position.y, pointLightData->position.z };
		static float pointIntensity = pointLightData->intensity;
		static float pointRadius = pointLightData->radius;
		static float pointDecay = pointLightData->decay;

		// 点光源の位置
		if (ImGui::SliderFloat3("Point Light Position", pointPosition, -10.0f, 10.0f))
		{
			pointLightData->position = { pointPosition[0], pointPosition[1], pointPosition[2] };
		}

		// 点光源の輝度
		if (ImGui::SliderFloat("Point Light Intensity", &pointIntensity, 0.0f, 5.0f))
		{
			pointLightData->intensity = pointIntensity;
		}

		// 点光源の半径
		if (ImGui::SliderFloat("Point Light Radius", &pointRadius, 0.0f, 20.0f))
		{
			pointLightData->radius = pointRadius;
		}

		// 点光源の減衰率
		if (ImGui::SliderFloat("Point Light Decay", &pointDecay, 0.1f, 5.0f))
		{
			pointLightData->decay = pointDecay;
		}
	}

	// スポットライトの設定
	// スポットライトの設定
	if (ImGui::CollapsingHeader("Spot Light Settings"))
	{
		static float spotPosition[3] = { spotLightData->position.x, spotLightData->position.y, spotLightData->position.z };
		static float spotDirection[3] = { spotLightData->direction.x, spotLightData->direction.y, spotLightData->direction.z };
		static float spotIntensity = spotLightData->intensity;
		static float spotDistance = spotLightData->distance;
		static float spotDecay = spotLightData->decay;

		// スポットライトの開始角度と終了角度（度単位で操作）
		static float spotFalloffStartAngle = std::acos(spotLightData->cosFalloffStart) * 180.0f / std::numbers::pi_v<float>;
		static float spotConeAngle = std::acos(spotLightData->cosAngle) * 180.0f / std::numbers::pi_v<float>;


		// スポットライトの位置
		if (ImGui::SliderFloat3("Spot Light Position", spotPosition, -10.0f, 10.0f))
		{
			spotLightData->position = { spotPosition[0], spotPosition[1], spotPosition[2] };
		}

		// スポットライトの方向
		if (ImGui::SliderFloat3("Spot Light Direction", spotDirection, -1.0f, 1.0f))
		{
			spotLightData->direction = Normalize({ spotDirection[0], spotDirection[1], spotDirection[2] });
		}

		// スポットライトの輝度
		if (ImGui::SliderFloat("Spot Light Intensity", &spotIntensity, 0.0f, 10.0f))
		{
			spotLightData->intensity = spotIntensity;
		}

		// スポットライトの距離
		if (ImGui::SliderFloat("Spot Light Distance", &spotDistance, 0.0f, 50.0f))
		{
			spotLightData->distance = spotDistance;
		}

		// スポットライトの減衰率
		if (ImGui::SliderFloat("Spot Light Decay", &spotDecay, 0.0f, 5.0f))
		{
			spotLightData->decay = spotDecay;
		}

		// スポットライトの開始角度（Falloff Start Angle）
		if (ImGui::SliderFloat("Spot Light Falloff Start Angle (Degrees)", &spotFalloffStartAngle, 0.0f, 90.0f))
		{
			// 度をラジアンに変換し、余弦値を計算
			spotLightData->cosFalloffStart = std::cos(spotFalloffStartAngle * std::numbers::pi_v<float> / 180.0f);
		}

		// スポットライトの終了角度（Cone Angle）
		if (ImGui::SliderFloat("Spot Light Cone Angle (Degrees)", &spotConeAngle, 0.0f, 90.0f))
		{
			// 度をラジアンに変換し、余弦値を計算
			spotLightData->cosAngle = std::cos(spotConeAngle * std::numbers::pi_v<float> / 180.0f);
		}

		// 開始角度が終了角度より大きくならないように調整
		if (spotLightData->cosFalloffStart < spotLightData->cosAngle)
		{
			spotLightData->cosFalloffStart = spotLightData->cosAngle;
			spotFalloffStartAngle = std::acos(spotLightData->cosFalloffStart) * 180.0f / std::numbers::pi_v<float>;
		}
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
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, modelData.material.gpuHandle);
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(4, directionalLightResource->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(6, spotLightResource->GetGPUVirtualAddress());

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
///					　		前処理
/// -------------------------------------------------------------
void Object3D::preInitialize(DirectXCommon* dxCommon)
{
	InitializeMaterial(dxCommon);
	InitializeTransfomation(dxCommon);
	ParallelLightSorce(dxCommon);
	InitializeVertexBufferData(dxCommon);
	InitializeCameraResource(dxCommon);
	PointLightSource(dxCommon);
	SpotLightSource(dxCommon);
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
	materialData->enableLighting = true; // 平行光源を有効にする
	materialData->shininess = 1.0f;
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
	wvpData->WorldInversedTranspose = MakeIdentity();
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
	directionalLightData->direction = Normalize({ 0.0f, 1.0f, 0.0f });
	directionalLightData->intensity = 1.0f;
#pragma endregion
}

void Object3D::InitializeCameraResource(DirectXCommon* dxCommon)
{
	// カメラ用のリソースを作る
	cameraResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(CameraForGPU));
	// 書き込むためのアドレスを取得
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// カメラの初期位置
	cameraData->worldPosition = { 0.0f, 0.0f, -20.0f };
}

void Object3D::PointLightSource(DirectXCommon* dxCommon)
{
	// ポイントライト用のリソースを作る
	pointLightResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(PointLight));
	// 書き込むためのアドレスを取得
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));
	// ポイントライトの初期位置
	pointLightData->position = { 0.0f, 10.0f, 0.0f };
	// ポイントライトの色
	pointLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// ポイントライトの輝度
	pointLightData->intensity = 1.0f;
	// ポイントライトの有効範囲
	pointLightData->radius = 10.0f;
	// ポイントライトの減衰率
	pointLightData->decay = 1.0f;
}

void Object3D::SpotLightSource(DirectXCommon* dxCommon)
{
	// スポットライト用のリソースを作る
	spotLightResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(SpotLight));
	// 書き込むためのアドレスを取得
	spotLightResource->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData));
	// スポットライトの色
	spotLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	// スポットライトの位置
	spotLightData->position = { 2.0f,2.0f,0.0f };
	// スポットライトの距離
	spotLightData->distance = 7.0f;
	// スポットライトの方向
	spotLightData->direction = Normalize({ -1.0f, -1.0f,0.0f });
	// スポットライトの輝度
	spotLightData->intensity = 4.0f;
	// スポットライトの減衰率
	spotLightData->decay = 2.0f;
	// スポットライトの開始角度の余弦値
	spotLightData->cosFalloffStart = std::cos(std::numbers::pi_v<float> / 6.0f);
	// スポットライトの余弦
	spotLightData->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLightResource->Unmap(0, nullptr);
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

	// アンマップ
	vertexResource->Unmap(0, nullptr);
}
