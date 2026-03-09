#define NOMINMAX
#include "Enemy.h"
#include "Bullet.h"
#include "BulletManager.h"
#include "CollisionTypeIdDef.h"
#include "CollisionManager.h"
#include "LinearInterpolation.h"
#include "Wireframe.h"


#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

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

void Enemy::Initialize(const K4E::Vector3& startPos)
{
	EnemyBase::Initialize(startPos);

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
	if (debugCamera_) return; // デバッグカメラ有効ならAI更新しない

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

	// 敵のタイプによって色を変える（debug用）
	switch (archetype_)
	{
	case EnemyArchetype::RifleGrunt: SetColor({ 0.4f, 0.4f, 1.0f, 1.0f }); break;
	case EnemyArchetype::SMGFlanker: SetColor({ 0.4f, 1.0f, 0.4f, 1.0f }); break;
	case EnemyArchetype::Sniper:     SetColor({ 1.0f, 0.4f, 0.4f, 1.0f }); break;
	}


	fsm_.Update(ctx);
	ApplyAICommand(cmd);
	EnemyBase::Update(dt);
}

void Enemy::Draw()
{
	EnemyBase::Draw();

#ifdef _DEBUG
	// デバッグ用：視覚判定の可視化
	DrawVisionWire();
#endif // _DEBUG
}

void Enemy::DrawImGui()
{

}

void Enemy::DrawShadow()
{
	EnemyBase::DrawShadow();
}

void Enemy::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
{
	EnemyBase::UpdateShadowMatrix(lightViewProjection);
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
	lastDistOk_ = false;
	lastHorizOk_ = false;
	lastVertOk_ = true;
	lastLosOk_ = true;
	lastNearBypass_ = false;

	if (distToTarget > viewRange_)
	{
		return false;
	}
	lastDistOk_ = true;

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

	const bool nearBypass = (distToTarget <= nearDetectRadius_) || (lastCanSee_ && distToTarget <= nearLoseRadius_);
	lastNearBypass_ = nearBypass;

	if (nearBypass)
	{
		lastHorizOk_ = true;
	}
	else if (lenXZ > 1e-6f)
	{
		const float halfH = (viewFovDeg_ * std::numbers::pi_v<float> / 180.0f) * 0.5f;
		const float cosHalfH = std::cosf(halfH);

		K4E::Vector3 forwardXZ = { std::sinf(yawRad_), 0.0f, std::cosf(yawRad_) };

		const float invTo = 1.0f / lenXZ;
		K4E::Vector3 nTo = { toXZ.x * invTo, 0.0f, toXZ.z * invTo };

		const float fwLen = std::sqrt(forwardXZ.x * forwardXZ.x + forwardXZ.z * forwardXZ.z);
		const float invFw = (fwLen > 1e-6f) ? (1.0f / fwLen) : 0.0f;
		K4E::Vector3 nFw = { forwardXZ.x * invFw, 0.0f, forwardXZ.z * invFw };

		const float dotH = nFw.x * nTo.x + nFw.z * nTo.z;
		lastHorizOk_ = (dotH >= cosHalfH);
		if (!lastHorizOk_) return false;
	}
	else
	{
		lastHorizOk_ = true;
	}

	// --- 縦FOV（pitch）---
	if (useVerticalFov_)
	{
		const float halfV = (viewFovVerticalDeg_ * 3.14159265f / 180.0f) * 0.5f;
		const float pitchTo = std::atan2(to.y, std::max(1e-6f, lenXZ));
		const float pitchForward = pitchRad_;
		lastVertOk_ = (std::fabs(pitchTo - pitchForward) <= halfV);
		if (!lastVertOk_) return false;
	}

	// --- 遮蔽物（LOS）---
	if (useLOS_ && collisionManager_)
	{
		K4E::Segment seg{};
		seg.origin = origin;
		seg.diff = target - origin;

		lastLosOk_ = !collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kWorld), seg);
		if (!lastLosOk_)
		{
			return false;
		}
	}
	else
	{
		lastLosOk_ = true;
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

	const float pitchTop = useVerticalFov_ ? (pitchRad_ + halfV) : 0.35f;
	const float pitchBot = useVerticalFov_ ? (pitchRad_ - halfV) : -0.35f;

	K4E::Vector4 col = { 0,1,0,1 };
	if (lastCanSee_) col = { 1,0,0,1 };
	else if (!lastLosOk_) col = { 1,0.35f,0.35f,1 };
	else if (!lastHorizOk_) col = { 1,0.65f,0.0f,1 };
	else if (!lastVertOk_) col = { 0.7f,0.4f,1.0f,1 };
	else if (!lastDistOk_) col = { 0.35f,0.7f,1.0f,1 };

	K4E::Vector3 topPrev = origin + DirFromYawPitch(yawStart, pitchTop) * viewRange_;
	K4E::Vector3 botPrev = origin + DirFromYawPitch(yawStart, pitchBot) * viewRange_;
	wf->DrawLine(origin, topPrev, col);
	wf->DrawLine(origin, botPrev, col);
	wf->DrawLine(topPrev, botPrev, col);

	for (int i = 1; i <= segN; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(segN);
		const float yaw = yawStart + (yawEnd - yawStart) * t;
		K4E::Vector3 top = origin + DirFromYawPitch(yaw, pitchTop) * viewRange_;
		K4E::Vector3 bot = origin + DirFromYawPitch(yaw, pitchBot) * viewRange_;
		wf->DrawLine(topPrev, top, col);
		wf->DrawLine(botPrev, bot, col);
		wf->DrawLine(top, bot, col);
		topPrev = top;
		botPrev = bot;
	}

	wf->DrawLine(origin, topPrev, col);
	wf->DrawLine(origin, botPrev, col);

	// 中央向き線
	const K4E::Vector3 forward = origin + DirFromYawPitch(yawRad_, useVerticalFov_ ? pitchRad_ : 0.0f) * ((lastCanSee_ || lastDistOk_) ? viewRange_ : (viewRange_ * 0.5f));
	wf->DrawLine(origin, forward, K4E::Vector4{ 0,1,1,1 });

	// 近距離全方位検知リング
	if (nearDetectRadius_ > 0.0f)
	{
		const int ringSeg = 24;
		K4E::Vector3 prev = origin + K4E::Vector3{ nearDetectRadius_, 0.0f, 0.0f };
		for (int i = 1; i <= ringSeg; ++i)
		{
			const float a = (2.0f * 3.14159265f * i) / static_cast<float>(ringSeg);
			K4E::Vector3 cur = origin + K4E::Vector3{ std::cosf(a) * nearDetectRadius_, 0.0f, std::sinf(a) * nearDetectRadius_ };
			wf->DrawLine(prev, cur, K4E::Vector4{ 0.25f,0.9f,1.0f,1 });
			prev = cur;
		}
	}

	// 敵→プレイヤー線（黄:見えてる / 橙:FOV外 / 赤:LOS遮蔽）
	K4E::Vector4 targetCol = lastCanSee_ ? K4E::Vector4{ 1,1,0,1 }
		: (!lastLosOk_ ? K4E::Vector4{ 1,0,0,1 }
			: (!lastHorizOk_ ? K4E::Vector4{ 1,0.5f,0,1 }
	: K4E::Vector4{ 0.7f,0.7f,0.7f,1 }));
	wf->DrawLine(origin, lastPlayerPos_, targetCol);
}

