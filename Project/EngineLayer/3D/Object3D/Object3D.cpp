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
	modelData = AssimpLoader::LoadModel(fileName);

	// サブメッシュ配列に対応してメッシュ・テクスチャを用意

	// 既存データをクリア
	meshes_.clear();
	materialSRVs_.clear();

	// メッシュとテクスチャの数を予約
	meshes_.reserve(modelData.subMeshes.size());
	materialSRVs_.reserve(modelData.subMeshes.size());

	// テクスチャ未指定時のフォールバック
	static const std::string kDefaultTexturePath = "white.png";

	for (const auto& sub : modelData.subMeshes)
	{
		// テクスチャSRV
		std::string texturePath = sub.material.textureFilePath; // テクスチャパス
		if (texturePath.empty()) texturePath = kDefaultTexturePath; // フォールバック
		TextureManager::GetInstance()->LoadTexture(texturePath); // テクスチャ読み込み
		materialSRVs_.push_back(TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));

		// メッシュ（頂点インデックス）
		Mesh m = {};
		m.Initialize(sub.vertices, sub.indices);
		meshes_.push_back(std::move(m));
	}

	// 環境マップ
	TextureManager::GetInstance()->LoadTexture("SkyBox/skybox.dds");

	// 環境マップのハンドルを取得
	environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("SkyBox/skybox.dds");

	// ワールドトランスフォームの初期化
	worldTransform.Initialize();

	// マテリアルデータの初期化処理
	material_.Initialize();

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
	cameraData->worldPosition = Object3DCommon::GetInstance()->GetActiveCameraPosition();
}


/// -------------------------------------------------------------
///					　		ImGuiの描画
/// -------------------------------------------------------------
void Object3D::DrawImGui()
{
	// ① IDスコープで衝突を防ぐ（this を使うのが簡単）
	ImGui::PushID(this);

	// ② 見やすいようにヘッダーでグループ化（任意）
	if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Position##pos", &worldTransform.translate_.x, 0.01f);
		ImGui::DragFloat3("Rotation##rot", &worldTransform.rotate_.x, 0.01f);
		ImGui::DragFloat3("Scale##scl", &worldTransform.scale_.x, 0.01f);

		// カメラ（必要ならスコープを分ける）
		if (ImGui::CollapsingHeader("Camera Settings"))
		{
			Vector3 tmp = camera_->GetTranslate();
			if (ImGui::SliderFloat3("Camera Position##cam", &tmp.x, -20.0f, 20.0f))
			{
				camera_->SetTranslate(tmp);              // ★ CBではなくカメラを更新
			}
		}

		material_.DrawImGui(); // マテリアル側も PushID していなければ内部で同様に対応
	}

	ImGui::PopID();
}


/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void Object3D::Draw()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

	Object3DCommon::GetInstance()->SetRenderSetting();

	material_.SetPipeline();
	worldTransform.SetPipeline();

	commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

	TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_);

	// サブメッシュ事にテクスチャを差し替えて描画
	for (size_t i = 0; i < meshes_.size(); i++)
	{
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, materialSRVs_[i]);
		meshes_[i].Draw();
	}
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
