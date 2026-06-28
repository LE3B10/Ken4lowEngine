#include "RigidbodyComponent.h"
#include "Actor.h"
#include "SceneComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// Rigidbodyの種類をJSON保存用文字列へ変換する。
		/// </summary>
		const char* ToString(BodyType bodyType)
		{
			switch (bodyType)
			{
			case BodyType::Static:
				return "Static";
			case BodyType::Dynamic:
				return "Dynamic";
			case BodyType::Kinematic:
				return "Kinematic";
			default:
				return "Unknown";
			}
		}

		BodyType BodyTypeFromString(const std::string& bodyType)
		{
			if (bodyType == "Static")
			{
				return BodyType::Static;
			}
			else if (bodyType == "Dynamic")
			{
				return BodyType::Dynamic;
			}
			else if (bodyType == "Kinematic")
			{
				return BodyType::Kinematic;
			}

			return BodyType::Dynamic; // デフォルトはDynamicにする
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

	void RigidbodyComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // 基底クラスの共通情報を保存する

		outJson["Class"] = GetClassTypeName(); // RigidbodyComponentとして保存する

		if (!rigidbody_)
		{
			return; // Rigidbody未生成の場合は保存しない
		}

		outJson["BodyType"] = ToString(bodyType_); // BodyTypeを文字列で保存する
		outJson["Mass"] = mass_;                   // 質量を保存する
		outJson["UseGravity"] = useGravity_;       // 重力フラグを保存する

		const Vector3 velocity = rigidbody_->GetVelocity(); // Rigidbodyから現在速度を取得する
		outJson["Velocity"] = { velocity.x, velocity.y, velocity.z }; // 速度を配列で保存する
	}

	void RigidbodyComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // 基底クラスの共通情報を復元する

		if (inJson.contains("BodyType") && inJson["BodyType"].is_string())
		{
			SetBodyType(BodyTypeFromString(inJson["BodyType"].get<std::string>())); // BodyTypeを復元する。
		}

		if (inJson.contains("Mass") && inJson["Mass"].is_number())
		{
			SetMass(inJson["Mass"].get<float>()); // 質量を復元する。
		}

		if (inJson.contains("UseGravity") && inJson["UseGravity"].is_boolean())
		{
			SetUseGravity(inJson["UseGravity"].get<bool>()); // 重力設定を復元する。
		}

		if (inJson.contains("Velocity") && inJson["Velocity"].is_array())
		{
			const Vector3 velocity = ReadVector3FromJson(inJson, "Velocity", Vector3{ 0.0f, 0.0f, 0.0f });
			SetVelocity(velocity); // Rigidbody生成前なら保持値に、生成後なら実体にも反映する。
		}
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