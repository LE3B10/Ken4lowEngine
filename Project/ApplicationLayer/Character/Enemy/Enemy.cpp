#define NOMINMAX
#include "Enemy.h"

#include <algorithm>
#include <cmath>

#include "Bullet.h"
#include "BulletManager.h"
#include "CollisionTypeIdDef.h"

namespace
{
	using K4E::Vector3;

	inline float LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	inline float Length(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	inline Vector3 NormalizeSafe(const Vector3& v)
	{
		const float len = Length(v);
		if (len <= 1e-6f) return { 0.0f, 0.0f, 0.0f };
		return { v.x / len, v.y / len, v.z / len };
	}
}

void Enemy::Initialize(const K4E::Vector3& startPos, const std::string& modelPath)
{
	EnemyBase::Initialize(startPos, modelPath);

	// 初期値
	lastSeenPos_ = startPos;
	timeSinceSeen_ = 9999.0f;
	stunRequestedSec_ = 0.0f;

	// FSM初期化
	EnemyAICommand cmd{};
	EnemyAIContext<Enemy> ctx{ *this, cmd, 0.0f };
	BuildContext(ctx);
	fsm_.Reset(ctx);
}

void Enemy::Update(float dt)
{
	if (IsRemovable()) return;
	if (IsDead())
	{
		EnemyBase::Update(dt);
		return;
	}

	// 1) command を作る（毎フレーム）
	EnemyAICommand cmd{};
	cmd.Clear();

	// 2) context を組み立てる
	EnemyAIContext<Enemy> ctx{ *this, cmd, dt };
	BuildContext(ctx);

	// 3) スタン要求があるなら先に強制遷移
	if (stunRequestedSec_ > 0.0f)
	{
		fsm_.Force(EnemyStateId::Stunned, ctx);
	}

	// 4) decision
	fsm_.Update(ctx);

	// 5) act
	ApplyAICommand(cmd);

	// 6) physics integrate
	EnemyBase::Update(dt);
}

void Enemy::BuildContext(EnemyAIContext<Enemy>& ctx)
{
	// ターゲットが無いなら何も見えない
	ctx.canSeePlayer = false;
	ctx.distToPlayer = 999999.0f;
	ctx.playerPos = { 0.0f,0.0f,0.0f };

	if (target_)
	{
		ctx.playerPos = target_->GetCenterPosition();
		const auto d = ctx.playerPos - GetCenterPosition();
		const float dist = Length(d);
		ctx.distToPlayer = dist;
		ctx.canSeePlayer = (dist <= viewRange_);
	}

	// memory
	if (ctx.canSeePlayer)
	{
		lastSeenPos_ = ctx.playerPos;
		timeSinceSeen_ = 0.0f;
	}
	else
	{
		timeSinceSeen_ += ctx.dt;
	}

	ctx.lastSeenPos = lastSeenPos_;
	ctx.timeSinceSeen = timeSinceSeen_;
}

void Enemy::ApplyAICommand(const EnemyAICommand& cmd)
{
	if (cmd.stopMove)
	{
		StopMove();
	}
	else if (cmd.moveGoal)
	{
		MoveTowards(*cmd.moveGoal);
	}
	else
	{
		// 何も指示が無いなら停止（好みで）
		StopMove();
	}

	if (cmd.lookAt) FaceTo(*cmd.lookAt);
	if (cmd.fireAt) FireAt(*cmd.fireAt);
}

void Enemy::MoveTowards(const K4E::Vector3& goal)
{
	auto to = goal - GetCenterPosition();
	to.y = 0.0f;

	const Vector3 dir = NormalizeSafe(to);
	SetVelocity(dir * moveSpeed_);
}

void Enemy::StopMove()
{
	SetVelocity({ 0.0f, 0.0f, 0.0f });
}

void Enemy::FaceTo(const K4E::Vector3& lookAt)
{
	auto d = lookAt - GetCenterPosition();
	d.y = 0.0f;
	if (LengthXZ(d) <= 1e-6f) return;

	// Z前方の想定（必要なら atan2 の引数を入れ替えて調整）
	const float yaw = std::atan2(d.x, d.z);
	SetOrientation({ 0.0f, yaw, 0.0f });
}

void Enemy::FireAt(const K4E::Vector3& targetPos)
{
	if (!bulletManager_) return;

	Vector3 origin = GetCenterPosition();
	origin.y += muzzleHeight_;

	Vector3 dir = NormalizeSafe(targetPos - origin);
	if (Length(dir) <= 1e-6f) return;

	bulletManager_->Spawn(origin, dir, bulletSpeed_, bulletDamage_, bulletLifeSec_,
		static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet));
}

void Enemy::RequestStun(float sec)
{
	stunRequestedSec_ = std::max(stunRequestedSec_, sec);
}

float Enemy::ConsumeStunDurationOr(float fallbackSec)
{
	if (stunRequestedSec_ <= 0.0f) return fallbackSec;
	const float v = stunRequestedSec_;
	stunRequestedSec_ = 0.0f;
	return v;
}

void Enemy::OnBulletHit(K4E::Collider* bulletCollider)
{
	int dmg = 10;
	if (bulletCollider)
	{
		if (auto* b = bulletCollider->GetOwner<Bullet>())
		{
			dmg = b->GetDamage();
		}
	}
	TakeDamage(dmg);

	// 被弾で軽スタン（任意）
	RequestStun(0.12f);
}
