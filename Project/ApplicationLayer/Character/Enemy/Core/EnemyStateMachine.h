#pragma once
#include "EnemyAICommand.h"
#include "EnemyGunAI.h"
#include "EnemyArchetype.h"

#include <optional>
#include <variant>
#include <algorithm>
#include <numbers>
#include <cmath>

#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

namespace EnemyAIUtil
{
	inline float DistXZ(const K4E::Vector3& a, const K4E::Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return std::sqrt(dx * dx + dz * dz);
	}

	inline float LenXZ(const K4E::Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}
}

// ------------------------------------------------------------
// Enemy variant-FSM (C++20)
//  - 状態遷移は std::optional<EnemyStateId>
//  - Attack状態の中で EnemyGunAI を使用して「距離管理/ストレイフ/バースト」を行う
//  - 遮蔽物に隠れた相手には左右へ“ピーク”して射線を作る
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

	// 出力（Enemyが実行）
	EnemyAICommand& cmd;

	// 経過時間
	float dt = 0.0f;

	// ターゲット情報（Enemy側で埋める）
	K4E::Vector3 playerPos{};
	float distToPlayer = 0.0f;     // 水平距離(XZ)が入る想定
	bool canSeePlayer = false;
	bool canShootPlayer = false;   // 射線が通るか（マズル→ターゲット）

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
	enum class Phase { Move, WaitScan };

	Phase phase = Phase::Move;
	float timer = 0.0f;
	K4E::Vector3 goal{};
	bool hasGoal = false;

	// 調整値
	float roamRadius = 18.0f;
	float reachDist = 0.6f;
	float moveTimeMin = 0.2f;
	float moveTimeMax = 5.0f;
	float waitScanMin = 0.2f;
	float waitScanMax = 2.0f;
	float scanAngularSpeed = 2.0f;
	float scanLookRadius = 5.0f;

	std::mt19937 rng{ 1234u };
	std::uniform_real_distribution<float> angDist{ 0.0f, 2.0f * std::numbers::pi_v<float> };
	std::uniform_real_distribution<float> roamDist{ 0.0f, 1.0f };

	float Rand01() { return roamDist(rng); }
	float RandRange(float a, float b) { return a + (b - a) * Rand01(); }

	void Enter(EnemyAIContext<TEnemy>&)
	{
		phase = Phase::Move;
		timer = 0.0f;
		hasGoal = false;
	}
	void Exit(EnemyAIContext<TEnemy>&) {}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		ctx.cmd.debugState = static_cast<int>(EnemyStateId::Idle);
		if (ctx.canSeePlayer) return EnemyStateId::Chase;

		const K4E::Vector3 selfPos = ctx.self.GetCenterPosition();

		if (phase == Phase::Move)
		{
			timer -= ctx.dt;
			if (!hasGoal || timer <= 0.0f)
			{
				const K4E::Vector3 home = ctx.self.GetHomePos();
				const float a = angDist(rng);
				const float r = std::sqrt(Rand01()) * roamRadius;
				goal = { home.x + std::cosf(a) * r, home.y, home.z + std::sinf(a) * r };
				hasGoal = true;
				timer = RandRange(moveTimeMin, moveTimeMax);
			}

			const float d = EnemyAIUtil::DistXZ(selfPos, goal);
			if (d <= reachDist)
			{
				phase = Phase::WaitScan;
				timer = RandRange(waitScanMin, waitScanMax);
				ctx.cmd.stopMove = true;
				return std::nullopt;
			}

			ctx.cmd.moveGoal = goal;
			ctx.cmd.lookAt = goal;
			return std::nullopt;
		}

		timer -= ctx.dt;
		ctx.cmd.stopMove = true;
		const float t = (waitScanMax - timer);
		const float a = t * scanAngularSpeed;
		ctx.cmd.lookAt = { selfPos.x + std::cosf(a) * scanLookRadius, selfPos.y, selfPos.z + std::sinf(a) * scanLookRadius };

		if (timer <= 0.0f)
		{
			phase = Phase::Move;
			timer = 0.0f;
			hasGoal = false;
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

		ctx.cmd.moveGoal = ctx.playerPos;
		ctx.cmd.lookAt = ctx.playerPos;

		if (ctx.self.IsInAttackRange(ctx.distToPlayer)) return EnemyStateId::Attack;
		return std::nullopt;
	}
};

template<class TEnemy>
struct EnemyAttack
{
	EnemyGunAIState  gun{};
	EnemyGunAIParams p{};
	bool  inited = false;
	float fireFallbackAcc_ = 0.0f;

	// ---- Aim spread tuning ----
	float spreadNearDeg_ = 0.5f;
	float spreadFarDeg_ = 2.8f;
	float moveSpreadMul_ = 1.25f;
	float burstSpreadMul_ = 1.10f;

