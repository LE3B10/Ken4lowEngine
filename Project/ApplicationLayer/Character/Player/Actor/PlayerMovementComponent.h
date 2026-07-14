#pragma once

#include <Scene/Actor/Character/CharacterMovementComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの移動入力を速度へ変換し、共通CharacterMovementComponentへ移動反映を委譲するComponent。
	class PlayerMovementComponent final : public CharacterMovementComponent
	{
	public:
		/// 現在の移動入力からXZ速度を作り、共通移動処理へ渡す。
		void Update(float deltaTime) override
		{
			float x = moveInputX_;
			float z = moveInputZ_;
			const float lengthSq = x * x + z * z;
			if (lengthSq > 1.0f)
			{
				const float invLength = 1.0f / std::sqrt(lengthSq);
				x *= invLength;
				z *= invLength;
			}

			const Vector3 currentVelocity = GetVelocity();
			SetVelocity({ x * moveSpeed_, currentVelocity.y, z * moveSpeed_ }); // Player固有入力は速度へ変換し、Transform更新は共通Componentへ任せる。
			CharacterMovementComponent::Update(deltaTime);
		}

		/// 共通移動情報にPlayer用移動速度を追加表示する。
		void DrawImGui() override
		{
			CharacterMovementComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤー移動");
			ImGui::SliderFloat("移動速度", &moveSpeed_, 0.0f, 30.0f, "%.2f");
			ImGui::Text("入力: %.2f, %.2f", moveInputX_, moveInputZ_);
#endif
		}

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "PlayerMovementComponent"; }

		/// Player固有の移動速度をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override
		{
			CharacterMovementComponent::ToJson(outJson);
			outJson["MoveSpeed"] = moveSpeed_;
		}

		/// Actor JSONからPlayer固有の移動速度を復元する。
		void FromJson(const nlohmann::json& inJson) override
		{
			CharacterMovementComponent::FromJson(inJson);
			moveSpeed_ = inJson.value("MoveSpeed", moveSpeed_);
			if (!std::isfinite(moveSpeed_)) moveSpeed_ = 6.0f;
			moveSpeed_ = std::max(0.0f, moveSpeed_);
		}

		/// 入力Componentから受け取った移動要求を保持する。
		void SetMoveInput(float x, float z)
		{
			moveInputX_ = std::clamp(x, -1.0f, 1.0f);
			moveInputZ_ = std::clamp(z, -1.0f, 1.0f);
		}

		/// 移動要求と現在速度を停止状態へ戻す。
		void ResetMovement()
		{
			moveInputX_ = 0.0f;
			moveInputZ_ = 0.0f;
			Stop();
			SetMovementEnabled(true);
		}

		float GetMoveInputX() const { return moveInputX_; }
		float GetMoveInputZ() const { return moveInputZ_; }
		float GetMoveSpeed() const { return moveSpeed_; }

	private:
		float moveInputX_ = 0.0f;
		float moveInputZ_ = 0.0f;
		float moveSpeed_ = 6.0f;
	};
} // namespace Ken4lowEngine
