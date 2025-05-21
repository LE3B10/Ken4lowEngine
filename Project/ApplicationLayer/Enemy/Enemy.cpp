#include "Enemy.h"
#include "Player.h"
#include <imgui.h>
#include <CollisionTypeIdDef.h>
#include <ParticleManager.h>
#include "EnemyIdleState.h"

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

	// 初期ステートを Idle に設定
	ChangeState(std::make_unique<EnemyIdleState>());
}

void Enemy::Update()
{
	if (isDead_) return;

	shootTimer_ += 1.0f / 60.0f; // ✅ ここで毎フレーム加算すること！

	// 弾の更新 & 削除
	for (auto it = bullets_.begin(); it != bullets_.end(); )
	{
		(*it)->Update();
		if ((*it)->IsDead()) {
			it = bullets_.erase(it);
		}
		else {
			++it;
		}
	}

	if (currentState_)
	{
		currentState_->Update(this);
	}

	// コライダーの更新
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

	// 弾描画
	for (auto& bullet : bullets_) {
		bullet->Draw();
	}
}

void Enemy::DrawImGui()
{
	Collider::DrawImGui();
	ImGui::Text("HP: %.1f", hp_);
	ImGui::Checkbox("IsDead", &isDead_);

	// Enemy::DrawImGui() に表示追加
	ImGui::Text("State: %s", currentStateName_.c_str());
}

void Enemy::ChangeState(std::unique_ptr<IEnemyState> newState)
{
	if (currentState_) currentState_->Exit(this);
	currentState_ = std::move(newState);
	if (currentState_) currentState_->Enter(this);
}

void Enemy::RequestShoot()
{
	if (shootTimer_ < shootCooldown_) return;

	shootTimer_ = 0.0f;

	auto bullet = std::make_unique<EnemyBullet>();
	bullet->Initialize();
	Vector3 muzzlePos = body_.worldTransform_.translate_ + Vector3(0.0f, 13.0f, 0.0f);
	Vector3 direction = Vector3::Normalize(player_->GetWorldPosition() - body_.worldTransform_.translate_);
	bullet->SetPosition(muzzlePos);
	bullet->SetVelocity(direction * 0.5f);

	bullets_.push_back(std::move(bullet));

	// 弾の更新 & 削除
	for (auto it = bullets_.begin(); it != bullets_.end(); )
	{
		(*it)->Update();
		if ((*it)->IsDead()) {
			it = bullets_.erase(it);
		}
		else {
			++it;
		}
	}
}

void Enemy::OnCollision(Collider* other)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		// パーティクルを表示（仮演出）
		TakeDamage(10.0f);
	}
}
