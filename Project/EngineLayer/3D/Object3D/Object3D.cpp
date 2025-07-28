#include "Object3D.h"
#include "ImGuiManager.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"

#include "ModelManager.h"
#include "Model.h"

#include "Object3DCommon.h"
#include "AssimpLoader.h"
#include "ParameterManager.h"
#include "SkyBox.h"


/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void Object3D::Initialize(const std::string& fileName)
{
	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = AssimpLoader::LoadModel("Resources", fileName);

	std::string texturePath = "Resources/" + modelData.material.textureFilePath;

	// 参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(texturePath);

	// 読み込んだテクスチャ番号を取得
	modelData.material.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);

	// 環境マップ
	TextureManager::GetInstance()->LoadTexture("Resources/rostock_laage_airport_4k.dds");
	// 環境マップのハンドルを取得
	environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("Resources/rostock_laage_airport_4k.dds");

	// ワールドトランスフォームの初期化
	worldTransform.Initialize();

	// マテリアルデータの初期化処理
	material_.Initialize();

	// 頂点データの初期化
	mesh_.Initialize(modelData.vertices, modelData.indices);

	// カメラデータの初期化処理
	InitializeCameraResource();
}


/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void Object3D::Update()
{
	// 描画に使うカメラを明示的に毎フレームセット
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera(); // ←ここが重要！

	material_.Update();
	worldTransform.Update();

	// カメラ用バッファ更新（必要であれば）
	cameraData->worldPosition = camera_->GetTranslate();
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

	material_.DrawImGui();
}


/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void Object3D::Draw()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

	material_.SetPipeline();
	worldTransform.SetPipeline();

	TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, modelData.material.gpuHandle);

	commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

	TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 7, environmentMapHandle_);

	mesh_.Draw();
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
		model_->Initialize(filePath);
	}
}


/// -------------------------------------------------------------
///					　		カメラ用のリソース生成
/// -------------------------------------------------------------
void Object3D::InitializeCameraResource()
{
	// カメラ用のリソースを作る
	cameraResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
	// 書き込むためのアドレスを取得
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// カメラの初期位置
	cameraData->worldPosition = camera_->GetTranslate();
}
