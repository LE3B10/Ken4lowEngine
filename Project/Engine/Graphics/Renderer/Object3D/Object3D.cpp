#include "Object3D.h"
#include "Object3DCommon.h"
#include "ImGuiManager.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"

#include <Model.h>
#include "ModelManager.h"

#include "CameraManager.h"
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
		camera_ = CameraManager::GetInstance()->GetMainCamera();

		SetModel(fileName);

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
		material_.Update();
		worldTransform_.Update();

		// カメラ用バッファ更新
		cameraData->worldPosition = CameraManager::GetInstance()->GetActiveCameraPosition();

		// 平行光源の向きからシャドウ行列を毎フレーム作り直す
		Vector3 lightDir = { 0.3f, -1.0f, 0.2f }; // fallback

		const auto& lights = LightManager::GetInstance()->GetPunctualLights();
		for (const auto& light : lights)
		{
			if (light.lightType == 1)
			{
				lightDir = Vector3::Normalize(light.direction);
				break;
			}
		}

		// まずはカメラ中心で十分
		const Vector3 focusPos = cameraData->worldPosition;

		const Matrix4x4 lightViewProjection = Matrix4x4::MakeLightViewProjection(
			lightDir,
			focusPos,
			60.0f,   // distanceFromTarget
			35.0f,   // orthoHalfWidth
			35.0f,   // orthoHalfHeight
			0.1f,    // nearZ
			120.0f   // farZ
		);

		UpdateShadowMatrix(lightViewProjection);
	}

	void Object3D::UpdateWithWorldMatrix(const Matrix4x4& worldMatrix)
	{
		material_.Update();
		worldTransform_.UpdateWithWorldMatrix(worldMatrix);
		cameraData->worldPosition = CameraManager::GetInstance()->GetActiveCameraPosition();

		Vector3 lightDir = { 0.3f, -1.0f, 0.2f };
		const auto& lights = LightManager::GetInstance()->GetPunctualLights();
		for (const auto& light : lights)
		{
			if (light.lightType == 1)
			{
				lightDir = Vector3::Normalize(light.direction);
				break;
			}
		}

		const Vector3 focusPos = cameraData->worldPosition;
		const Matrix4x4 lightViewProjection = Matrix4x4::MakeLightViewProjection(
			lightDir,
			focusPos,
			60.0f,
			35.0f,
			35.0f,
			0.1f,
			120.0f);

		UpdateShadowMatrix(lightViewProjection);
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
		shadowParameterData_->shadowBias = 0.00045f;
		shadowParameterData_->normalBias = 0.025f;
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
		if (!model_) { return; }

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
		auto& meshes = model_->GetMeshes();
		for (size_t i = 0; i < meshes.size(); i++)
		{
			if (i < materialSRVs_.size())
			{
				TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, materialSRVs_[i]);
			}
			meshes[i].Draw();
		}
	}

	void Object3D::DrawShadow()
	{
		if (!model_) { return; }

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

		Object3DCommon::GetInstance()->SetShadowMapRenderSetting();

		commandList->SetGraphicsRootConstantBufferView(0, shadowTransformResource_->GetGPUVirtualAddress());

		auto& meshes = model_->GetMeshes();
		for (auto& mesh : meshes)
		{
			mesh.Draw();
		}
	}

	/// -------------------------------------------------------------
	///					　モデルの設定
	/// -------------------------------------------------------------
	void Object3D::SetModel(const std::string& filePath)
	{
		model_ = ModelManager::GetInstance()->LoadModel(filePath);

		materialSRVs_.clear();
		if (model_)
		{
			materialSRVs_ = model_->GetMaterialSRVs(); // 共有モデルから初期値をコピー
		}
	}

	/// -------------------------------------------------------------
	///					テクスチャの設定
	/// -------------------------------------------------------------
	void Object3D::SetTextureForAll(const std::string& texturePath)
	{
		TextureManager::GetInstance()->LoadTexture(texturePath);
		auto h = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);

		for (auto& srv : materialSRVs_)
		{
			srv = h;
		}
	}

	/// -------------------------------------------------------------
	///				指定サブメッシュのテクスチャ設定
	/// -------------------------------------------------------------
	void Object3D::SetTextureForSubmesh(size_t index, const std::string& texturePath)
	{
		if (index >= materialSRVs_.size()) return;

		TextureManager::GetInstance()->LoadTexture(texturePath);
		materialSRVs_[index] = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);
	}

	size_t Object3D::GetSubmeshCount() const
	{
		return model_ ? model_->GetMeshes().size() : 0;
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