	// ---- Peek tuning ----
	float peekMoveSpeedMul_ = 0.92f;   // 通常の横移動より少しだけ控えめ
	float peekCommitSec_ = 0.55f;      // 一度ピークし始めたら最低これだけ継続
	float peekSwitchAfterSec_ = 1.15f; // 射線が通らなければ反対側へ切り替え
	float peekExposeHoldSec_ = 0.16f;  // 射線が通った直後も少し顔を出し続ける
	float peekBackstepMul_ = 0.28f;    // 近すぎる時だけ少し下がる

	float peekTimer_ = 0.0f;
	float exposeTimer_ = 0.0f;
	float peekSign_ = 1.0f;
	bool  wasBlockedLastFrame_ = false;

	void Enter(EnemyAIContext<TEnemy>& ctx)
	{
		const EnemyTuning& t = ctx.self.GetTuning();
		p.attackRange = t.attackRange;
		p.moveSpeed = t.moveSpeed;
		p.strafeSpeed = t.moveSpeed * t.strafeSpeedMul;
		p.preferredMinDist = (std::max)(2.5f, t.attackRange * t.preferredMinRatio);
		p.preferredMaxDist = (std::max)(p.preferredMinDist + 1.0f, t.attackRange * t.preferredMaxRatio);
		p.fireIntervalSec = (std::max)(0.05f, t.fireInterval);
		p.burstMinShots = t.burstMin;
		p.burstMaxShots = t.burstMax;
		p.aimMoveMul = t.aimMoveMul;
		p.burstMoveMul = t.burstMoveMul;
		p.reactionDelaySec = t.reactionDelaySec;
		spreadNearDeg_ = t.spreadNearDeg;
		spreadFarDeg_ = t.spreadFarDeg;
		inited = true;

		gun = EnemyGunAIState{};
		fireFallbackAcc_ = 0.0f;
		peekTimer_ = 0.0f;
		exposeTimer_ = 0.0f;
		peekSign_ = ((gun.rng() & 1u) == 0u) ? -1.0f : 1.0f;
		wasBlockedLastFrame_ = false;
	}

	void Exit(EnemyAIContext<TEnemy>&) {}

	K4E::Vector3 ApplyAimSpread(const K4E::Vector3& selfPos, const K4E::Vector3& targetPos, float distToTarget, float currentMoveSpeed)
	{
		const float safeRange = (std::max)(0.001f, p.attackRange);
		const float t = std::clamp(distToTarget / safeRange, 0.0f, 1.0f);
		float spreadDeg = spreadNearDeg_ + (spreadFarDeg_ - spreadNearDeg_) * t;
		if (currentMoveSpeed > 0.01f) spreadDeg *= moveSpreadMul_;
		if (gun.phase == EnemyGunPhase::Burst) spreadDeg *= burstSpreadMul_;

		const float pi = std::numbers::pi_v<float>;
		const float spreadRad = spreadDeg * (pi / 180.0f);
		const float sigma = std::tanf(spreadRad) * distToTarget;
		const float sigmaV = sigma * 0.6f;
		if (sigma <= 1e-5f) return targetPos;

		K4E::Vector3 to = targetPos - selfPos;
		to.y = 0.0f;
		if (EnemyAIUtil::LenXZ(to) <= 1e-5f) return targetPos;

		const K4E::Vector3 dir = NormalizeXZ(to);
		const K4E::Vector3 right = NormalizeXZ(PerpRightXZ(dir));
		std::uniform_real_distribution<float> dx(-sigma, sigma);
		std::uniform_real_distribution<float> dy(-sigmaV, sigmaV);
		const float offX = dx(gun.rng);
		const float offY = dy(gun.rng);
		return targetPos + right * offX + K4E::Vector3{ 0.0f, 1.0f, 0.0f } * offY;
	}

	void ApplyPeekMove(EnemyAIContext<TEnemy>& ctx, const K4E::Vector3& selfPos)
	{
		const K4E::Vector3 toTarget = ctx.playerPos - selfPos;
		const K4E::Vector3 dirTo = NormalizeXZ(toTarget);
		const K4E::Vector3 right = NormalizeXZ(PerpRightXZ(dirTo));
		K4E::Vector3 move = right * peekSign_;

		// 近すぎるなら少しだけ下がりながら顔を出す
		if (ctx.distToPlayer < p.preferredMinDist)
		{
			move = NormalizeXZ(move + dirTo * -peekBackstepMul_);
		}

		ctx.cmd.moveDir = move;
		ctx.cmd.moveSpeed = p.strafeSpeed * peekMoveSpeedMul_;
		ctx.cmd.lookAt = ctx.playerPos;
	}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		ctx.cmd.debugState = static_cast<int>(EnemyStateId::Attack);
		if (!ctx.canSeePlayer) return EnemyStateId::Search;
		if (ctx.distToPlayer > ctx.self.GetAttackRange() * 2.0f) return EnemyStateId::Chase;

