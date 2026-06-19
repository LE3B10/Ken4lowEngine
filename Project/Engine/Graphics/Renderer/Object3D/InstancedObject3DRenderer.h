#pragma once
#include "DX12Include.h"
#include "Material.h"
#include "Matrix4x4.h"
#include "Vector4.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class Model;

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

		InstancedObject3DRenderer() = default;
		~InstancedObject3DRenderer();
		InstancedObject3DRenderer(const InstancedObject3DRenderer&) = delete;
		InstancedObject3DRenderer& operator=(const InstancedObject3DRenderer&) = delete;

		/// <summary>共有Modelと、最大インスタンス数分のStructuredBufferを初期化します。</summary>
		void Initialize(const std::string& modelPath, size_t maxInstanceCount = 30000);
		void Finalize();

		/// <summary>描画する静的インスタンスをGPU可視バッファへ一括転送します。</summary>
		bool SetInstances(const std::vector<InstanceData>& instances);

		/// <summary>World行列列から逆転置行列を生成して一括登録します。</summary>
		bool SetWorldMatrices(const std::vector<Matrix4x4>& worldMatrices, const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

		/// <summary>サブメッシュごとに1回、DrawIndexedInstancedを発行します。</summary>
		void Draw();

		size_t GetInstanceCount() const { return instanceCount_; }
		size_t GetMaxInstanceCount() const { return maxInstanceCount_; }
		void SetMaterialColor(const Vector4& color) { material_.SetColor(color); }

	private:
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
	};
}
