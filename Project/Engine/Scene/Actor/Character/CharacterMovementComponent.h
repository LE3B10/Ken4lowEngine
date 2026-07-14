#pragma once

#include "ActorComponent.h"
#include "ComponentProperty.h"
#include "Vector3.h"

#include <vector>

namespace Ken4lowEngine
{
	/// CharacterActorから移動計算を分離し、Root Transformへ速度を反映する基本Movement Component。
	class CharacterMovementComponent : public ActorComponent
	{
	public:
		/// Play中だけ現在速度をRoot Transformへ反映する。
		void Update(float deltaTime) override;

		/// Character用移動設定をDetails上で編集・確認する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponentクラス名を返す。
		std::string GetClassTypeName() const override { return "CharacterMovementComponent"; }

		/// 移動設定をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから移動設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// Root Transformへ反映する移動速度を設定する。
		virtual void SetVelocity(const Vector3& velocity);

		/// 現在速度を0へ戻す。
		virtual void Stop() { velocity_ = {}; }

		/// Actorを持たない移行Adapterでも同じ速度積分を使えるよう、1フレームの移動量を返す。
		Vector3 CalculateDisplacement(float deltaTime) const;

		/// Character共通の+Z前方規約でRootを指定XZ方向へ滑らかに旋回させる。
		bool FaceDirectionXZ(const Vector3& direction, float rotateSpeed, float deltaTime);

		/// Root Transformへ反映する現在速度を返す。
		virtual const Vector3& GetVelocity() const { return velocity_; }

		/// Componentの移動反映を切り替える。
		void SetMovementEnabled(bool enabled) { movementEnabled_ = enabled; }

		/// Componentの移動反映が有効か返す。
		bool IsMovementEnabled() const { return movementEnabled_; }

	protected:
		/// 派生Movement Componentが衝突解決などを追加できる移動適用点。
		virtual void ApplyMovement(float deltaTime);

	private:
		/// JSONとDetailsで共有する編集プロパティ一覧を生成する。
		std::vector<ComponentProperty> CreateProperties();

	private:
		Vector3 velocity_{};
		bool movementEnabled_ = true;
	};
} // namespace Ken4lowEngine