		EnemyGunAIInput in{};
		in.dt = ctx.dt;
		in.selfPos = ctx.self.GetCenterPosition();
		in.targetPos = ctx.playerPos;
		in.canSeeTarget = ctx.canSeePlayer;
		in.canShootTarget = ctx.canShootPlayer; // ← 壁越し撃ちをやめる
		in.distToTarget = ctx.distToPlayer;
		in.lastSeenPos = ctx.lastSeenPos;
		in.timeSinceSeen = ctx.timeSinceSeen;

		const bool blockedButVisible = ctx.canSeePlayer && !ctx.canShootPlayer;
		if (blockedButVisible)
		{
			if (!wasBlockedLastFrame_)
			{
				peekTimer_ = 0.0f;
				exposeTimer_ = 0.0f;
			}
			peekTimer_ += ctx.dt;
			if (peekTimer_ >= peekSwitchAfterSec_)
			{
				peekTimer_ = 0.0f;
				peekSign_ *= -1.0f;
			}
			wasBlockedLastFrame_ = true;

			ApplyPeekMove(ctx, in.selfPos);
			fireFallbackAcc_ = 0.0f;
			return std::nullopt;
		}

		// 射線が通った直後も少しだけ出続けると「顔出し→撃つ」感が増す
		if (wasBlockedLastFrame_)
		{
			exposeTimer_ = peekExposeHoldSec_;
			wasBlockedLastFrame_ = false;
		}
		if (exposeTimer_ > 0.0f)
		{
			exposeTimer_ -= ctx.dt;
			if (peekTimer_ < peekCommitSec_)
			{
				peekTimer_ += ctx.dt;
				ApplyPeekMove(ctx, in.selfPos);
			}
		}
		else
		{
			peekTimer_ = 0.0f;
		}

		EnemyGunAIOutput out{};
		EnemyGunAI_Update(gun, p, in, out);

		if (out.wantLookAt) ctx.cmd.lookAt = out.lookAt;
		else ctx.cmd.lookAt = ctx.playerPos;

		float usedMoveSpeed = 0.0f;
		// 顔出し継続中は通常の strafe で上書きしない
		const bool keepPeekExpose = (exposeTimer_ > 0.0f && peekTimer_ > 0.0f);
		if (!keepPeekExpose)
		{
			if (out.moveSpeed > 0.0f && (std::fabs(out.moveDirXZ.x) + std::fabs(out.moveDirXZ.z)) > 1e-4f)
			{
				ctx.cmd.moveDir = out.moveDirXZ;
				ctx.cmd.moveSpeed = out.moveSpeed;
				usedMoveSpeed = out.moveSpeed;
			}
			else if (!ctx.cmd.moveDir)
			{
				ctx.cmd.stopMove = true;
			}
		}
		else if (ctx.cmd.moveSpeed)
		{
			usedMoveSpeed = *ctx.cmd.moveSpeed;
		}

		if (out.wantFire && ctx.canShootPlayer)
		{
			ctx.cmd.fireAt = ApplyAimSpread(in.selfPos, ctx.playerPos, ctx.distToPlayer, usedMoveSpeed);
			fireFallbackAcc_ = 0.0f;
		}

		if (!ctx.cmd.fireAt && gun.phase != EnemyGunPhase::Burst && ctx.canShootPlayer)
		{
			fireFallbackAcc_ += ctx.dt;
			const float fallbackInterval = ctx.self.GetFireInterval() * 1.35f;
			if (fireFallbackAcc_ >= fallbackInterval)
			{
				if (ctx.distToPlayer <= ctx.self.GetAttackRange())
				{
					ctx.cmd.fireAt = ApplyAimSpread(in.selfPos, ctx.playerPos, ctx.distToPlayer, usedMoveSpeed);
				}
				fireFallbackAcc_ = 0.0f;
			}
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
		ctx.cmd.debugState = static_cast<int>(EnemyStateId::Search);
		t += ctx.dt;
		if (ctx.canSeePlayer) return EnemyStateId::Chase;
		ctx.cmd.moveGoal = ctx.lastSeenPos;
		ctx.cmd.lookAt = ctx.lastSeenPos;
		if (t > 6.0f) return EnemyStateId::Idle;
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
			// 最近撃たれた/最近見た記憶があるなら、棒立ちに戻さず捜索へ
			if (ctx.timeSinceSeen <= 1.25f)
			{
				return EnemyStateId::Search;
			}
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
		case EnemyStateId::Idle:    state_.template emplace<EnemyIdle<TEnemy>>(); break;
		case EnemyStateId::Chase:   state_.template emplace<EnemyChase<TEnemy>>(); break;
		case EnemyStateId::Attack:  state_.template emplace<EnemyAttack<TEnemy>>(); break;
		case EnemyStateId::Search:  state_.template emplace<EnemySearch<TEnemy>>(); break;
		case EnemyStateId::Stunned: state_.template emplace<EnemyStunned<TEnemy>>(); break;
		}
		std::visit([&](auto& s) { s.Enter(ctx); }, state_);
	}

private:
	EnemyStateId current_ = EnemyStateId::Idle;
	State state_{ EnemyIdle<TEnemy>{} };
};
