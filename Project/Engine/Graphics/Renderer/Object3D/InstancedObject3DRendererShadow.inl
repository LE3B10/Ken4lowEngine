#pragma once

#include "DirectXCommon.h"
#include "LightManager.h"
#include "Model.h"
#include "Object3DCommon.h"
#include "SRVManager.h"

#include <algorithm>

namespace Ken4lowEngine
{
	inline void InstancedObject3DRenderer::DrawShadow()
	{
		if (!initialized_ || !model_ || sourceInstances_.empty() || !mappedInstances_ || !perViewData_)
		{
			return;
		}

		const size_t shadowInstanceCount = sourceInstances_.size();
		const uint64_t shadowIndexCount = model_->GetTotalIndexCount() * static_cast<uint64_t>(shadowInstanceCount);
		if (debugIndexBudget_ > 0 && shadowIndexCount > debugIndexBudget_)
		{
			drawSkippedByBudget_ = true;
			return; // Shadow Passでも極端なDraw量によるTDRを防ぐ。
		}

		std::copy(sourceInstances_.begin(), sourceInstances_.end(), mappedInstances_);
		instanceBufferDirty_ = true; // 通常描画ではカメラFrustumに合わせて再度可視Instanceを詰め直す。
		perViewData_->viewProjection = LightManager::GetInstance()->GetActiveShadowPassLightViewProjection();

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		Object3DCommon::GetInstance()->SetInstancedShadowMapRenderSetting();
		commandList->SetGraphicsRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, instanceSrvIndex_);

		auto& meshes = model_->GetMeshes();
		for (auto& mesh : meshes)
		{
			mesh.DrawInstanced(static_cast<UINT>(shadowInstanceCount));
		}
	}
}
