#include "Bullet.h"
#include <CollisionTypeIdDef.h>
#include <ScoreManager.h>

void Bullet::Initialize()
{
	// コライダーを初期化
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));
	Collider::SetOBBHalfSize({ 0.5f, 0.5f, 0.5f }); // OBBの半径を設定


	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");
	model_->SetScale({ 0.5f,0.5f,0.5f });
}

void Bullet::Update()
{
	// 位置更新
	position_ += velocity_;

	// 寿命を減らす
	lifeTime_ -= 1.0f / 60.0f;

	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();

	// Collider の位置・回転を更新
	Collider::SetCenterPosition(position_);
}

void Bullet::Draw()
{
	// 衝突したら描画しない
	if (!isDead_) model_->Draw();
}

void Bullet::OnCollision(Collider* other)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
	{
		isDead_ = true; // または寿命をゼロにするなど

		// スコアを加算
		ScoreManager::GetInstance()->AddKill();
	}
}
