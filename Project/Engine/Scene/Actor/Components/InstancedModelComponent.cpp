#define NOMINMAX
#include "InstancedModelComponent.h"
#include "AssetPathSelector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>

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

		/// <summary>
		/// JSONからVector3を読み取る
		/// </summary>
		Vector3 ReadVector3FromJson(const nlohmann::json& json, const char* key, const Vector3& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3)
			{
				return defaultValue; // 配列が存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>()
			};
		}
	}

	void InstancedModelComponent::Initialize()
	{
		SceneComponent::Initialize(); // 親子関係を考慮したWorldTransformを初期計算する。
		hasInitialized_ = true;

		RebuildRenderer();
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
		std::array<char, 256> modelPathBuffer{};
		std::snprintf(modelPathBuffer.data(), modelPathBuffer.size(), "%s", modelPath_.c_str());
		if (ImGui::InputText("モデルパス", modelPathBuffer.data(), modelPathBuffer.size()))
		{
			SetModelPath(modelPathBuffer.data());
		}

		std::string selectedModelPath = modelPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##InstancedModelComponentModelPath", selectedModelPath, AssetType::Model))
		{
			SetModelPath(selectedModelPath);
		}

		ImGui::Text("Renderer: %s", renderer_ ? "Created" : "None");
		ImGui::Text("Model: %s", rendererStatus_.c_str());

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
		rendererStatus_ = modelPath_.empty() ? "Empty" : "Finalized";
		hasInitialized_ = false;
	}

	void InstancedModelComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // 親クラスの共通情報を保存する。

		outJson["Class"] = GetClassTypeName(); // InstancedModelComponentとして保存する

		outJson["ModelPath"] = modelPath_;			// 使用しているモデルパスを保存する
		outJson["InstanceCount"] = instanceCount_;	// 描画するインスタンス数を保存する
		outJson["Spacing"] = spacing_;				// インスタンス同士の間隔を保存する
		outJson["InstanceScale"] = { instanceScale_.x, instanceScale_.y, instanceScale_.z };
	}

	void InstancedModelComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // 親クラスの共通情報を復元する。

		if (inJson.contains("ModelPath") && inJson["ModelPath"].is_string())
		{
			SetModelPath(inJson["ModelPath"].get<std::string>());
		}

		if (inJson.contains("InstanceCount") && inJson["InstanceCount"].is_number_integer())
		{
			SetInstanceCount(inJson["InstanceCount"].get<int>()); // インスタンス数を復元する
		}

		if (inJson.contains("Spacing") && inJson["Spacing"].is_number())
		{
			SetSpacing(inJson["Spacing"].get<float>()); // インスタンス間隔を復元する
		}

		if (inJson.contains("InstanceScale") && inJson["InstanceScale"].is_array())
		{
			SetInstanceScale(ReadVector3FromJson(inJson, "InstanceScale", { 1.0f, 1.0f, 1.0f })); // インスタンスごとのスケールを復元する
		}

		RequestRebuild(); // JSON復元後の設定でインスタンス配置を再構築する
	}

	void InstancedModelComponent::SetModelPath(std::string_view modelPath)
	{
		const std::string newModelPath(modelPath);
		if (modelPath_ == newModelPath)
		{
			if (!renderer_ && hasInitialized_)
			{
				RebuildRenderer();
				RequestRebuild();
			}
			return; // 同じモデルパスなら再生成しない
		}

		modelPath_ = newModelPath; // string_viewは保持せず、内部で所有する。
		if (renderer_)
		{
			renderer_->Finalize();
			renderer_.reset();
		}

		isInitializedRenderer_ = false;
		RebuildRenderer();
		RequestRebuild();
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

	bool InstancedModelComponent::RebuildRenderer()
	{
		if (renderer_)
		{
			renderer_->Finalize();
			renderer_.reset();
		}

		isInitializedRenderer_ = false;

		if (modelPath_.empty())
		{
			rendererStatus_ = "Empty";
			return false;
		}

		if (!hasInitialized_)
		{
			rendererStatus_ = "Waiting Initialize";
			return false;
		}

		try
		{
			renderer_ = std::make_unique<InstancedObject3DRenderer>();
			renderer_->Initialize(modelPath_, static_cast<size_t>(std::max(instanceCount_, 1)));
			renderer_->SetDebugIndexBudget(50'000'000ull);
			isInitializedRenderer_ = true;
			rendererStatus_ = "Loaded";
			RebuildInstances();
			return true;
		}
		catch (const std::exception& e)
		{
			renderer_.reset();
			rendererStatus_ = std::string("Failed: ") + e.what();
		}
		catch (...)
		{
			renderer_.reset();
			rendererStatus_ = "Failed";
		}

		isInitializedRenderer_ = false;
		return false;
	}

	void InstancedModelComponent::RequestRebuild()
	{
		isRebuildRequested_ = true; // 次回Updateで安全に配置を再構築する。
	}
}
