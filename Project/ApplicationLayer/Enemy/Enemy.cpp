#include "Enemy.h"
#include "Player.h"
#include <imgui.h>
#include <CollisionTypeIdDef.h>
#include <ParticleManager.h>

void Enemy::Initialize()
{
	// 基底クラスの初期化
	BaseCharacter::Initialize();

	// エネミーのコライダーを初期化
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
	Collider::SetOBBHalfSize({ 2.5f, 6.0f, 2.5f }); // OBBの半径を設定

	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("body.gltf");
	body_.worldTransform_.scale_ = { 3.0f, 3.0f, 3.0f };
	body_.worldTransform_.translate_ = { 0.0f, 0.0f, 200.0f };
}

void Enemy::Update()
{
	if (isDead_) return; // 死亡済みなら更新スキップ（描画も停止する）

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

	// コライダーに座標・回転を反映
	Collider::SetCenterPosition(body_.worldTransform_.translate_ + Vector3(0.0f, 8.2f, 0.0f));

	body_.object->SetScale(body_.worldTransform_.scale_);
	body_.object->SetTranslate(body_.worldTransform_.translate_);
	body_.object->SetRotate(body_.worldTransform_.rotate_);

	BaseCharacter::Update();
}

void Enemy::Draw()
{
	//body_.object->Draw();
	if (IsDead()) return; // 死亡済みなら描画スキップ
	BaseCharacter::Draw();
}

void Enemy::DrawImGui()
{
	Collider::DrawImGui();
	ImGui::Text("HP: %.1f", hp_);
	ImGui::Checkbox("IsDead", &isDead_);
}

void Enemy::OnCollision(Collider* other)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		// パーティクルを表示（仮演出）
		TakeDamage(50.0f);
	}
}
