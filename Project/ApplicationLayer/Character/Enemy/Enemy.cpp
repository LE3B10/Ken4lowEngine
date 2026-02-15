#define NOMINMAX
#include "Enemy.h"

#include <algorithm>
#include <cmath>

#include "Bullet.h"
#include "BulletManager.h"
#include "CollisionTypeIdDef.h"
#include "CollisionManager.h"
#include "LinearInterpolation.h"
#include "Wireframe.h"

namespace
{
	using K4E::Vector3;

	static EnemyTuning MakeTuning(EnemyArchetype t)
	{
		EnemyTuning p{};
		switch (t)
		{
		case EnemyArchetype::RifleGrunt:
			p.moveSpeed = 3.0f;		// [m/s] 移動速度（追跡/回避/徘徊の速さ）
			p.attackRange = 44.0f;  // [m] 射撃を開始する距離（これ以上遠いと基本撃たない）
			p.viewRange = 32.0f;	// [m] 視認距離（これより遠いと発見/追跡/攻撃に入らない）
			p.fireInterval = 0.25f; // [sec] 1発（または次バースト内の次弾）までの最短間隔
			p.burstMin = 2;			// [shots] 1バーストの最小発射数
			p.burstMax = 4;			// [shots] 1バーストの最大発射数（ランダムで[min,max]）
			p.spreadNearDeg = 0.6f; // [deg] 近距離での照準ブレ（小さいほど正確）
			p.spreadFarDeg = 2.4f;  // [deg] 遠距離での照準ブレ（大きいほど外れやすい）
			p.bulletSpeed = 95.0f;  // [m/s] 弾速（速いほど当てやすい・すり抜け注意）
			p.bulletLifeSec = 2.2f; // [sec] 弾の寿命（有効射程 ≒ bulletSpeed * bulletLifeSec）
			p.bulletDamage = 1;     // [hp] 1発のダメージ（プレイヤーHP/防具とバランス調整）
			break;

		case EnemyArchetype::SMGFlanker:
			p.moveSpeed = 3.8f;	 		// [m/s] 移動速度（フランカーは高めがそれっぽい）
			p.attackRange = 34.0f;		// [m] 近距離で撃ち始める距離（短いほど詰めてくる）
			p.viewRange = 26.0f;		// [m] 視認距離（短め＝近距離で気づくタイプ）
			p.fireInterval = 0.10f;		// [sec] 連射間隔（小さいほどレートが高い）
			p.burstMin = 7;				// [shots] 1バースト最小（SMGはバースト長め）
			p.burstMax = 12;			// [shots] 1バースト最大
			p.strafeSpeedMul = 1.10f;	// [ratio] 横移動速度倍率（>1でストレイフが鋭い）
			p.aimMoveMul = 0.85f;		// [ratio] エイム中の移動倍率（撃つ前の移動の鈍化）
			p.burstMoveMul = 0.65f;		// [ratio] バースト中の移動倍率（高いほど撃ちながら動く）
			p.spreadNearDeg = 1.2f;		// [deg] 近距離でもブレ多め（SMGらしさ）
			p.spreadFarDeg = 5.5f;		// [deg] 遠距離で大きく外す（遠距離弱い）
			p.reactionDelaySec = 0.15f; // [sec] 発見してから撃ち始めるまでの反応遅延（短い＝即応）
			p.bulletSpeed = 105.0f;		// [m/s] 弾速（速めにすると近距離でも当てやすい）
			p.bulletLifeSec = 1.6f;		// [sec] 弾寿命（短め＝遠距離には届きにくい）
			p.bulletDamage = 1;			// [hp] 1発ダメージ（レートが高いので控えめ推奨）
			break;

		case EnemyArchetype::Sniper:
			p.moveSpeed = 2.4f;			 // [m/s] 移動速度（スナは控えめ＝陣取りやすい）
			p.attackRange = 64.0f;		 // [m] 射撃開始距離（遠距離から撃ってくる）
			p.viewRange = 55.0f;		 // [m] 視認距離（attackRangeより長くして“見えるが届かない”を防ぐ）
			p.fireInterval = 1.15f;		 // [sec] 発射間隔（大きい＝低連射）
			p.burstMin = 1;				 // [shots] 単発
			p.burstMax = 1;				 // [shots] 単発
			p.strafeSpeedMul = 0.30f;	 // [ratio] 横移動をほぼしない（陣取るタイプ）
			p.aimMoveMul = 0.20f;		 // [ratio] エイム中の移動倍率（小さい＝撃つとき止まり気味）
			p.burstMoveMul = 0.0f;		 // [ratio] 発射中の移動倍率（0で完全停止に近い）
			p.preferredMinRatio = 0.65f; // [ratio] 取りたい距離（attackRangeに対する最小距離割合）
			p.preferredMaxRatio = 0.90f; // [ratio] 取りたい距離（attackRangeに対する最大距離割合）
			p.spreadNearDeg = 0.25f;	 // [deg] 近距離でも精度高い
			p.spreadFarDeg = 0.55f;		 // [deg] 遠距離でも精度維持（小さいほど“スナっぽい”）
			p.reactionDelaySec = 0.55f;	 // [sec] 反応遅延（長め＝覗いてから撃つ感じ）
			p.bulletSpeed = 160.0f;		 // [m/s] 弾速（高速＝偏差撃ち不要/当てやすい）
			p.bulletLifeSec = 3.2f;		 // [sec] 弾寿命（長め＝遠距離でも届く）
			p.bulletDamage = 2;			 // [hp] 1発ダメージ（単発なので高めでもバランス取りやすい）
			break;
		}
		return p;
	}

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

