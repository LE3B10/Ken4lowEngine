#include "EnemyBullet.h"
#include <CollisionTypeIdDef.h>
#include <Player.h>

void EnemyBullet::Initialize()
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet)); // 新たなIDを定義
	Collider::SetOBBHalfSize({ 0.5f, 0.5f, 0.5f });

	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf"); // 敵弾用の異なるモデルにするなら
	model_->SetScale({ 0.5f, 0.5f, 0.5f });
}

void EnemyBullet::Update()
{
	position_ += velocity_ * 8.0f;
	lifeTime_ -= 1.0f / 60.0f;

	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();

	Collider::SetCenterPosition(position_);
}

void EnemyBullet::Draw()
{
	if (!isDead_) model_->Draw();
}

void EnemyBullet::OnCollision(Collider* other)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		isDead_ = true;

		// プレイヤーにダメージを与える処理などもここに追加
		Player* player = dynamic_cast<Player*>(other);
		if (player)
		{
			player->TakeDamage(25.0f); // 例：25ダメージ
		}
	}
}
