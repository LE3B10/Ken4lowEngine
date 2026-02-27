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
	float distToPlayer = 0.0f;     // ★水平距離(XZ)が入る想定
	bool canSeePlayer = false;
	bool canShootPlayer = false;   // ★追加：射線が通るか（マズル→ターゲット）

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
	float roamRadius = 18.0f;      // home周りの徘徊半径
	float reachDist = 0.6f;      // 到着判定
	float moveTimeMin = 0.2f;
	float moveTimeMax = 5.0f;

	float waitScanMin = 0.2f;     // 停止+索敵時間（短く）
	float waitScanMax = 2.0f;
	float scanAngularSpeed = 2.0f; // 首振り速度
	float scanLookRadius = 5.0f;   // lookAt距離

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

		// 見えたら即Chaseへ
		if (ctx.canSeePlayer) return EnemyStateId::Chase;

		const K4E::Vector3 selfPos = ctx.self.GetCenterPosition();

		// ---- Moveフェーズ：徘徊してる時間の方が長い ----
		if (phase == Phase::Move)
		{
			timer -= ctx.dt;

			// goal生成
			if (!hasGoal || timer <= 0.0f)
			{
				const K4E::Vector3 home = ctx.self.GetHomePos();
				const float a = angDist(rng);
				// 半径を均等に散らすなら sqrt を使う（端に寄りすぎない）
				const float r = std::sqrt(Rand01()) * roamRadius;

				goal = { home.x + std::cosf(a) * r, home.y, home.z + std::sinf(a) * r };
				hasGoal = true;
				timer = RandRange(moveTimeMin, moveTimeMax);
			}

			// 到着したら WaitScan に切り替え
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

		// ---- WaitScanフェーズ：短時間だけ停止して索敵（常時回転しない）----
		timer -= ctx.dt;
		ctx.cmd.stopMove = true;

		const float t = (waitScanMax - timer); // 経過っぽい値
		const float a = t * scanAngularSpeed;

		ctx.cmd.lookAt = { selfPos.x + std::cosf(a) * scanLookRadius, selfPos.y, selfPos.z + std::sinf(a) * scanLookRadius };

		if (timer <= 0.0f)
		{
			phase = Phase::Move;
			timer = 0.0f;
			hasGoal = false; // 次のMoveで新しいgoal
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
	EnemyGunAIState  gun{};
	EnemyGunAIParams p{};
	bool  inited = false;

	float fireFallbackAcc_ = 0.0f; // ★保険：環境依存で wantFire が立たない時でも撃つ

	// ---- Aim spread tuning（雑魚っぽい命中ブレ）----
	float spreadNearDeg_ = 0.5f;   // 近距離のブレ（度）
	float spreadFarDeg_ = 2.8f;   // 遠距離のブレ（度）
	float moveSpreadMul_ = 1.25f;  // 移動中のブレ倍率
	float burstSpreadMul_ = 1.10f; // バースト中のブレ倍率

	void Enter(EnemyAIContext<TEnemy>& ctx)
	{
		// アーキタイプ（EnemyTuning）から毎回パラメータを反映（タイプ変更にも追従）
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

			// 命中ブレ
			spreadNearDeg_ = t.spreadNearDeg;
			spreadFarDeg_ = t.spreadFarDeg;

			inited = true;
		}

		// 状態はAttackに入るたびリセット（好みで保持してもOK）
		gun = EnemyGunAIState{};
		fireFallbackAcc_ = 0.0f;
	}

	void Exit(EnemyAIContext<TEnemy>&) {}

	// targetPos を少しだけズラして「命中ブレ」を作る
	K4E::Vector3 ApplyAimSpread(const K4E::Vector3& selfPos, const K4E::Vector3& targetPos, float distToTarget, float currentMoveSpeed)
	{
		// dist は水平距離でOK（上下のブレは別で入れる）
		const float safeRange = (std::max)(0.001f, p.attackRange);
		const float t = std::clamp(distToTarget / safeRange, 0.0f, 1.0f);

		// 距離でブレを補間
		float spreadDeg = spreadNearDeg_ + (spreadFarDeg_ - spreadNearDeg_) * t;

		// 状態/移動でブレ倍率をかける
		if (currentMoveSpeed > 0.01f) spreadDeg *= moveSpreadMul_;
		if (gun.phase == EnemyGunPhase::Burst) spreadDeg *= burstSpreadMul_;

		// deg → 半径（ワールド単位）
		const float pi = 3.14159265f;
		const float spreadRad = spreadDeg * (pi / 180.0f);
		const float sigma = std::tanf(spreadRad) * distToTarget;
		const float sigmaV = sigma * 0.6f; // 縦は少し小さめ

		if (sigma <= 1e-5f) return targetPos;

		// 右方向（XZ）を作る
		K4E::Vector3 to = targetPos - selfPos;
		to.y = 0.0f;
		if (LenXZ(to) <= 1e-5f) return targetPos;

		const K4E::Vector3 dir = NormalizeXZ(to);
		const K4E::Vector3 right = NormalizeXZ(PerpRightXZ(dir));

		std::uniform_real_distribution<float> dx(-sigma, sigma);
		std::uniform_real_distribution<float> dy(-sigmaV, sigmaV);

		const float offX = dx(gun.rng);
		const float offY = dy(gun.rng);

		// ターゲットの“狙い点”をずらす
		return targetPos + right * offX + K4E::Vector3{ 0.0f, 1.0f, 0.0f } * offY;
	}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		ctx.cmd.debugState = (int)EnemyStateId::Attack;

		if (!ctx.canSeePlayer) return EnemyStateId::Search;

		// かなり離れたら一旦Chaseへ
		if (ctx.distToPlayer > ctx.self.GetAttackRange() * 2.0f)
			return EnemyStateId::Chase;

		EnemyGunAIInput in{};
		in.dt = ctx.dt;
		in.selfPos = ctx.self.GetCenterPosition();
		in.targetPos = ctx.playerPos;
		in.canSeeTarget = ctx.canSeePlayer;

		// ★まずは撃てること優先（遮蔽物LOSは後で有効化）
		//   LOSを使うなら: in.canShootTarget = ctx.canShootPlayer;
		in.canShootTarget = true;

		in.distToTarget = ctx.distToPlayer;
		in.lastSeenPos = ctx.lastSeenPos;
		in.timeSinceSeen = ctx.timeSinceSeen;

		EnemyGunAIOutput out{};
		EnemyGunAI_Update(gun, p, in, out);

		// 向き
		if (out.wantLookAt) ctx.cmd.lookAt = out.lookAt;
		else ctx.cmd.lookAt = ctx.playerPos;

		// 移動
		float usedMoveSpeed = 0.0f;
		if (out.moveSpeed > 0.0f && (std::fabs(out.moveDirXZ.x) + std::fabs(out.moveDirXZ.z)) > 1e-4f)
		{
			ctx.cmd.moveDir = out.moveDirXZ;
			ctx.cmd.moveSpeed = out.moveSpeed;
			usedMoveSpeed = out.moveSpeed;
		}
		else
		{
			ctx.cmd.stopMove = true;
		}

		// 射撃（GunAIがバーストで wantFire を立てる）
		if (out.wantFire)
		{
			ctx.cmd.fireAt = ApplyAimSpread(in.selfPos, ctx.playerPos, ctx.distToPlayer, usedMoveSpeed);
			fireFallbackAcc_ = 0.0f; // ★撃てたら保険タイマーはリセット
		}

		// ★保険：何かの条件で wantFire が立たない環境でも最低限撃つ
		//   バースト中に割り込むと弾が増えすぎるので Burst 中は保険を止める
		if (!ctx.cmd.fireAt && gun.phase != EnemyGunPhase::Burst)
		{
			fireFallbackAcc_ += ctx.dt;
			const float fallbackInterval = ctx.self.GetFireInterval() * 1.35f; // 少し遅め
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
		duration = ctx.self.ConsumeStunDurationOr(duration);
	}
	void Exit(EnemyAIContext<TEnemy>&) {}

	std::optional<EnemyStateId> Update(EnemyAIContext<TEnemy>& ctx)
	{
		t += ctx.dt;
		ctx.cmd.debugState = static_cast<int>(EnemyStateId::Stunned);
		ctx.cmd.stopMove = true;

		if (t >= duration) return EnemyStateId::Idle;
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