	inline float Dot(const Vector3& a, const Vector3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}
}

void Enemy::Initialize(const K4E::Vector3& startPos, const std::string& modelPath)
{
	EnemyBase::Initialize(startPos, modelPath);

	// アーキタイプ設定（Spawn側で事前に SetArchetype しててもOK：ここで同じ値を反映するだけ）
	SetArchetype(archetype_);

	homePos_ = startPos;

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

void Enemy::SetArchetype(EnemyArchetype t)
{
	archetype_ = t;
	tuning_ = MakeTuning(t);

	// 既存メンバへ反映（EnemyStateMachine は GetTuning() を使うが、他の処理はメンバ参照しているため）
	moveSpeed_ = tuning_.moveSpeed;
	attackRange_ = tuning_.attackRange;
	fireInterval_ = tuning_.fireInterval;
	viewRange_ = tuning_.viewRange;

	bulletSpeed_ = tuning_.bulletSpeed;
	bulletLifeSec_ = tuning_.bulletLifeSec;
}


void Enemy::Update(float dt)
{
	if (IsRemovable()) return;
	if (IsDead())
	{
		EnemyBase::Update(dt);
		return;
	}

	EnemyAICommand cmd{};
	cmd.Clear();

	EnemyAIContext<Enemy> ctx{ *this, cmd, dt };
	BuildContext(ctx);

	if (stunRequestedSec_ > 0.0f)
	{
		fsm_.Force(EnemyStateId::Stunned, ctx);
	}

	fsm_.Update(ctx);
	ApplyAICommand(cmd);
	EnemyBase::Update(dt);
}

void Enemy::Draw()
{
	EnemyBase::Draw();
	DrawVisionWire();
}

void Enemy::DrawImGui()
{

}

void Enemy::BuildContext(EnemyAIContext<Enemy>& ctx)
{
	ctx.canSeePlayer = false;
	ctx.canShootPlayer = false;
	ctx.distToPlayer = 999999.0f;
	ctx.playerPos = { 0.0f,0.0f,0.0f };

	if (target_)
	{
		ctx.playerPos = target_->GetCenterPosition();

		// ★攻撃/追跡は水平距離(XZ)で統一（段差でAttackに入れない事故を防ぐ）
		auto d = ctx.playerPos - GetCenterPosition();
		const float distXZ = std::sqrt(d.x * d.x + d.z * d.z);
		ctx.distToPlayer = distXZ;

		// 視覚（FOV+縦FOV+LOS）
		ctx.canSeePlayer = CanSeeTarget(ctx.playerPos, distXZ);

		// 射線（マズル→ターゲット）。collisionManager_未注入なら常に撃てる扱い
		ctx.canShootPlayer = CanShootTarget(ctx.playerPos);
	}
	else
	{
		// ターゲットなし
		ctx.canShootPlayer = false;
	}

	// debug用（任意）
	lastCanSee_ = ctx.canSeePlayer;
	lastPlayerPos_ = ctx.playerPos;

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

bool Enemy::CanShootTarget(const K4E::Vector3& targetPos) const
{
	if (!useLOS_ || !collisionManager_) return true;

	K4E::Vector3 origin = GetCenterPosition();
	origin.y += muzzleHeight_;

	K4E::Vector3 target = targetPos;
	target.y += targetEyeHeight_;

	K4E::Segment seg{};
	seg.origin = origin;
	seg.diff = target - origin;

	// true = 何かに当たった
	return !collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kWorld), seg);
}


