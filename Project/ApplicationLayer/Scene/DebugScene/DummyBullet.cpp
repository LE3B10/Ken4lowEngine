#include "DummyBullet.h"
#include "CollisionTypeIdDef.h"
#include "DummyEnemy.h"

using namespace Ken4lowEngine;

void DummyBullet::Initialize(const K4E::Vector3& startPos, const K4E::Vector3& velocity, int damage)
{
	damage_ = damage;
	moveVelocity_ = velocity;

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));
	Collider::SetOwner(this);

	model_ = std::make_unique<K4E::Object3D>();
	model_->Initialize("cube.gltf");

	// 見やすいように少し小さめの当たり（セグメント判定が主）
	Collider::SetOBBHalfSize({ 0.1f,0.1f,0.1f });

	prevPos_ = startPos;
	Collider::SetCenterPosition(startPos);
	model_->SetTranslate(startPos);
	model_->SetColor(debugColor_);
	model_->Update();

	// 初期セグメントは長さ0（このフレームは衝突しない）
	K4E::Segment seg{};
	seg.origin = startPos;
	seg.diff = { 0.0f, 0.0f, 0.0f };
	Collider::SetSegment(seg);

	isDead_ = false;
	removable_ = false;
	deadFrames_ = 0;
	contactRecord_.Clear();
}

void DummyBullet::Update()
{
	if (removable_) return;

	// 死亡済み：Exit 解決のために 1フレーム残す
	if (isDead_)
	{
		++deadFrames_;
		if (deadFrames_ >= 2) // 1フレーム猶予 + このフレームで削除OK
		{
			removable_ = true;
		}
		return;
	}

	float dt = 1.0f / 60.0f; // 固定フレームレート想定

	const K4E::Vector3 current = GetCenterPosition();
	const K4E::Vector3 delta = moveVelocity_ * dt;   // ★ここが重要
	const K4E::Vector3 next = current + delta;

	K4E::Segment seg{};
	seg.origin = current;
	seg.diff = delta;                               
	SetSegment(seg);


	// 位置更新
	prevPos_ = current;
	Collider::SetCenterPosition(next);
	model_->SetTranslate(next);

	// 画面外に行ったら消す（適当な範囲）
	if (next.z > 100.0f || next.z < -100.0f || next.x > 100.0f || next.x < -100.0f)
	{
		isDead_ = true;
		deadFrames_ = 0;

		// Exit を出すため、次フレームは衝突しない位置へ飛ばす
		Collider::SetCenterPosition({ 1e9f, 1e9f, 1e9f });
		model_->SetTranslate({ 1e9f, 1e9f, 1e9f });

		K4E::Segment s{};
		s.origin = { 1e9f, 1e9f, 1e9f };
		s.diff = { 0,0,0 };
		Collider::SetSegment(s);
	}

	model_->Update();
}

void DummyBullet::Draw()
{
	if (removable_) return;
	//if (model_) model_->Draw();
}

void DummyBullet::DrawImGui()
{
	if (model_) model_->DrawImGui();
}

void DummyBullet::OnCollisionEnter(K4E::Collider* other)
{
	if (!other) return;
	if (isDead_ || removable_) return;

	const uint32_t otherType = other->GetTypeID();
	if (otherType != static_cast<uint32_t>(CollisionTypeIdDef::kEnemy) &&
		otherType != static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
	{
		return;
	}

	const uint32_t otherId = other->GetUniqueID();
	if (contactRecord_.Check(otherId))
	{
		return; // 同じ相手への多段ヒット防止
	}

	contactRecord_.Add(otherId);

	// ダメージ適用（Owner 経由）
	if (auto* enemy = other->GetOwner<DummyEnemy>())
	{
		enemy->TakeDamage(damage_);
	}

	// 命中で消滅（Exit 解決のために 1フレーム残す）
	isDead_ = true;
	deadFrames_ = 0;

	// 次フレームは衝突しない位置へ飛ばして Exit を確実に出す
	Collider::SetCenterPosition({ 1e9f, 1e9f, 1e9f });
	if (model_) model_->SetTranslate({ 1e9f, 1e9f, 1e9f });

	K4E::Segment s{};
	s.origin = { 1e9f, 1e9f, 1e9f };
	s.diff = { 0,0,0 };
	Collider::SetSegment(s);

	if (model_) model_->Update();
}