void Enemy::ApplyAICommand(const EnemyAICommand& cmd)
{
	if (cmd.stopMove)
	{
		StopMove();
	}
	else if (cmd.moveDir)
	{
		// GunAI等：そのフレームの移動方向（XZのみ更新）
		K4E::Vector3 dir = *cmd.moveDir;
		dir.y = 0.0f;

		const Vector3 n = NormalizeSafe(dir);
		const float spd = cmd.moveSpeed.value_or(moveSpeed_);

		// Y速度は保持して、XZだけ更新する
		K4E::Vector3 v = GetVelocity();
		v.x = n.x * spd;
		v.z = n.z * spd;
		SetVelocity(v);
	}
	else if (cmd.moveGoal)
	{
		MoveTowards(*cmd.moveGoal);
	}
	else
	{
		StopMove();
	}

	// リロード開始の瞬間だけSEを鳴らす
	if (!wasReloadingLastFrame_ && cmd.wantReload)
	{
		if (onReloadSE_)
		{
			onReloadSE_();
		}
	}
	wasReloadingLastFrame_ = cmd.wantReload;

	if (cmd.lookAt) FaceTo(*cmd.lookAt);
	if (cmd.fireAt) FireAt(*cmd.fireAt);
}


void Enemy::MoveTowards(const K4E::Vector3& goal)
{
	K4E::Vector3 to = goal - GetCenterPosition();
	to.y = 0.0f;

	const Vector3 dir = NormalizeSafe(to);

	// Y速度は保持して、XZだけ更新する
	K4E::Vector3 v = GetVelocity();
	v.x = dir.x * moveSpeed_;
	v.z = dir.z * moveSpeed_;
	SetVelocity(v);
}

void Enemy::StopMove()
{
	// 落下中でもY速度は止めない
	K4E::Vector3 v = GetVelocity();
	v.x = 0.0f;
	v.z = 0.0f;
	SetVelocity(v);
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

	bulletManager_->Spawn(origin, dir, bulletSpeed_, bulletDamage_, bulletLifeSec_, static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet));

	// 銃声
	if (onFireSE_) onFireSE_();
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
	bool isHeadshot = false;

	if (bulletCollider)
	{
		if (auto* b = bulletCollider->GetOwner<Bullet>())
		{
			dmg = b->GetDamage();
		}
	}

	const bool wasDead = IsDead();

	K4E::Vector3 hitDir{ 0.0f, 0.0f, 0.0f };
	float hitPower = 1.0f;
	if (bulletCollider)
	{
		hitDir = GetCenterPosition() - bulletCollider->GetCenterPosition();
		hitPower = 1.0f + static_cast<float>(dmg) * 0.015f;
	}

	TakeDamage(dmg, hitDir, hitPower);

	// 被弾時サウンド
	if (!wasDead && !IsDead())
	{
		if (onHitSE_)
		{
			onHitSE_();
		}
	}

	if (!wasDead && onPlayerHitUICallback_)
	{
		onPlayerHitUICallback_(isHeadshot);
	}

	if (!wasDead && IsDead())
	{
		// 死亡サウンド
		if (onDeathSE_)
		{
			onDeathSE_();
		}

		if (onPlayerKillUICallback_)
		{
			onPlayerKillUICallback_(isHeadshot);
		}
	}

	// 生きている時だけ「撃たれた相手を記憶して警戒」
	if (!IsDead())
	{
		if (target_)
		{
			lastSeenPos_ = target_->GetCenterPosition();
			timeSinceSeen_ = 0.0f;
			FaceTo(lastSeenPos_);
		}
		else if (bulletCollider)
		{
			// ターゲット未設定時の保険：弾位置の反対側をざっくり向く
			K4E::Vector3 alertPos = GetCenterPosition() + hitDir * -6.0f;
			lastSeenPos_ = alertPos;
			timeSinceSeen_ = 0.0f;
			FaceTo(alertPos);
		}

		RequestStun(0.12f);
	}
}
