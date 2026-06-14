#pragma once
#include "PhysicsTypes.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         剛体クラス
	/// -------------------------------------------------------------
	class Rigidbody
	{
	public: /// ---------- メンバ関数 ---------- ///

		// BodyTypeを設定し、Staticの場合は質量計算から外す。
		void SetBodyType(BodyType bodyType);
		BodyType GetBodyType() const { return bodyType_; }

		// 質量を設定し、逆質量を同時に更新する。
		void SetMass(float mass);
		float GetMass() const { return mass_; }
		float GetInvMass() const { return invMass_; }

		// 重力適用フラグを設定する。
		void SetUseGravity(bool useGravity) { useGravity_ = useGravity; }
		bool IsUseGravity() const { return useGravity_; }

		// 力を蓄積し、次のIntegrateで速度へ反映する。
		void AddForce(const Vector3& force);

		// 蓄積された力をクリアする。
		void ClearForces();

		// 速度を直接設定する。
		void SetVelocity(const Vector3& velocity);

		// 現在の速度を取得する。
		Vector3 GetVelocity() const;

		// 蓄積された力を速度へ積分し、力をクリアする。
		void Integrate(float deltaTime);

	private: /// ---------- メンバ変数 ---------- ///

		// 剛体の動作種別。
		BodyType bodyType_ = BodyType::Dynamic;

		// 現在速度。
		Vector3 velocity_{};

		// このステップで蓄積された力。
		Vector3 force_{};

		// 質量と逆質量。
		float mass_ = 1.0f;
		float invMass_ = 1.0f;

		// 重力を速度へ反映するか。
		bool useGravity_ = false;
	};

} // namespace Ken4lowEngine
