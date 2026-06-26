#define NOMINMAX
#include "InstancedModelComponent.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		bool IsDifferentVector3(const Vector3& v1, const Vector3& v2)
		{
			constexpr float kEpsilon = 0.0001f;

			// 小さな誤差で毎フレーム再構築されないようにする
			return
				std::abs(v1.x - v2.x) > kEpsilon ||
				std::abs(v1.y - v2.y) > kEpsilon ||
				std::abs(v1.z - v2.z) > kEpsilon;
		}
	}


	void InstancedModelComponent::Initialize()
	{
		SceneComponent::Initialize(); // 親子関係を考慮したWorldTransformを初期計算する。

		if (modelPath_.empty())
		{
			return; // モデルパス未設定の場合はRendererを作らない。
		}

		renderer_ = std::make_unique<InstancedObject3DRenderer>();
		renderer_->Initialize(modelPath_, static_cast<size_t>(std::max(instanceCount_, 1)));
		renderer_->SetDebugIndexBudget(50'000'000ull);

		isInitializedRenderer_ = true;
		RebuildInstances();
	}

	void InstancedModelComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime); // SceneComponent側でWorldTransformを更新する。

		const Vector3 currentWorldPosition = GetWorldPosition();
		const Vector3 currentWorldRotation = GetWorldRotation();
		const Vector3 currentWorldScale = GetWorldScale();

		if (!hasLastWorldTransform_ ||
			IsDifferentVector3(currentWorldPosition, lastWorldPosition_) ||
			IsDifferentVector3(currentWorldRotation, lastWorldRotation_) ||
			IsDifferentVector3(currentWorldScale, lastWorldScale_))
		{
			RequestRebuild(); // 親RootのTransform変更をGPUインスタンス配置へ反映する
			lastWorldPosition_ = currentWorldPosition;
			lastWorldRotation_ = currentWorldRotation;
			lastWorldScale_ = currentWorldScale;
			hasLastWorldTransform_ = true;
		}

		if (isRebuildRequested_)
		{
			RebuildInstances(); // ImGuiや外部設定で変更された配置をGPU用データへ反映する。
		}
	}

	void InstancedModelComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		const Vector3 currentWorldPosition = GetWorldPosition();
		const Vector3 currentWorldRotation = GetWorldRotation();
		const Vector3 currentWorldScale = GetWorldScale();

		if (!hasLastWorldTransform_ ||
			IsDifferentVector3(currentWorldPosition, lastWorldPosition_) ||
			IsDifferentVector3(currentWorldRotation, lastWorldRotation_) ||
			IsDifferentVector3(currentWorldScale, lastWorldScale_))
		{
			RequestRebuild(); // 親RootのTransform変更をGPUインスタンス配置へ反映する
			lastWorldPosition_ = currentWorldPosition;
			lastWorldRotation_ = currentWorldRotation;
			lastWorldScale_ = currentWorldScale;
			hasLastWorldTransform_ = true;
		}

		if (isRebuildRequested_)
		{
			RebuildInstances(); // ImGuiや外部設定で変更された配置をGPU用データへ反映する。
		}
	}

	void InstancedModelComponent::Draw()
	{
		if (!renderer_)
		{
			return; // Renderer未生成の場合は描画しない。
		}

		renderer_->Draw(); // GPUインスタンシングでまとめて描画する。
	}

	void InstancedModelComponent::DrawImGui()
	{
		SceneComponent::DrawImGui(); // Component全体の基準Transformを編集できるようにする。

#ifdef USE_IMGUI
		ImGui::SeparatorText("Instanced Model Component");
		ImGui::Text("Model Path: %s", modelPath_.empty() ? "None" : modelPath_.c_str());
		ImGui::Text("Renderer: %s", renderer_ ? "Created" : "None");

		if (ImGui::SliderInt("Instance Count", &instanceCount_, 1, 30000))
		{
			RequestRebuild(); // インスタンス数変更時は配置を作り直す。
		}

		if (ImGui::DragFloat("Spacing", &spacing_, 0.05f, 0.1f, 50.0f))
		{
			RequestRebuild(); // 間隔変更時は配置を作り直す。
		}

		if (ImGui::DragFloat3("Instance Scale", &instanceScale_.x, 0.05f, 0.01f, 100.0f))
		{
			RequestRebuild(); // スケール変更時は配置を作り直す。
		}

		if (renderer_)
		{
			ImGui::Text("Visible: %zu / Total: %zu / Capacity: %zu",
				renderer_->GetVisibleInstanceCount(),
				renderer_->GetInstanceCount(),
				renderer_->GetMaxInstanceCount());

			if (renderer_->WasDrawSkippedByBudget())
			{
				ImGui::Text("Draw skipped by index budget.");
			}
		}
#endif // USE_IMGUI
	}

	void InstancedModelComponent::Finalize()
	{
		if (renderer_)
		{
			renderer_->Finalize(); // GPUリソースを明示的に解放する。
			renderer_.reset();
		}

		isInitializedRenderer_ = false;
		isRebuildRequested_ = true;
	}

	void InstancedModelComponent::SetModelPath(std::string_view modelPath)
	{
		modelPath_ = std::string(modelPath); // string_viewは保持せず、内部で所有する。
	}

	void InstancedModelComponent::SetInstanceCount(int instanceCount)
	{
		instanceCount_ = std::clamp(instanceCount, 1, 30000); // 極端な数でGPU負荷が暴れないよう制限する。
		RequestRebuild();
	}

	void InstancedModelComponent::SetSpacing(float spacing)
	{
		spacing_ = std::max(spacing, 0.1f); // 0以下の間隔で重なりすぎないようにする。
		RequestRebuild();
	}

	void InstancedModelComponent::SetInstanceScale(const Vector3& scale)
	{
		instanceScale_ = scale; // 各インスタンスの基本スケールを更新する。
		RequestRebuild();
	}

	void InstancedModelComponent::RebuildInstances()
	{
		if (!renderer_ || !isInitializedRenderer_)
		{
			return; // Renderer未生成の場合は再構築できない。
		}

		const int count = std::clamp(instanceCount_, 1, 30000);
		const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));

		std::vector<InstancedObject3DRenderer::InstanceTransform> transforms;
		transforms.reserve(static_cast<size_t>(count));

		const Vector3 basePosition = GetWorldPosition();
		const Vector3 baseRotation = GetWorldRotation();
		const Vector3 baseScale = GetWorldScale();

		for (int i = 0; i < count; ++i)
		{
			const int x = i % columns;
			const int z = i / columns;

			InstancedObject3DRenderer::InstanceTransform transform{};
			transform.position = {
				basePosition.x + (static_cast<float>(x) - columns * 0.5f) * spacing_,
				basePosition.y,
				basePosition.z + (static_cast<float>(z) - columns * 0.5f) * spacing_
			};
			transform.rotation = baseRotation;
			transform.scale = {
				baseScale.x * instanceScale_.x,
				baseScale.y * instanceScale_.y,
				baseScale.z * instanceScale_.z
			};
			transform.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			transforms.push_back(transform);
		}

		renderer_->SetTransforms(transforms); // 構築したTransform配列をGPUインスタンスデータへ転送する。
		isRebuildRequested_ = false;
	}

	void InstancedModelComponent::RequestRebuild()
	{
		isRebuildRequested_ = true; // 次回Updateで安全に配置を再構築する。
	}
}