bool Enemy::CanSeeTarget(const K4E::Vector3& targetPos, float distToTarget)
{
	if (distToTarget > viewRange_) return false;

	// 目の位置
	K4E::Vector3 origin = GetCenterPosition();
	origin.y += eyeHeight_;

	// ターゲット側も高さを合わせる（頭/胸を狙う感じ）
	K4E::Vector3 target = targetPos;
	target.y += targetEyeHeight_;

	K4E::Vector3 to = target - origin;

	// --- 横FOV（XZ）---
	K4E::Vector3 toXZ = to;
	toXZ.y = 0.0f;

	const float lenXZ = std::sqrt(toXZ.x * toXZ.x + toXZ.z * toXZ.z);
	if (lenXZ > 1e-6f)
	{
		const float halfH = (viewFovDeg_ * 3.14159265f / 180.0f) * 0.5f;
		const float cosHalfH = std::cosf(halfH);

		K4E::Vector3 forwardXZ = { std::sinf(yawRad_), 0.0f, std::cosf(yawRad_) };

		// normalize（簡易）
		const float invTo = 1.0f / lenXZ;
		K4E::Vector3 nTo = { toXZ.x * invTo, 0.0f, toXZ.z * invTo };

		const float fwLen = std::sqrt(forwardXZ.x * forwardXZ.x + forwardXZ.z * forwardXZ.z);
		const float invFw = (fwLen > 1e-6f) ? (1.0f / fwLen) : 0.0f;
		K4E::Vector3 nFw = { forwardXZ.x * invFw, 0.0f, forwardXZ.z * invFw };

		const float dotH = nFw.x * nTo.x + nFw.z * nTo.z;
		if (dotH < cosHalfH) return false;
	}

	// --- 縦FOV（pitch）---
	{
		const float halfV = (viewFovVerticalDeg_ * 3.14159265f / 180.0f) * 0.5f;

		// ターゲットへのpitch
		const float pitchTo = std::atan2(to.y, std::max(1e-6f, lenXZ));

		// 敵の視線pitch（敵が水平なら 0 でOK。FaceToで更新してるなら pitchRad_）
		const float pitchForward = pitchRad_;

		if (std::fabs(pitchTo - pitchForward) > halfV) return false;
	}

	// --- 遮蔽物（LOS）---
	if (useLOS_ && collisionManager_)
	{
		K4E::Segment seg{};
		seg.origin = origin;
		seg.diff = target - origin;

		if (collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kWorld), seg))
		{
			return false;
		}
	}

	return true;
}

