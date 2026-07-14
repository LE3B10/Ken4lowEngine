#include "AttackBehaviors.h"

#include "CharacterActor.h"
#include "CharacterMovementComponent.h"
#include "SceneComponent.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{
	namespace
	{
		/// 単一TargetへDamageを適用し、共通攻撃イベント用の結果へ変換する。
		AttackExecutionResult ApplyDamageOnce(AttackContext& context, float damage)
		{
			AttackExecutionResult execution{};
			execution.executed = true;
			if (!context.target || context.target->IsDead()) return execution;
			const CharacterDamageResult damageResult = context.target->ApplyDamage(damage);
			execution.accepted = damageResult.accepted;
			execution.appliedDamage = damageResult.appliedDamage;
			return execution;
		}

		/// XZ平面上の方向を0除算せず正規化する。
		Vector3 NormalizeXZ(const Vector3& value)
		{
			const float length = Vector3::LengthXZ(value);
			return length > 0.0001f ? Vector3{ value.x / length, 0.0f, value.z / length } : Vector3{};
		}

		/// 攻撃Active中も水平距離とY差の両方を確認し、高所Targetへの不自然な命中を防ぐ。
		bool IsWithinAttackVolume(const AttackContext& context, const AttackData& data)
		{
			return context.distanceToTarget >= data.minRange &&
				context.distanceToTarget <= data.maxRange &&
				context.heightDifferenceToTarget <= data.maxHeightDifference;
		}
	}

	bool MeleeAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.target && !context.target->IsDead() && IsWithinAttackVolume(context, data);
	}

	void MeleeAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)context;
		(void)data;
		executed_ = false; // 同じActive中に複数フレーム更新されても1回だけ命中させる。
	}

	AttackExecutionResult MeleeAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)deltaTime;
		(void)normalizedActiveTime;
		if (executed_) return {};
		executed_ = true;
		if (!IsWithinAttackVolume(context, data)) return { true, false, 0.0f };
		return ApplyDamageOnce(context, data.damage);
	}

	void MeleeAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)context;
		(void)data;
		(void)interrupted;
		executed_ = false;
	}

	bool ProjectileAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.target && !context.target->IsDead() && IsWithinAttackVolume(context, data);
	}

	void ProjectileAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)context;
		(void)data;
		fired_ = false; // 発射物生成へ差し替える場合もこの一度きりの発射点を共有する。
	}

	AttackExecutionResult ProjectileAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)deltaTime;
		(void)normalizedActiveTime;
		if (fired_) return {};
		fired_ = true;
		if (!IsWithinAttackVolume(context, data)) return { true, false, 0.0f };
		return ApplyDamageOnce(context, data.damage);
	}

	void ProjectileAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)context;
		(void)data;
		(void)interrupted;
		fired_ = false;
	}

	bool ChargeAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.owner && context.target && data.movementSpeed > 0.0f && IsWithinAttackVolume(context, data);
	}

	void ChargeAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)context;
		(void)data;
		hit_ = false; // 突進中の接触ダメージは攻撃1回につき一度だけ許可する。
	}

	AttackExecutionResult ChargeAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)deltaTime;
		(void)normalizedActiveTime;
		if (!context.owner || !context.target) return {};
		CharacterMovementComponent* movement = context.owner->GetMovementComponent();
		const SceneComponent* root = context.owner->GetRootComponent();
		if (movement && root)
		{
			const Vector3 direction = NormalizeXZ(context.target->GetTargetPosition() - root->GetWorldPosition());
			movement->SetVelocity(direction * data.movementSpeed); // 突進も位置を直接変更せず共通Movementへ速度を渡す。
		}
		if (hit_ || !IsWithinAttackVolume(context, data)) return {};
		hit_ = true;
		return ApplyDamageOnce(context, data.damage);
	}

	void ChargeAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)data;
		(void)interrupted;
		if (context.owner)
		{
			if (CharacterMovementComponent* movement = context.owner->GetMovementComponent()) movement->Stop();
		}
		hit_ = false;
	}

	bool ShockwaveAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.target && IsWithinAttackVolume(context, data);
	}

	void ShockwaveAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)context;
		(void)data;
		emitted_ = false; // 将来の範囲検索へ差し替えても衝撃波生成を1回に保つ。
	}

	AttackExecutionResult ShockwaveAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)deltaTime;
		(void)normalizedActiveTime;
		if (emitted_) return {};
		emitted_ = true;
		if (!IsWithinAttackVolume(context, data)) return { true, false, 0.0f };
		return ApplyDamageOnce(context, data.damage);
	}

	void ShockwaveAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)context;
		(void)data;
		(void)interrupted;
		emitted_ = false;
	}

	std::unique_ptr<IAttackBehavior> CreateAttackBehavior(std::string_view behaviorType)
	{
		using Factory = std::function<std::unique_ptr<IAttackBehavior>()>;
		static const std::unordered_map<std::string, Factory> factories = {
			{ "Melee", [] { return std::make_unique<MeleeAttackBehavior>(); } },
			{ "Projectile", [] { return std::make_unique<ProjectileAttackBehavior>(); } },
			{ "Charge", [] { return std::make_unique<ChargeAttackBehavior>(); } },
			{ "Shockwave", [] { return std::make_unique<ShockwaveAttackBehavior>(); } }
		}; // 個別クラスをFactory登録し、攻撃種別追加で巨大なswitchを増やさない。
		const auto factoryIt = factories.find(std::string(behaviorType));
		return factoryIt != factories.end() ? factoryIt->second() : nullptr;
	}
} // namespace Ken4lowEngine
