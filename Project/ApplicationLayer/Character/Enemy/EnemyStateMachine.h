#pragma once
#include "EnemyAICommand.h"

#include <optional>
#include <variant>

#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

// ------------------------------------------------------------
// Enemy variant-FSM (C++20)
//  - テンプレート化しているので、Enemy型の前方宣言だけで利用可能
//  - 状態遷移は std::optional<EnemyStateId>
// ------------------------------------------------------------

enum class EnemyStateId
{
	Idle,
	Chase,
	Attack,
	Search,
	Stunned,
};

template<class TEnemy>
struct EnemyAIContext
{
	TEnemy& self;

	// 意思決定の出力
	EnemyAICommand& cmd; // 命令をここに書き込む

	float dt = 0.0f;

	// ターゲット情報（Enemy側で埋める）
	K4E::Vector3 playerPos{};
	float distToPlayer = 0.0f;
	bool canSeePlayer = false;

	// 記憶
	K4E::Vector3 lastSeenPos{};
	float timeSinceSeen = 9999.0f;

	EnemyAIContext(TEnemy& s, EnemyAICommand& c, float delta)
		: self(s), cmd(c), dt(delta) {
	}
};

template<class TEnemy>
struct EnemyIdle
{
	float t = 0.0f;
	void Enter(EnemyAIContext<TEnemy>&) { t = 0.0f; }
	void Exit(EnemyAIContext<TEnemy>&) {}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		t += ctx.dt;
		ctx.cmd.debugState = static_cast<int>(EnemyStateId::Idle);
		ctx.cmd.stopMove = true;

		if (ctx.canSeePlayer)
		{
			return EnemyStateId::Chase;
		}
		return std::nullopt;
	}
};

template<class TEnemy>
struct EnemyChase
{
	void Enter(EnemyAIContext<TEnemy>&) {}
	void Exit(EnemyAIContext<TEnemy>&) {}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		ctx.cmd.debugState = static_cast<int>(EnemyStateId::Chase);

		if (!ctx.canSeePlayer) return EnemyStateId::Search;

		// MoveTo命令
		ctx.cmd.moveGoal = ctx.playerPos;
		ctx.cmd.lookAt = ctx.playerPos;

		if (ctx.self.IsInAttackRange(ctx.distToPlayer)) return EnemyStateId::Attack;

		return std::nullopt;
	}
};

template<class TEnemy>
struct EnemyAttack
{
	float fireCooldown = 0.0f;

	void Enter(EnemyAIContext<TEnemy>&) { fireCooldown = 0.0f; }
	void Exit(EnemyAIContext<TEnemy>&) {}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		ctx.cmd.debugState = (int)EnemyStateId::Attack;

		if (!ctx.canSeePlayer) return EnemyStateId::Search;
		if (!ctx.self.IsInAttackRange(ctx.distToPlayer)) return EnemyStateId::Chase;

		ctx.cmd.stopMove = true;
		ctx.cmd.lookAt = ctx.playerPos;

		fireCooldown -= ctx.dt;
		if (fireCooldown <= 0.0f)
		{
			ctx.cmd.fireAt = ctx.playerPos;   // ★撃て命令
			fireCooldown = ctx.self.GetFireInterval();
		}
		return std::nullopt;
	}
};

template<class TEnemy>
struct EnemySearch
{
	float t = 0.0f;

	void Enter(EnemyAIContext<TEnemy>&) { t = 0.0f; }
	void Exit(EnemyAIContext<TEnemy>&) {}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		ctx.cmd.debugState = (int)EnemyStateId::Search;

		t += ctx.dt;
		if (ctx.canSeePlayer) return EnemyStateId::Chase;

		ctx.cmd.moveGoal = ctx.lastSeenPos;
		ctx.cmd.lookAt = ctx.lastSeenPos;

		if (t > 2.0f) return EnemyStateId::Idle;
		return std::nullopt;
	}
};

template<class TEnemy>
struct EnemyStunned
{
	float t = 0.0f;
	float duration = 0.3f;

	void Enter(EnemyAIContext<TEnemy>& ctx)
	{
		t = 0.0f;
		// Enemy側からスタン時間要求があればそれを使う
		duration = ctx.self.ConsumeStunDurationOr(duration);
	}
	void Exit(EnemyAIContext<TEnemy>&) {}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		t += ctx.dt;
		ctx.cmd.debugState = static_cast<int>(EnemyStateId::Stunned);
		ctx.cmd.stopMove = true;

		if (t >= duration)
		{
			return EnemyStateId::Idle;
		}

		return std::nullopt;
	}
};

template<class TEnemy>
class EnemyStateMachine
{
public:
	using State = std::variant<
		EnemyIdle<TEnemy>,
		EnemyChase<TEnemy>,
		EnemyAttack<TEnemy>,
		EnemySearch<TEnemy>,
		EnemyStunned<TEnemy>
	>;

	void Reset(EnemyAIContext<TEnemy>& ctx)
	{
		current_ = EnemyStateId::Idle;
		state_.template emplace<EnemyIdle<TEnemy>>();
		std::get<EnemyIdle<TEnemy>>(state_).Enter(ctx);
	}

	void Update(EnemyAIContext<TEnemy>& ctx)
	{
		std::optional<EnemyStateId> next;
		std::visit([&](auto& s) { next = s.Update(ctx); }, state_);
		if (next) { Change(*next, ctx); }
	}

	// 外部から強制遷移（被弾スタンなど）
	void Force(EnemyStateId id, EnemyAIContext<TEnemy>& ctx)
	{
		if (current_ == id) return;
		Change(id, ctx);
	}

	EnemyStateId GetStateId() const { return current_; }

private:
	void Change(EnemyStateId id, EnemyAIContext<TEnemy>& ctx)
	{
		std::visit([&](auto& s) { s.Exit(ctx); }, state_);

		current_ = id;
		switch (id)
		{
		case EnemyStateId::Idle:   state_.template emplace<EnemyIdle<TEnemy>>(); break;
		case EnemyStateId::Chase:  state_.template emplace<EnemyChase<TEnemy>>(); break;
		case EnemyStateId::Attack: state_.template emplace<EnemyAttack<TEnemy>>(); break;
		case EnemyStateId::Search: state_.template emplace<EnemySearch<TEnemy>>(); break;
		case EnemyStateId::Stunned:state_.template emplace<EnemyStunned<TEnemy>>(); break;
		}

		std::visit([&](auto& s) { s.Enter(ctx); }, state_);
	}

private:
	EnemyStateId current_ = EnemyStateId::Idle;
	State state_{ EnemyIdle<TEnemy>{} };
};