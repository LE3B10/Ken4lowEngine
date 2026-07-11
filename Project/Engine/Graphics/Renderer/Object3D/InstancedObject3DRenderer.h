#pragma once
#include "DX12Include.h"
#include "Material.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include "Vector3.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Model.h"
#include "ObjectIdPipeline.h"
#include "SRVManager.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// CPUでObject3Dを大量生成せず、同じModelをGPUインスタンシングでまとめて描画する専用レンダラーです。
	/// </summary>
	class InstancedObject3DRenderer
	{
	public:
		struct InstanceData
		{
			Matrix4x4 world;
			Matrix4x4 worldInverseTranspose;
			Vector4 color;
		};

		/// <summary>JSONなどの配置データへ移しやすい、インスタンス単位のTransformと色です。</summary>
		struct InstanceTransform
		{
			Vector3 position{};
			Vector3 rotation{};
			Vector3 scale{ 1.0f, 1.0f, 1.0f };
			Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		};

		InstancedObject3DRenderer() = default;
		~InstancedObject3DRenderer();
		InstancedObject3DRenderer(const InstancedObject3DRenderer&) = delete;
		InstancedObject3DRenderer& operator=(const InstancedObject3DRenderer&) = delete;

		/// <summary>共有Modelと、最大インスタンス数分のStructuredBufferを初期化します。</summary>
		void Initialize(const std::string& modelPath, size_t maxInstanceCount = 30000);
		void Finalize();

		/// <summary>モデルの総インデックス数を取得します。</summary>
		uint64_t GetModelTotalIndexCount() const;

		/// <summary>描画する静的インスタンスをGPU可視バッファへ一括転送します。</summary>
		bool SetInstances(const std::vector<InstanceData>& instances);

		/// <summary>World行列列から逆転置行列を生成して一括登録します。</summary>
		bool SetWorldMatrices(const std::vector<Matrix4x4>& worldMatrices, const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

		/// <summary>位置・回転・スケール・色からGPU用InstanceDataを構築します。</summary>
		bool SetTransforms(const std::vector<InstanceTransform>& transforms);

		/// <summary>解決済みMaterialDescを全インスタンス共通のMaterialへ反映します。</summary>
		void ApplyMaterialDesc(const MaterialDesc& desc);

		/// <summary>Material Bindingを解除し、モデル読み込み時のMaterial状態へ戻します。</summary>
		void ResetMaterialBinding();

		/// <summary>サブメッシュごとに1回、DrawIndexedInstancedを発行します。</summary>
		void Draw();

		/// <summary>全インスタンスを現在のShadow Sliceへまとめて描画します。</summary>
		void DrawShadow();

		/// <summary>全Instanceを同じComponent IDでR32_UINT Object-ID Targetへ描画します。</summary>
		void DrawEditorObjectId(uint32_t objectId)
		{
			if (!initialized_ || !dxCommon_ || !model_ || sourceInstances_.empty() || objectId == 0)
			{
				return;
			}

			const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
			perViewData_->viewProjection = viewProjection;
			UpdateVisibleInstances(viewProjection);
			if (instanceCount_ == 0)
			{
				return;
			}

			ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
			SRVManager::GetInstance()->PreDraw();
			ObjectIdPipeline::GetInstance()->BindInstanced(commandList, objectId);
			SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, instanceSrvIndex_);
			commandList->SetGraphicsRootConstantBufferView(1, perViewResource_->GetGPUVirtualAddress());
			for (auto& mesh : model_->GetMeshes())
			{
				mesh.DrawInstanced(static_cast<UINT>(instanceCount_)); // Instanceごとの行列を維持したままComponent IDだけを出力する。
			}
		}

	public: /// ---------- アクセサ ---------- ///

		size_t GetInstanceCount() const { return sourceInstances_.size(); }
		size_t GetVisibleInstanceCount() const { return instanceCount_; }
		size_t GetMaxInstanceCount() const { return maxInstanceCount_; }

		void SetDebugIndexBudget(uint64_t budget) { debugIndexBudget_ = budget; }
		uint64_t GetEstimatedDrawIndexCount() const { return estimatedDrawIndexCount_; }
		bool WasDrawSkippedByBudget() const { return drawSkippedByBudget_; }

		void SetMaterialColor(const Vector4& color) { material_.SetColor(color); }
		void SetFrustumCullingEnabled(bool enabled);
		bool IsFrustumCullingEnabled() const { return frustumCullingEnabled_; }

	private: /// ---------- メンバ変数 ---------- ///

		struct PerViewData { Matrix4x4 viewProjection; };
		struct CameraForGPU { float x, y, z, padding; };
		struct DissolveSetting
		{
			float threshold;
			float edgeThickness;
			float padding[2];
			Vector4 edgeColor;
		};
		struct ShadowParameterForGPU
		{
			Matrix4x4 lightViewProjection;
			float shadowBias;
			float normalBias;
			float shadowStrength;
			uint32_t shadowMode;
			uint32_t shadowDebugMode;
			float padding[1];
		};

		DirectXCommon* dxCommon_ = nullptr;
		std::shared_ptr<Model> model_;
		Material material_{};
		MaterialTextureSlots materialTextureSlots_{}; // 全Instance共通の5 Texture Slotを保持する。

		ComPtr<ID3D12Resource> instanceResource_;
		InstanceData* mappedInstances_ = nullptr;
		uint32_t instanceSrvIndex_ = UINT32_MAX;
		size_t maxInstanceCount_ = 0;
		size_t instanceCount_ = 0;

		ComPtr<ID3D12Resource> perViewResource_;
		PerViewData* perViewData_ = nullptr;
		ComPtr<ID3D12Resource> cameraResource_;
		CameraForGPU* cameraData_ = nullptr;
		ComPtr<ID3D12Resource> dissolveResource_;
		DissolveSetting* dissolveData_ = nullptr;
		ComPtr<ID3D12Resource> shadowParameterResource_;
		ShadowParameterForGPU* shadowParameterData_ = nullptr;

		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;
		std::vector<bool> materialUsePointSampling_;
		D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};
		D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskHandle_{};
		bool initialized_ = false;
		std::vector<InstanceData> sourceInstances_{};
		bool instanceBufferDirty_ = false;
		bool frustumCullingEnabled_ = false;
		uint64_t debugIndexBudget_ = 50'000'000ull;
		uint64_t estimatedDrawIndexCount_ = 0;
		bool drawSkippedByBudget_ = false;

		/// <summary>カリング設定に応じ、描画対象だけをGPUバッファの先頭へ詰め直します。</summary>
		void UpdateVisibleInstances(const Matrix4x4& viewProjection);

		/// <summary>共有Modelが保持するSubMesh TextureとSampler設定をRendererへ復元します。</summary>
		void RestoreModelMaterials();
	};
}

#include "InstancedObject3DRendererShadow.inl"
