#include "RigidbodyComponent.h"
#include "Actor.h"
#include "SceneComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void RigidbodyComponent::Initialize()
	{
		rigidbody_ = std::make_unique<Rigidbody>();
		rigidbody_->SetBodyType(bodyType_);
		rigidbody_->SetMass(mass_);
		rigidbody_->SetUseGravity(useGravity_);
		rigidbody_->SetVelocity(velocity_);
		rigidbody_->SetSleepEnabled(sleepEnabled_); // Editor上でDebug操作するためSleep機能は無効化する
	}

	void RigidbodyComponent::Update([[maybe_unused]] float deltaTime)
	{
		if (!rigidbody_)
		{
			return; // Rigidbody未生成の場合は更新しない
		}

		velocity_ = rigidbody_->GetVelocity(); // Debug表示用に現在速度を保持する
	}

	void RigidbodyComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!rigidbody_)
		{
			return; // Rigidbody未生成の場合は更新しない
		}

		velocity_ = rigidbody_->GetVelocity(); // Debug表示用に現在速度を保持する
	}

	void RigidbodyComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("Rigidbody Component");

		const char* bodyTypeNames[] = { "Static", "Dynamic", "Kinematic" };
		int bodyTypeIndex = static_cast<int>(bodyType_);

		if (ImGui::Combo("Body Type", &bodyTypeIndex, bodyTypeNames, IM_ARRAYSIZE(bodyTypeNames)))
		{
			SetBodyType(static_cast<BodyType>(bodyTypeIndex)); // ImGuiで選択したBodyTypeをRigidbodyへ反映する
		}

		if (ImGui::DragFloat("Mass", &mass_, 0.05f, 0.01f, 1000.0f))
		{
			SetMass(mass_); // ImGuiで変更した質量をRigidbodyへ反映する
		}

		if (ImGui::Checkbox("Use Gravity", &useGravity_))
		{
			SetUseGravity(useGravity_); // ImGuiで変更した重力フラグをRigidbodyへ反映する
		}

		if (ImGui::DragFloat3("Velocity", &velocity_.x, 0.05f))
		{
			SetVelocity(velocity_); // ImGuiで変更した速度をRigidbodyへ反映する
		}

		if (rigidbody_ && ImGui::Button("Wake Up Rigidbody"))
		{
			rigidbody_->WakeUp(); // Debug操作でSleepから復帰させる
		}

		if (rigidbody_)
		{
			ImGui::Text("Sleeping: %s", rigidbody_->IsSleeping() ? "Yes" : "No");
			ImGui::Text("Grounded: %s", rigidbody_->IsGrounded() ? "Yes" : "No");
		}

#endif // USE_IMGUI
	}

	void RigidbodyComponent::Finalize()
	{
		rigidbody_.reset(); // Component破棄時にRigidbodyも破棄する
	}

	void RigidbodyComponent::SetBodyType(BodyType bodyType)
	{
		bodyType_ = bodyType; // ImGui表示用のBodyTypeも更新する

		if (rigidbody_)
		{
			rigidbody_->SetBodyType(bodyType_);
		}
	}

	void RigidbodyComponent::SetMass(float mass)
	{
		mass_ = mass; // ImGui表示用の質量も更新する

		if (rigidbody_)
		{
			rigidbody_->SetMass(mass_);
		}
	}

	void RigidbodyComponent::SetUseGravity(bool useGravity)
	{
		useGravity_ = useGravity; // ImGui表示用の重力フラグも更新する

		if (rigidbody_)
		{
			rigidbody_->SetUseGravity(useGravity_);
		}
	}

	void RigidbodyComponent::SetVelocity(const Vector3& velocity)
	{
		velocity_ = velocity; // ImGui表示用の速度も更新する

		if (rigidbody_)
		{
			rigidbody_->SetVelocity(velocity_);
			rigidbody_->WakeUp(); // 速度を設定した場合はSleep状態から復帰させる
		}
	}

	void RigidbodyComponent::AddForce(const Vector3& force)
	{
		if (rigidbody_)
		{
			rigidbody_->AddForce(force); // 外部から加えた力をRigidbodyへ蓄積する
		}
	}

	void RigidbodyComponent::WakeUp()
	{
		if (rigidbody_)
		{
			rigidbody_->WakeUp(); // RigidbodyをSleep状態から復帰させる
		}
	}

	void RigidbodyComponent::SetSleepEnabled(bool enabled)
	{
		sleepEnabled_ = enabled; // ImGui表示用のSleep機能有効状態も更新する

		if (rigidbody_)
		{
			rigidbody_->SetSleepEnabled(sleepEnabled_);
		}
	}

	SceneComponent* RigidbodyComponent::GetTargetRootComponent() const
	{
		const Actor* owner = GetOwner();
		if (!owner)
		{
			return nullptr; // 所有者Actorが無い場合はRootComponentを取得できない
		}

		return owner->GetRootComponent(); // 所有者ActorのRootComponentを返す
	}

}