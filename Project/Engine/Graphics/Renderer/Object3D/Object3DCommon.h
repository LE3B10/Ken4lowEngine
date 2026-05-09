#pragma once
#include "DX12Include.h"
#include "LightManager.h"
#include "Camera.h"
#include "Object3DPipelineSet.h"
#include "Engine/Graphics/Culling/Frustum.h"

namespace Ken4lowEngine
{
	class DirectXCommon;
	class PipelineFactory;

	class Object3DCommon
	{
	public:
		enum class CullingCameraMode
		{
			ActiveCamera,
			MainCamera,
			DebugCamera
		};

	public:
		static Object3DCommon* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();
		void BeginObject3DPass();
		void DrawImGui();

	public: /// ---------- 描画設定関数 ---------- ///

		void SetRenderSetting();
		void SetShadowMapRenderSetting();
		bool ShouldDrawObject(const BoundingSphere& worldBounds, bool objectCullingEnabled);
		void SetCullingCameraMode(CullingCameraMode mode) { cullingCameraMode_ = mode; }
		CullingCameraMode GetCullingCameraMode() const { return cullingCameraMode_; }
		void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled_ = enabled; }
		bool IsFrustumCullingEnabled() const { return frustumCullingEnabled_; }
		int GetDrawnObjectCount() const { return drawnObjectCount_; }
		int GetCulledObjectCount() const { return culledObjectCount_; }
		int GetTotalObjectCount() const { return totalObjectCount_; }
		Matrix4x4 GetCullingViewProjectionMatrix() const;

	public: /// ---------- 拡張予定の関数 ---------- ///

		/*const PipelineBundle& GetForward() const;
		const PipelineBundle& GetDeferred() const;
		const PipelineBundle& GetShadow() const;
		const PipelineBundle& GetAlphaClipped() const;*/

	private:

		Object3DCommon() = default;
		~Object3DCommon() = default;
		Object3DCommon(const Object3DCommon&) = delete;
		Object3DCommon& operator=(const Object3DCommon&) = delete;

	private:
		DirectXCommon* dxCommon_ = nullptr;

		Object3DPipelineSet pipelineSet_{};

		Frustum activeFrustum_{};
		CullingCameraMode cullingCameraMode_ = CullingCameraMode::ActiveCamera;
		bool frustumCullingEnabled_ = false;
		int drawnObjectCount_ = 0;
		int culledObjectCount_ = 0;
		int totalObjectCount_ = 0;
	};
}