#pragma once
#include "DX12Include.h"
#include "LightManager.h"
#include "Camera.h"
#include "Object3DPipelineSet.h"

namespace Ken4lowEngine
{
	class DirectXCommon;
	class PipelineFactory;

	class Object3DCommon
	{
	public:
		static Object3DCommon* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();
		void DrawImGui();

	public: /// ---------- 描画設定関数 ---------- ///

		void SetRenderSetting();
		void SetShadowMapRenderSetting();

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
	};
}