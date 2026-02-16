#include "EnemyBase.h"
#include <cmath>
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void EnemyBase::Initialize(const Vector3& startPos, const std::string& modelPath)
{
	// Collider設定
	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
	SetOwner(this);

	SetOBBHalfSize(obbHalf_);
	SetSegment(Segment{});

	model_ = std::make_unique<Object3D>();
	model_->Initialize(modelPath);
	SetColor(baseColor_);
	hitFlashTimer_ = 0.0f;

	SetCenterPosition(startPos);

	isDead_ = false;
	removable_ = false;
	deadFrames_ = 0;
	hp_ = maxHp_;
}

/// -------------------------------------------------------------
///							中心座標
/// -------------------------------------------------------------
void EnemyBase::SetCenterPosition(const Vector3& pos)
{
	Collider::SetCenterPosition(pos);
	if (model_)
	{
		model_->SetTranslate(pos);
		model_->Update();
	}
}

/// -------------------------------------------------------------
///							位置
/// -------------------------------------------------------------
void EnemyBase::SetPosition(const Vector3& p)
{
	SetCenterPosition(p);
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void EnemyBase::Update(float dt)
{
	if (removable_) return;

	if (isDead_)
	{
		++deadFrames_;
		if (deadFrames_ >= 2) removable_ = true;
		return;
	}

	if (useGravity_) velocity_.y -= gravity_ * dt;

	Vector3 pos = GetCenterPosition();
	pos = pos + velocity_ * dt;
	SetCenterPosition(pos);
	UpdateHitFlash(dt);
}

/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void EnemyBase::Draw()
{
	if (isDead_ || removable_) return;
	if (model_) model_->Draw();
}

/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void EnemyBase::DrawImGui()
{
	if (model_) model_->DrawImGui();
}

/// -------------------------------------------------------------
///							ダメージ処理
/// -------------------------------------------------------------
void EnemyBase::TakeDamage(int amount)
{
	if (isDead_) return;

	StartHitFlash();
	hp_ -= amount;
	if (hp_ <= 0)
	{
		hp_ = 0;
		isDead_ = true;
		deadFrames_ = 0;
		OnKilled();
		DisableColliderAndMoveFar();
	}
}

/// -------------------------------------------------------------
///							死亡処理
/// -------------------------------------------------------------
void EnemyBase::OnKilled()
{
	// 派生で死亡演出を入れたいならここ
}

/// -------------------------------------------------------------
/// 						コライダー無効化
/// -------------------------------------------------------------
void EnemyBase::DisableColliderAndMoveFar()
{
	// OBB枠と判定を消す
	SetOBBHalfSize({ 0.0f, 0.0f, 0.0f });

	Segment s{};
	s.origin = { 0,0,0 };
	s.diff = { 0,0,0 };
	SetSegment(s);

	const Vector3 far_ = { 1e9f, 1e9f, 1e9f };
	Collider::SetCenterPosition(far_);
	if (model_)
	{
		model_->SetTranslate(far_);
		model_->Update();
	}
}

/// -------------------------------------------------------------
///						ヒットフラッシュ開始
/// -------------------------------------------------------------
void EnemyBase::StartHitFlash()
{
	if (!hitFlashEnabled_) return;
	hitFlashTimer_ = hitFlashDuration_;
}

/// -------------------------------------------------------------
///						ヒットフラッシュ更新
/// -------------------------------------------------------------
void EnemyBase::UpdateHitFlash(float dt)
{
	if (!model_) return;

	if (hitFlashTimer_ > 0.0f)
	{
		hitFlashTimer_ -= dt;
		if (hitFlashTimer_ < 0.0f) hitFlashTimer_ = 0.0f;

		// 0..1 (1=開始直後, 0=終了)
		const float t = (hitFlashDuration_ > 0.0f) ? (hitFlashTimer_ / hitFlashDuration_) : 0.0f;

		// 点滅（sin）+ フェードアウト
		const float elapsed = hitFlashDuration_ - hitFlashTimer_;
		const float phase = elapsed * hitFlashFrequencyHz_ * 6.28318530718f; // 2π
		const float blink = 0.5f * (1.0f + std::sinf(phase)); // 0..1
		const float a = blink * t; // 0..1

		K4E::Vector4 c{};
		c.x = baseColor_.x + (hitFlashColor_.x - baseColor_.x) * a;
		c.y = baseColor_.y + (hitFlashColor_.y - baseColor_.y) * a;
		c.z = baseColor_.z + (hitFlashColor_.z - baseColor_.z) * a;
		c.w = baseColor_.w + (hitFlashColor_.w - baseColor_.w) * a;

		model_->SetColor(c);
	}
	else
	{
		// 元の色に戻す
		model_->SetColor(baseColor_);
	}
}

/// -------------------------------------------------------------
/// 						弾丸ヒット処理
/// -------------------------------------------------------------
void EnemyBase::OnBulletHit(Collider* bulletCollider)
{
	(void)bulletCollider;
	TakeDamage(10);
}

/// -------------------------------------------------------------
///							衝突開始
/// -------------------------------------------------------------
void EnemyBase::OnCollisionEnter(Collider* other)
{
	if (!other || isDead_) return;

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		OnBulletHit(other);
	}
}