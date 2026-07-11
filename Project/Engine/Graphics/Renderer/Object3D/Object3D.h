#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "TextureManager.h"
#include "Material.h"
#include "VertexData.h"
#include "Camera.h"
#include "TransformationMatrix.h"
#include "Engine/Graphics/Culling/BoundingVolume.h"
#include "Model.h"
#include "ObjectIdPipeline.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <numbers>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;
	class Object3DCommon;
	class SkyBox;

	/// -------------------------------------------------------------
	///						オブジェクト3Dクラス
	/// -------------------------------------------------------------
	class Object3D
	{
	public: /// ---------- 構造体 ---------- ///

		// シェーダー側のカメラ構造体
		struct CameraForGPU
		{
			Vector3 worldPosition;
		};

		// ディゾルブの設定
		struct DissolveSetting
		{
			float threshold;        // 閾値
			float edgeThickness;    // エッジの太さ
			float padding0[2];      // パディング
			Vector4 edgeColor;      // 色
		};

		struct ShadowParameterForGPU
		{
			Matrix4x4 lightViewProjection; // ライトのビュー射影行列
			float shadowBias;              // シャドウバイアス
			float normalBias;              // 法線方向オフセット量
			float shadowStrength;          // 影の濃さ（DirectLight のみへ適用）
			uint32_t shadowMode;           // 0:Off 1:Directional 2:Spot 3:PointCube 4:CSM
			uint32_t shadowDebugMode;      // 0:None 1:ShadowMap 2:ShadowFactor
			float padding[1];              // パディング
		};

	public: /// ---------- メンバ関数 ---------- ///

		void Initialize(const std::string& fileName);
		void Update();
		void UpdateWithWorldMatrix(const Matrix4x4& worldMatrix);
		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection);
		void DrawImGui();
		void Draw();
		void DrawMeshes(const std::vector<size_t>& meshIndices);
		void DrawShadow();

		void DrawEditorObjectId(uint32_t objectId)
		{
			if (!dxCommon_ || !model_ || objectId == 0)
			{
				return;
			}

			ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
			ObjectIdPipeline::GetInstance()->BindStatic(commandList, objectId);
			worldTransform_.SetPipeline(0);
			for (auto& mesh : model_->GetMeshes())
			{
				mesh.Draw(); // 通常描画のMaterialを使わず、形状だけをR32_UINT Object-ID Targetへ描く。
			}
		}

	public: /// ---------- 設定処理 ---------- ///

		void SetModel(const std::string& filePath);
		void SetScale(const Vector3& scale) { worldTransform_.scale_ = scale; }
		Vector3 GetScale() const { return worldTransform_.scale_; }
		void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }
		Vector3 GetRotate() const { return worldTransform_.rotate_; }
		void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }
		Vector3 GetTranslate() const { return worldTransform_.translate_; }
		void SetColor(const Vector4& color) { material_.SetColor(color); }
		void SetCamera(Camera* camera) { camera_ = camera; }
		void SetReflectivity(float reflectivity) { material_.SetReflection(reflectivity); }
		/// <summary>解決済みMaterialDescを定数バッファと5つのMaterial Texture Slotへ反映します。</summary>
		void ApplyMaterialDesc(const MaterialDesc& desc);
		/// <summary>Material Bindingを解除し、モデル読み込み時のMaterial状態へ戻します。</summary>
		void ResetMaterialBinding();
		void SetTextureForAll(const std::string& texturePath);
		void SetTextureForSubmesh(size_t index, const std::string& texturePath);
		size_t GetSubmeshCount() const;
		BoundingSphere GetWorldBoundsForCulling() const { return GetWorldBounds(); }
		BoundingSphere GetMeshWorldBoundsForCulling(size_t meshIndex) const { return GetMeshWorldBounds(meshIndex); }
		bool HasMeshWorldBoundsForCulling(size_t meshIndex) const { return HasMeshWorldBounds(meshIndex); }
		void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled_ = enabled; }
		bool IsFrustumCullingEnabled() const { return frustumCullingEnabled_; }
		void SetStageObjectCullingUnit(bool enabled) { isStageObjectCullingUnit_ = enabled; }
		bool IsStageObjectCullingUnit() const { return isStageObjectCullingUnit_; }
		void SetIgnoreStageChunkCulling(bool ignore) { ignoreStageChunkCulling_ = ignore; }
		bool IsIgnoreStageChunkCulling() const { return ignoreStageChunkCulling_; }
		bool HasWorldBoundsForCulling() const { return HasWorldBounds(); }

	public: /// ---------- ディゾルブの設定 ---------- ///

		void SetDissolveThreshold(float threshold) { dissolveSetting_->threshold = threshold; }
		void SetDissolveEdgeThickness(float thickness) { dissolveSetting_->edgeThickness = thickness; }
		void SetDissolveEdgeColor(const Vector4& color) { dissolveSetting_->edgeColor = color; }

	private: /// ---------- 内部メンバ関数 ---------- ///

		void InitializeCameraResource();
		void InitializeDissolveResource();
		void InitializeShadowResource();
		void InitializeShadowParameterResource();
		void AcquireShadowMapHandle();
		BoundingSphere GetWorldBounds() const;
		BoundingSphere GetMeshWorldBounds(size_t meshIndex) const;
		BoundingSphere TransformLocalBounds(const BoundingSphere& localBounds) const;
		bool HasWorldBounds() const;
		bool HasMeshWorldBounds(size_t meshIndex) const;
		void DrawInternal(const std::vector<size_t>* meshIndices);
		void DrawBoundsDebug(const BoundingSphere& bounds, bool visible) const;

	private: /// ---------- メンバ変数 ---------- ///

		DirectXCommon* dxCommon_ = nullptr;
		Camera* camera_ = nullptr;
		SkyBox* skyBox_ = nullptr;

		std::shared_ptr<Model> model_;

		// マテリアルデータ
		Material material_;
		MaterialTextureSlots materialTextureSlots_{}; // PBRの5 Texture Slotを全SubMeshへ共通Bindingする。

		// ワールドトランスフォーム
		WorldTransform worldTransform_;

		// 影用のワールドトランスフォーム
		WorldTransform shadowWorldTransform_;

		// バッファリソースの作成
		ComPtr <ID3D12Resource> cameraResource;

		// カメラにデータを書き込む
		CameraForGPU* cameraData = nullptr;

		ComPtr<ID3D12Resource> shadowTransformResource_;
		TransformationMatrix* shadowTransformData_ = nullptr;

		float alpha = 1.0f; // α値

		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_; // サブメッシュごとのテクスチャ SRV ハンドルを保存するベクター
		std::vector<bool> materialUsePointSampling_;

		// 環境マップのテクスチャ
		D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};

		// ディゾルブマスクのテクスチャ
		D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskHandle_{};

		// ディゾルブの設定
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
		DissolveSetting* dissolveSetting_ = nullptr;

		// シャドウマップ用の定数バッファ
		ComPtr<ID3D12Resource> shadowParameterResource_;
		ShadowParameterForGPU* shadowParameterData_ = nullptr;

		// ShadowMap の SRV を引くための GPU ハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE shadowMapHandle_{};

		bool frustumCullingEnabled_ = true;
		bool isStageObjectCullingUnit_ = false;
		bool ignoreStageChunkCulling_ = false;
	};

} // namespace Ken4lowEngine
