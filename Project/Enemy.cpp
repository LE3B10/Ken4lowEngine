#include "Enemy.h"
#include "Player.h"

void Enemy::Initialize()
{
	// 基底クラスの初期化
	BaseCharacter::Initialize();

	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("body.gltf");
	body_.worldTransform_.scale_ = { 3.0f, 3.0f, 3.0f };
	body_.worldTransform_.translate_ = { 0.0f, 0.0f, 200.0f };
}

void Enemy::Update()
{
    if (player_)
    {
        Vector3 toPlayer = player_->GetWorldPosition() - body_.worldTransform_.translate_;
        float distance = Vector3::Length(toPlayer);

        // 距離5以上なら追いかける
        if (distance > 5.0f)
        {
            Vector3 direction = Vector3::Normalize(toPlayer);
            body_.worldTransform_.translate_ += direction * 0.1f;

            // 回転も補正（Z+が前提）
            float targetYaw = std::atan2(-direction.x, direction.z);
            body_.worldTransform_.rotate_.y = Vector3::LerpAngle(body_.worldTransform_.rotate_.y, targetYaw, 0.1f);
        }
    }

    body_.object->SetScale(body_.worldTransform_.scale_);
    body_.object->SetTranslate(body_.worldTransform_.translate_);
    body_.object->SetRotate(body_.worldTransform_.rotate_);

	BaseCharacter::Update();
}

void Enemy::Draw()
{
	//body_.object->Draw();
	BaseCharacter::Draw();
}
