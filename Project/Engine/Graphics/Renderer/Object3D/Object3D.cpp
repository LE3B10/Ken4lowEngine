#define NOMINMAX
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
#include "Wireframe.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace Ken4lowEngine
{
	namespace
	{
		bool ShouldUsePointSamplingForTexture(const std::string& texturePath)
		{
			std::string lowered = texturePath;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lowered.find("face") != std::string::npos || lowered.find("pixel") != std::string::npos || lowered.find("dot") != std::string::npos;
		}
	}


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

		const auto* lightMgr = LightManager::GetInstance();
		const Vector3 focusPos = cameraData->worldPosition;
		const Matrix4x4 lightViewProjection = lightMgr->BuildShadowLightViewProjection(focusPos);
		UpdateShadowMatrix(lightViewProjection);
		shadowParameterData_->shadowBias = lightMgr->GetShadowBias();
		shadowParameterData_->normalBias = lightMgr->GetNormalBias();
		shadowParameterData_->shadowStrength = lightMgr->GetShadowStrength();
		const auto casterType = lightMgr->GetActiveShadowCasterType();
		shadowParameterData_->shadowMode = lightMgr->IsShadowEnabled() ? (casterType == LightManager::ShadowCasterType::Spot ? 2u : (casterType == LightManager::ShadowCasterType::Directional ? 1u : 0u)) : 0u;
		shadowParameterData_->shadowDebugMode = lightMgr->IsShadowMapDebugEnabled() ? 1u : (lightMgr->IsShadowFactorDebugEnabled() ? 2u : 0u);
	}

	void Object3D::UpdateWithWorldMatrix(const Matrix4x4& worldMatrix)
	{
		material_.Update();
		worldTransform_.UpdateWithWorldMatrix(worldMatrix);
		cameraData->worldPosition = CameraManager::GetInstance()->GetActiveCameraPosition();

		const auto* lightMgr = LightManager::GetInstance();
		const Vector3 focusPos = cameraData->worldPosition;
		const Matrix4x4 lightViewProjection = lightMgr->BuildShadowLightViewProjection(focusPos);
		UpdateShadowMatrix(lightViewProjection);
		shadowParameterData_->shadowBias = lightMgr->GetShadowBias();
		shadowParameterData_->normalBias = lightMgr->GetNormalBias();
		shadowParameterData_->shadowStrength = lightMgr->GetShadowStrength();
		const auto casterType = lightMgr->GetActiveShadowCasterType();
		shadowParameterData_->shadowMode = lightMgr->IsShadowEnabled() ? (casterType == LightManager::ShadowCasterType::Spot ? 2u : (casterType == LightManager::ShadowCasterType::Directional ? 1u : 0u)) : 0u;
		shadowParameterData_->shadowDebugMode = lightMgr->IsShadowMapDebugEnabled() ? 1u : (lightMgr->IsShadowFactorDebugEnabled() ? 2u : 0u);
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
			ImGui::Checkbox("Object Frustum Culling", &frustumCullingEnabled_);
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
		DrawInternal(nullptr);
	}

	void Object3D::DrawMeshes(const std::vector<size_t>& meshIndices)
	{
		DrawInternal(&meshIndices);
	}

	void Object3D::DrawInternal(const std::vector<size_t>* meshIndices)
	{
		if (!model_) { return; }

		Object3DCommon* object3DCommon = Object3DCommon::GetInstance();

		if (!meshIndices)
		{
			const BoundingSphere objectBounds = GetWorldBounds();

			// Object3D 全体が完全に視錐台外なら、更新系は止めず Draw だけをスキップする。
			if (!object3DCommon->ShouldDrawObject(objectBounds, frustumCullingEnabled_, HasWorldBounds(), isStageObjectCullingUnit_))
			{
				DrawBoundsDebug(objectBounds, false);
				return;
			}
			DrawBoundsDebug(objectBounds, true);
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

		object3DCommon->SetRenderSetting();

		material_.SetPipeline();
		worldTransform_.SetPipeline();

		commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_);

		commandList->SetGraphicsRootConstantBufferView(7, constantBuffer_->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 8, dissolveMaskHandle_); // t3

		commandList->SetGraphicsRootConstantBufferView(9, shadowParameterResource_->GetGPUVirtualAddress()); // シャドウマップ用行列
		commandList->SetGraphicsRootDescriptorTable(10, shadowMapHandle_); // シャドウマップのSRV

		// StageChunk で可視判定済みのメッシュは二重カリングで欠けないよう、Stage専用ObjectはMesh単位Frustum判定をスキップする。
		auto& meshes = model_->GetMeshes();
		const size_t drawCount = meshIndices ? meshIndices->size() : meshes.size();
		for (size_t drawIndex = 0; drawIndex < drawCount; ++drawIndex)
		{
			const size_t i = meshIndices ? (*meshIndices)[drawIndex] : drawIndex;
			if (i >= meshes.size())
			{
				continue;
			}

			const BoundingSphere meshBounds = GetMeshWorldBounds(i);
			const bool hasMeshBounds = HasMeshWorldBounds(i);
			const bool skipMeshFrustumForStageChunk = (meshIndices != nullptr) && isStageObjectCullingUnit_;
			const bool meshVisible = skipMeshFrustumForStageChunk
				? true
				: object3DCommon->ShouldDrawMesh(meshBounds, frustumCullingEnabled_, hasMeshBounds);
			DrawBoundsDebug(meshBounds, meshVisible);
			if (!meshVisible)
			{
				continue;
			}

			if (i < materialSRVs_.size())
			{
				TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, materialSRVs_[i]);
				material_.SetUsePointSampling(i < materialUsePointSampling_.size() ? materialUsePointSampling_[i] : false);
				material_.Update();
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
			materialUsePointSampling_ = model_->GetMaterialPointSamplingFlags();
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
		materialUsePointSampling_.assign(materialSRVs_.size(), ShouldUsePointSamplingForTexture(texturePath));
	}

	/// -------------------------------------------------------------
	///				指定サブメッシュのテクスチャ設定
	/// -------------------------------------------------------------
	void Object3D::SetTextureForSubmesh(size_t index, const std::string& texturePath)
	{
		if (index >= materialSRVs_.size()) return;

		TextureManager::GetInstance()->LoadTexture(texturePath);
		materialSRVs_[index] = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);
		if (index >= materialUsePointSampling_.size()) { materialUsePointSampling_.resize(materialSRVs_.size(), false); }
		materialUsePointSampling_[index] = ShouldUsePointSamplingForTexture(texturePath);
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
		shadowParameterData_->shadowStrength = 0.6f;
		shadowParameterData_->shadowMode = 0u;
		shadowParameterData_->shadowDebugMode = 0u;
	}

	BoundingSphere Object3D::GetWorldBounds() const
	{
		if (!HasWorldBounds())
		{
			return {};
		}

		return TransformLocalBounds(model_->GetLocalBounds());
	}

	BoundingSphere Object3D::GetMeshWorldBounds(size_t meshIndex) const
	{
		if (!HasMeshWorldBounds(meshIndex))
		{
			return {};
		}

		return TransformLocalBounds(model_->GetMeshLocalBounds(meshIndex));
	}

	BoundingSphere Object3D::TransformLocalBounds(const BoundingSphere& localBounds) const
	{
		BoundingSphere worldBounds{};
		worldBounds.center = Vector3::Transform(localBounds.center, worldTransform_.matWorld_);

		const float scaleX = std::sqrt(worldTransform_.matWorld_.m[0][0] * worldTransform_.matWorld_.m[0][0] + worldTransform_.matWorld_.m[0][1] * worldTransform_.matWorld_.m[0][1] + worldTransform_.matWorld_.m[0][2] * worldTransform_.matWorld_.m[0][2]);
		const float scaleY = std::sqrt(worldTransform_.matWorld_.m[1][0] * worldTransform_.matWorld_.m[1][0] + worldTransform_.matWorld_.m[1][1] * worldTransform_.matWorld_.m[1][1] + worldTransform_.matWorld_.m[1][2] * worldTransform_.matWorld_.m[1][2]);
		const float scaleZ = std::sqrt(worldTransform_.matWorld_.m[2][0] * worldTransform_.matWorld_.m[2][0] + worldTransform_.matWorld_.m[2][1] * worldTransform_.matWorld_.m[2][1] + worldTransform_.matWorld_.m[2][2] * worldTransform_.matWorld_.m[2][2]);
		const float maxScale = std::max({ scaleX, scaleY, scaleZ, 1.0f });
		worldBounds.radius = localBounds.radius * maxScale;
		return worldBounds;
	}

	bool Object3D::HasWorldBounds() const
	{
		return model_ && model_->HasLocalBounds();
	}

	bool Object3D::HasMeshWorldBounds(size_t meshIndex) const
	{
		return model_ && model_->HasMeshLocalBounds(meshIndex);
	}

	void Object3D::DrawBoundsDebug(const BoundingSphere& bounds, bool visible) const
	{
		if (!Object3DCommon::GetInstance()->IsBoundsDebugVisible() || bounds.radius <= 0.0f)
		{
			return;
		}

		const Vector4 color = visible ? Vector4{ 0.1f, 1.0f, 0.2f, 1.0f } : Vector4{ 1.0f, 0.15f, 0.1f, 1.0f };
		Wireframe::GetInstance()->DrawSphere(bounds.center, bounds.radius, color);
	}

	void Object3D::AcquireShadowMapHandle()
	{
		shadowMapHandle_ = dxCommon_->GetShadowMapSrvHandleGPU();
	}

} // namespace Ken4lowEngine
