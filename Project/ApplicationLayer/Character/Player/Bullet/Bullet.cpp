#include "Bullet.h"
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

void Bullet::Initialize(const K4E::Vector3& startPos,
	const K4E::Vector3& velocity,
	int damage,
	float lifeTimeSec)
{
	damage_ = damage;
	moveVelocity_ = velocity;
	lifeTimeSec_ = lifeTimeSec;
	lifeTimer_ = 0.0f;

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));
	Collider::SetOwner(this);

	// デバッグ表示したいなら
	model_ = std::make_unique<K4E::Object3D>();
	model_->Initialize("cube.gltf");

	// セグメント判定が主なので OBB は小さめでOK
	Collider::SetOBBHalfSize({ 0.1f, 0.1f, 0.1f });

	prevPos_ = startPos;
	Collider::SetCenterPosition(startPos);
	if (model_) {
		model_->SetTranslate(startPos);
		model_->SetColor(debugColor_);
		model_->Update();
	}

	// 初期セグメント長さ0
	K4E::Segment seg{};
	seg.origin = startPos;
	seg.diff = { 0.0f, 0.0f, 0.0f };
	Collider::SetSegment(seg);

	isDead_ = false;
	removable_ = false;
	deadFrames_ = 0;
	contactRecord_.Clear();
}

void Bullet::KillAndMoveFar()
{
	// Exit 解決用に 1フレーム残す
	isDead_ = true;
	deadFrames_ = 0;

	const K4E::Vector3 far_ = { 1e9f, 1e9f, 1e9f };
	Collider::SetCenterPosition(far_);
	if (model_) model_->SetTranslate(far_);

	K4E::Segment s{};
	s.origin = far_;
	s.diff = { 0.0f, 0.0f, 0.0f };
	Collider::SetSegment(s);

	if (model_) model_->Update();
}

void Bullet::Update(float dt)
{
	if (removable_) return;

	// 死亡済み：Exit 解決のため 1フレーム残す
	if (isDead_)
	{
		++deadFrames_;
		if (deadFrames_ >= 2) removable_ = true;
		return;
	}

	lifeTimer_ += dt;
	if (lifeTimer_ >= lifeTimeSec_)
	{
		KillAndMoveFar();
		return;
	}

	const K4E::Vector3 current = GetCenterPosition();
	const K4E::Vector3 delta = moveVelocity_ * dt;
	const K4E::Vector3 next = current + delta;

	// このフレームの移動分を Segment にする（すり抜け防止）
	K4E::Segment seg{};
	seg.origin = current;
	seg.diff = delta;
	SetSegment(seg);

	prevPos_ = current;
	Collider::SetCenterPosition(next);
	if (model_) model_->SetTranslate(next);

	// ざっくり範囲外で消す（必要なら後で world bounds に置換）
	if (next.x > 1000.0f || next.x < -1000.0f || next.z > 1000.0f || next.z < -1000.0f)
	{
		KillAndMoveFar();
		return;
	}

	if (model_) model_->Update();
}

void Bullet::Draw()
{
	if (removable_) return;
	if (model_) model_->Draw(); // デバッグ用に見たいならON
}

void Bullet::DrawImGui()
{
	if (model_) model_->DrawImGui();
}

void Bullet::OnCollisionEnter(K4E::Collider* other)
{
	if (!other) return;
	if (isDead_ || removable_) return;

	const uint32_t otherType = other->GetTypeID();
	if (otherType != static_cast<uint32_t>(CollisionTypeIdDef::kEnemy) &&
		otherType != static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
	{
		return;
	}

	// 多段ヒット防止（基本は当たったら即死なので保険）
	const uint32_t otherId = other->GetUniqueID();
	if (contactRecord_.Check(otherId)) return;
	contactRecord_.Add(otherId);

	// ここで「ダメージ適用」は、今は一旦入れなくてOK
	// おすすめは “敵側の OnCollisionEnter が Bullet を見て TakeDamageする” 方式
	// 例：敵側で `if (auto* b = other->GetOwner<Bullet>()) TakeDamage(b->GetDamage());`

	KillAndMoveFar();
}