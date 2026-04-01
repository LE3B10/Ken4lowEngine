#include "Object3D.h"
#include "ImGuiManager.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"

#include "ModelManager.h"

#include "Object3DCommon.h"
#include "AssimpLoader.h"
#include "ParameterManager.h"
#include "SkyBox.h"

namespace Ken4lowEngine
{


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
		static const std::string kDefaultTexturePath = "Effects/white.dds";

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

		// ディゾルブマスクテクスチャの読み込み
		TextureManager::GetInstance()->LoadTexture("Effects/Masks/noise.dds");
		dissolveMaskHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("Effects/Masks/noise.dds");

		// ワールドトランスフォームの初期化
		worldTransform_.Initialize();

		// マテリアルデータの初期化処理
		material_.Initialize();

		// カメラデータの初期化処理
		InitializeCameraResource();

		// ディゾルブ用リソースの初期化
		InitializeDissolveResource();

		// シャドウマップ用リソースの初期化
		InitializeShadowResource();

		// 通常描画で使う ShadowParameter 用CBV
		InitializeShadowParameterResource();

		// ShadowMap の SRV ハンドルを取得
		AcquireShadowMapHandle();
	}


	/// -------------------------------------------------------------
	///					　		更新処理
	/// -------------------------------------------------------------
	void Object3D::Update()
	{
		// 描画に使うカメラを明示的に毎フレームセット
		camera_ = Object3DCommon::GetInstance()->GetDefaultCamera(); // ←ここが重要！

		material_.Update();
		worldTransform_.Update();

		// カメラ用バッファ更新（必要であれば）
		cameraData->worldPosition = Object3DCommon::GetInstance()->GetActiveCameraPosition();
	}

	void Object3D::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		const Matrix4x4& worldMatrix = worldTransform_.matWorld_;

		// シャドウマップ用の行列を計算して定数バッファにセット
		shadowTransformData_->World = worldMatrix;
		shadowTransformData_->WVP = Matrix4x4::Multiply(worldMatrix, lightViewProjection);
		shadowTransformData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));

		// シャドウマップ用の行列を GPU に転送
		shadowParameterData_->lightViewProjection = lightViewProjection;
		shadowParameterData_->shadowBias = 0.0015f;
		shadowParameterData_->normalBias = 0.01f;
	}

	/// -------------------------------------------------------------
	///					　		ImGuiの描画
	/// -------------------------------------------------------------
	void Object3D::DrawImGui()
	{
#ifdef USE_IMGUI
		// ① IDスコープで衝突を防ぐ（this を使うのが簡単）
		ImGui::PushID(this);

		// ② 見やすいようにヘッダーでグループ化（任意）
		if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Position##pos", &worldTransform_.translate_.x, 0.01f);
			ImGui::DragFloat3("Rotation##rot", &worldTransform_.rotate_.x, 0.01f);
			ImGui::DragFloat3("Scale##scl", &worldTransform_.scale_.x, 0.01f);

			// カメラ（必要ならスコープを分ける）
			if (ImGui::CollapsingHeader("Camera Settings"))
			{
				Vector3 tmp = camera_->GetTranslate();
				if (ImGui::SliderFloat3("Camera Position##cam", &tmp.x, -20.0f, 20.0f))
				{
					camera_->SetTranslate(tmp);              // CBではなくカメラを更新
				}
			}

			material_.DrawImGui(); // マテリアル側も PushID していなければ内部で同様に対応
		}

		ImGui::PopID();
#endif // USE_IMGUI
	}


	/// -------------------------------------------------------------
	///					　		描画処理
	/// -------------------------------------------------------------
	void Object3D::Draw()
	{
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

		Object3DCommon::GetInstance()->SetRenderSetting();

		material_.SetPipeline();
		worldTransform_.SetPipeline();

		commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_);

		commandList->SetGraphicsRootConstantBufferView(7, constantBuffer_->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 8, dissolveMaskHandle_); // t3

		commandList->SetGraphicsRootConstantBufferView(9, shadowParameterResource_->GetGPUVirtualAddress()); // シャドウマップ用行列
		commandList->SetGraphicsRootDescriptorTable(10, shadowMapHandle_); // シャドウマップのSRV

		// サブメッシュ事にテクスチャを差し替えて描画
		for (size_t i = 0; i < meshes_.size(); i++)
		{
			TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, materialSRVs_[i]);
			meshes_[i].Draw();
		}
	}

	void Object3D::DrawShadow()
	{
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

		Object3DCommon::GetInstance()->SetShadowMapRenderSetting();

		commandList->SetGraphicsRootConstantBufferView(0, shadowTransformResource_->GetGPUVirtualAddress());

		for (size_t i = 0; i < meshes_.size(); i++)
		{
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
		if (model_)
		{
			model_->Initialize(filePath);
		}
	}

	/// -------------------------------------------------------------
	///					テクスチャの設定
	/// -------------------------------------------------------------
	void Object3D::SetTextureForAll(const std::string& texturePath)
	{
		TextureManager::GetInstance()->LoadTexture(texturePath);
		auto h = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);
		for (auto& srv : materialSRVs_) {
			srv = h;
		}
	}

	/// -------------------------------------------------------------
	///				指定サブメッシュのテクスチャ設定
	/// -------------------------------------------------------------
	void Object3D::SetTextureForSubmesh(size_t index, const std::string& texturePath)
	{
		if (index >= materialSRVs_.size()) { return; }
		TextureManager::GetInstance()->LoadTexture(texturePath);
		materialSRVs_[index] = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);
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

	/// -------------------------------------------------------------
	///					ディゾルブ用のリソース生成
	/// -------------------------------------------------------------
	void Object3D::InitializeDissolveResource()
	{
		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveSetting_));

		// ディゾルブの設定
		dissolveSetting_->threshold = 1.0f;
		dissolveSetting_->edgeThickness = 0.0f;
		dissolveSetting_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	void Object3D::InitializeShadowResource()
	{
		// 影用の行列バッファを作成
		shadowTransformResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));

		// データのマッピング
		shadowTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowTransformData_));

		// 初期化（単位行列）
		shadowTransformData_->World = Matrix4x4::MakeIdentity();
		shadowTransformData_->WVP = Matrix4x4::MakeIdentity();
		shadowTransformData_->WorldInversedTranspose = Matrix4x4::MakeIdentity();
	}

	void Object3D::InitializeShadowParameterResource()
	{
		shadowParameterResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ShadowParameterForGPU));

		// データのマッピング
		shadowParameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowParameterData_));

		// シャドウバイアスの初期値
		shadowParameterData_->lightViewProjection = Matrix4x4::MakeIdentity();
		shadowParameterData_->shadowBias = 0.015f;
		shadowParameterData_->normalBias = 0.02f;
	}

	void Object3D::AcquireShadowMapHandle()
	{
		shadowMapHandle_ = dxCommon_->GetShadowMapSrvHandleGPU();
	}

} // namespace Ken4lowEngine