void Enemy::DrawVisionWire() const
{
	if (!debugDrawVision_) return;
	if (IsDead() || IsRemovable()) return;

	auto* wf = K4E::Wireframe::GetInstance();
	if (!wf) return;

	const K4E::Vector3 origin = {
		GetCenterPosition().x,
		GetCenterPosition().y + eyeHeight_,
		GetCenterPosition().z
	};

	const float halfH = (viewFovDeg_ * 3.14159265f / 180.0f) * 0.5f;
	const float halfV = (viewFovVerticalDeg_ * 3.14159265f / 180.0f) * 0.5f;

	const int segN = std::max(3, debugVisionSegments_);
	const float yawStart = yawRad_ - halfH;
	const float yawEnd = yawRad_ + halfH;

	auto DirFromYawPitch = [](float yaw, float pitch) -> K4E::Vector3 {
		const float cp = std::cosf(pitch);
		return { std::sinf(yaw) * cp, std::sinf(pitch), std::cosf(yaw) * cp };
		};

	const float pitchTop = pitchRad_ + halfV;
	const float pitchBot = pitchRad_ - halfV;

	const K4E::Vector4 col = lastCanSee_ ? K4E::Vector4{ 1,0,0,1 } : K4E::Vector4{ 0,1,0,1 };

	// 最初の点
	K4E::Vector3 topPrev = origin + DirFromYawPitch(yawStart, pitchTop) * viewRange_;
	K4E::Vector3 botPrev = origin + DirFromYawPitch(yawStart, pitchBot) * viewRange_;

	// 境界線（左端）
	wf->DrawLine(origin, topPrev, col);
	wf->DrawLine(origin, botPrev, col);
	wf->DrawLine(topPrev, botPrev, col); // 左端の縦線

	// 弧（上面/下面）＋縦連結
	for (int i = 1; i <= segN; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(segN);
		const float yaw = yawStart + (yawEnd - yawStart) * t;

		K4E::Vector3 top = origin + DirFromYawPitch(yaw, pitchTop) * viewRange_;
		K4E::Vector3 bot = origin + DirFromYawPitch(yaw, pitchBot) * viewRange_;

		// 上面弧、下面弧
		wf->DrawLine(topPrev, top, col);
		wf->DrawLine(botPrev, bot, col);

		// 分割ごとの縦線
		wf->DrawLine(top, bot, col);

		topPrev = top;
		botPrev = bot;
	}

	// 境界線（右端）
	wf->DrawLine(origin, topPrev, col);
	wf->DrawLine(origin, botPrev, col);

	// 任意：敵→プレイヤー線
	wf->DrawLine(origin, lastPlayerPos_, K4E::Vector4{ 1,1,0,1 });
}

void Enemy::ApplyAICommand(const EnemyAICommand& cmd)
{
	if (cmd.stopMove)
	{
		StopMove();
	}
	else if (cmd.moveDir)
	{
		// ★GunAI等：そのフレームの移動方向
		K4E::Vector3 dir = *cmd.moveDir;
		dir.y = 0.0f;

		const Vector3 n = NormalizeSafe(dir);
		const float spd = cmd.moveSpeed.value_or(moveSpeed_);
		SetVelocity(n * spd);
	}
	else if (cmd.moveGoal)
	{
		// ★通常：目標地点へ移動
		MoveTowards(*cmd.moveGoal);
	}
	else
	{
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

	// 視覚用：目の高さ基準で見上げ/見下ろしを作るなら y も使う
	const float lenXZ = std::sqrt(d.x * d.x + d.z * d.z);
	if (lenXZ <= 1e-6f) return;

	yawRad_ = std::atan2(d.x, d.z);

	// pitch：上が+。敵が水平なら pitchRad_ は 0 のままでも良い
	pitchRad_ = std::atan2(d.y, lenXZ);

	// 見た目の回転は yaw だけ（必要なら pitch も渡してOK）
	SetOrientation({ 0.0f, yawRad_, 0.0f });
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
