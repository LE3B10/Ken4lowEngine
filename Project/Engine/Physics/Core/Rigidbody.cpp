#include "Rigidbody.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kMinimumMass = 0.0001f;
		const Vector3 kGravity{ 0.0f, -9.8f, 0.0f };
	}

	void Rigidbody::SetBodyType(BodyType bodyType)
	{
		bodyType_ = bodyType;

		// Staticは物理積分で動かさないため、逆質量を0として扱う。
		if (bodyType_ == BodyType::Static)
		{
			invMass_ = 0.0f;
		}
		else
		{
			SetMass(mass_);
		}
	}

	void Rigidbody::SetMass(float mass)
	{
		mass_ = std::max(mass, kMinimumMass);

		// Staticは質量値を保持しつつ、応答計算では無限質量として扱う。
		invMass_ = (bodyType_ == BodyType::Static) ? 0.0f : (1.0f / mass_);
	}

	void Rigidbody::AddForce(const Vector3& force)
	{
		// Dynamic以外は外力で速度を変えない。
		if (bodyType_ != BodyType::Dynamic)
		{
			return;
		}

		force_ += force;
	}

	void Rigidbody::ClearForces()
	{
		// Resetや外部制御から次ステップへ持ち越したくない力を破棄する。
		force_ = {};
	}

	void Rigidbody::SetVelocity(const Vector3& velocity)
	{
		// Staticは速度を持たせず、将来の押し戻し対象からも外しやすくする。
		velocity_ = (bodyType_ == BodyType::Static) ? Vector3{} : velocity;
	}

	Vector3 Rigidbody::GetVelocity() const
	{
		// 速度は読み取り専用で返し、外部からの変更はSetVelocityへ集約する。
		return velocity_;
	}

	void Rigidbody::Integrate(float deltaTime)
	{
		// 不正な時間やDynamic以外のBodyは速度積分を行わない。
		if (deltaTime <= 0.0f || bodyType_ != BodyType::Dynamic)
		{
			force_ = {};
			return;
		}

		Vector3 acceleration = force_ * invMass_;
		if (useGravity_)
		{
			acceleration += kGravity;
		}

		// 現段階では位置更新先を持たず、速度だけを更新して将来のTransform接続に備える。
		velocity_ += acceleration * deltaTime;
		force_ = {};
	}

} // namespace Ken4lowEngine
