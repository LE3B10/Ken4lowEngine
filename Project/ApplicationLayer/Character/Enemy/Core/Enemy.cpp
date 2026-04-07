#define NOMINMAX
#include "Enemy.h"
#include "Bullet.h"
#include "BulletManager.h"
#include "CollisionTypeIdDef.h"
#include "CollisionManager.h"
#include "LinearInterpolation.h"
#include "EnemyTuningRepository.h"
#include "Wireframe.h"
#include "EnemyArchetypeBehaviorFactory.h"


#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	inline Vector3 NormalizeSafe(const Vector3& v)
	{
		const float len = Vector3::Length(v);
		if (len <= 1e-6f) return { 0.0f, 0.0f, 0.0f };
		return { v.x / len, v.y / len, v.z / len };
	}
}

void Enemy::Initialize(const K4E::Vector3& startPos)
{
	EnemyBase::Initialize(startPos);

	// --------------------------------------------------------
	// EnemyTuningRepository を初期化
	// - まず既定値を構築
	// - その後、敵ごとの JSON で上書き
	// --------------------------------------------------------
	EnemyTuningRepository::Initialize();

	// アーキタイプ設定
	// Spawn 側で先に SetArchetype していても、
	// ここでは現在の archetype_ をもとに反映し直すだけなので安全
	SetArchetype(archetype_);

	homePos_ = startPos;

	lastSeenPos_ = startPos;
	timeSinceSeen_ = 9999.0f;
	stunRequestedSec_ = 0.0f;

	EnemyAICommand cmd{};
	EnemyAIContext<Enemy> ctx{ *this, cmd, 0.0f };
	BuildContext(ctx);
	fsm_.Reset(ctx);
}

void Enemy::SetArchetype(EnemyArchetype t)
{
	archetype_ = t;

	// Repository から tuning を取得
	tuning_ = EnemyTuningRepository::Get(t);

	// Enemy 本体が直接使う値を反映
	moveSpeed_ = tuning_.moveSpeed;
	attackRange_ = tuning_.attackRange;
	fireInterval_ = tuning_.fireInterval;
	viewRange_ = tuning_.viewRange;

	bulletSpeed_ = tuning_.bulletSpeed;
	bulletLifeSec_ = tuning_.bulletLifeSec;
	bulletDamage_ = tuning_.bulletDamage;

	// 耐久反映
	SetMaxHp(tuning_.maxHp);

	// archetype固有ロジックを差し替え
	archetypeBehavior_ = EnemyArchetypeBehaviorFactory::Create(t);

	// tuning適用後の追加補正
	if (archetypeBehavior_)
	{
		archetypeBehavior_->OnApplyTuning(*this, tuning_);
	}
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

	// archetype固有の前補正
	if (archetypeBehavior_)
	{
		archetypeBehavior_->BeforeFSMUpdate(*this, ctx);
	}

	// 敵のタイプによって色を変える（debug用）
	switch (archetype_)
	{
	case EnemyArchetype::RifleGrunt:    SetColor({ 0.4f, 0.4f, 1.0f, 1.0f }); break;
	case EnemyArchetype::SMGFlanker:    SetColor({ 0.4f, 1.0f, 0.4f, 1.0f }); break;
	case EnemyArchetype::Sniper:        SetColor({ 1.0f, 0.4f, 0.4f, 1.0f }); break;
	case EnemyArchetype::BurstTrooper:  SetColor({ 0.2f, 0.7f, 1.0f, 1.0f }); break;
	case EnemyArchetype::HeavyRifleman: SetColor({ 0.9f, 0.7f, 0.2f, 1.0f }); break;
	case EnemyArchetype::ShotgunRusher: SetColor({ 1.0f, 0.5f, 0.1f, 1.0f }); break;
	case EnemyArchetype::Scout:         SetColor({ 0.6f, 1.0f, 0.9f, 1.0f }); break;
	case EnemyArchetype::Marksman:      SetColor({ 0.9f, 0.5f, 0.8f, 1.0f }); break;
	case EnemyArchetype::Suppressor:    SetColor({ 0.7f, 0.7f, 0.7f, 1.0f }); break;
	case EnemyArchetype::EliteFlanker:  SetColor({ 0.2f, 1.0f, 0.2f, 1.0f }); break;
	case EnemyArchetype::HeavySniper:   SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); break;
	}

	fsm_.Update(ctx);

	// archetype固有の後補正
	if (archetypeBehavior_)
	{
		archetypeBehavior_->AfterFSMUpdate(*this, cmd, dt);
	}

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
#ifdef USE_IMGUI
	EnemyBase::DrawImGui();
#endif // USE_IMGUI
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


const char* Enemy::GerArcheTypeBehaviorDebugName() const
{
	if (!archetypeBehavior_)
	{
		return "None";
	}
	return archetypeBehavior_->GetDebugName();
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

	const float lenXZ = std::sqrt(d.x * d.x + d.z * d.z);
	if (lenXZ <= 1e-6f) return;

	yawRad_ = std::atan2(d.x, d.z);
	pitchRad_ = std::atan2(d.y, lenXZ);

	SetOrientation({ 0.0f, -yawRad_, 0.0f });
}

void Enemy::FireAt(const K4E::Vector3& targetPos)
{
	if (!bulletManager_) return;

	Vector3 origin = GetCenterPosition();
	origin.y += muzzleHeight_;

	Vector3 dir = NormalizeSafe(targetPos - origin);
	if (Vector3::Length(dir) <= 1e-6f) return;

	bulletManager_->Spawn(origin, dir, bulletSpeed_, bulletDamage_, bulletLifeSec_, GetCenterPosition(), static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet));

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
	int dmg = 25;
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
