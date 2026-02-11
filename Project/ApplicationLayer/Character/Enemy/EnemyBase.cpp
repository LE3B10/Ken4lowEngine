#include "EnemyBase.h"
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

void EnemyBase::Initialize(const Vector3& startPos, const std::string& modelPath)
{
	// Collider設定
	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
	SetOwner(this);

	SetOBBHalfSize(obbHalf_);
	SetSegment(Segment{});

	model_ = std::make_unique<Object3D>();
	model_->Initialize(modelPath);

	SetCenterPosition(startPos);

	isDead_ = false;
	removable_ = false;
	deadFrames_ = 0;
	hp_ = maxHp_;
}

void EnemyBase::SetCenterPosition(const Vector3& pos)
{
	Collider::SetCenterPosition(pos);
	if (model_)
	{
		model_->SetTranslate(pos);
		model_->Update();
	}
}

void EnemyBase::SetPosition(const Vector3& p)
{
	SetCenterPosition(p);
}

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
}

void EnemyBase::Draw()
{
	if (isDead_ || removable_) return;
	if (model_) model_->Draw();
}

void EnemyBase::DrawImGui()
{
	if (model_) model_->DrawImGui();
}

void EnemyBase::TakeDamage(int amount)
{
	if (isDead_) return;

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

void EnemyBase::OnKilled()
{
	// 派生で死亡演出を入れたいならここ
}

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

void EnemyBase::OnBulletHit(Collider* bulletCollider)
{
	(void)bulletCollider;
	TakeDamage(10);
}

void EnemyBase::OnCollisionEnter(Collider* other)
{
	if (!other || isDead_) return;

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		OnBulletHit(other);
	}
}