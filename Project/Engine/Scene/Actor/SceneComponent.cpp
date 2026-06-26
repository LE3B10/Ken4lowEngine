#include "SceneComponent.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void SceneComponent::Initialize()
	{
		// 初期TransformをWorldTransformへ反映する。
		UpdateWorldTransform();
	}

	void SceneComponent::Update([[maybe_unused]] float deltaTime)
	{
		// 毎フレーム親子関係を考慮したWorldTransformへ更新する。
		UpdateWorldTransform();
	}

	void SceneComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		// SceneComponentの親子TransformをDebugScene上で確認・編集できるようにする。
		ImGui::SeparatorText("Scene Component");
		ImGui::DragFloat3("Local Position", &localPosition_.x, 0.1f);
		ImGui::DragFloat3("Local Rotation", &localRotation_.x, 0.1f);
		ImGui::DragFloat3("Local Scale", &localScale_.x, 0.1f);

		ImGui::SeparatorText("World Transform");
		ImGui::Text("World Position : %.2f, %.2f, %.2f", worldPosition_.x, worldPosition_.y, worldPosition_.z);
		ImGui::Text("World Rotation : %.2f, %.2f, %.2f", worldRotation_.x, worldRotation_.y, worldRotation_.z);
		ImGui::Text("World Scale    : %.2f, %.2f, %.2f", worldScale_.x, worldScale_.y, worldScale_.z);

		ImGui::Text("Children Count : %zu", children_.size());
#endif // USE_IMGUI
	}

	void SceneComponent::AttachTo(SceneComponent* parent)
	{
		if (parent_ == parent)
		{
			return; // 既に同じ親に接続されている場合は何もしない。
		}

		if (parent == this)
		{
			return; // 自分自身を親にすると循環するため禁止する。
		}

		Detach();

		parent_ = parent;
		if (parent_)
		{
			parent_->children_.push_back(this); // 親側にも子として登録し、階層を双方向で追えるようにする。
		}

		UpdateWorldTransform();
	}

	void SceneComponent::Detach()
	{
		if (!parent_)
		{
			return; // 親が無い場合は切り離し不要。
		}

		parent_->RemoveChild(this);
		parent_ = nullptr; // 親参照を消してRoot扱いに戻す。

		UpdateWorldTransform();
	}

	void SceneComponent::UpdateWorldTransform()
	{
		if (parent_)
		{
			worldPosition_ = {
				parent_->worldPosition_.x + localPosition_.x,
				parent_->worldPosition_.y + localPosition_.y,
				parent_->worldPosition_.z + localPosition_.z
			};

			worldRotation_ = {
				parent_->worldRotation_.x + localRotation_.x,
				parent_->worldRotation_.y + localRotation_.y,
				parent_->worldRotation_.z + localRotation_.z
			};

			worldScale_ = {
				parent_->worldScale_.x * localScale_.x,
				parent_->worldScale_.y * localScale_.y,
				parent_->worldScale_.z * localScale_.z
			};
		}
		else
		{
			worldPosition_ = localPosition_; // 親が無い場合はLocal値をそのままWorld値として扱う。
			worldRotation_ = localRotation_;
			worldScale_ = localScale_;
		}

		for (SceneComponent* child : children_)
		{
			if (child)
			{
				child->UpdateWorldTransform(); // 親のWorld更新後に子のWorldも再計算する。
			}
		}
	}

	void SceneComponent::RemoveChild(SceneComponent* child)
	{
		std::erase(children_, child); // eraseで一致する子Component参照を取り除く。
	}
}