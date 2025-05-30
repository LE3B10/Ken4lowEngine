#include "Enemy.h"
#include "Player.h"
#include <imgui.h>
#include <CollisionTypeIdDef.h>
#include <ParticleManager.h>
#include "EnemyIdleState.h"
#include "EnemyChaseState.h"
#include "TankIdleState.h"
#include "BossIdleState.h"
#include "SniperIdleState.h"

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
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
	
	switch (type_) {
	case EnemyType::Basic: // 基本的な敵
		ChangeState(std::make_unique<EnemyIdleState>());
		break;
	case EnemyType::Sniper:
		ChangeState(std::make_unique<SniperIdleState>());
		currentState_->Enter(this); // 初期化時にEnterを呼ぶ
		break;
	case EnemyType::Tank:
		// タンク専用ステートを作る想定
		ChangeState(std::make_unique<TankIdleState>());
		currentState_->Enter(this); // 初期化時にEnterを呼ぶ
		break;
	case EnemyType::Boss:
		ChangeState(std::make_unique<BossIdleState>());
		currentState_->Enter(this); // 初期化時にEnterを呼ぶ
		break;
	}
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void Enemy::Update()
{
	if (isDead_) return;

	// タイプ別で固有の処理を追加できる
	switch (type_)
	{
	case EnemyType::Basic:
		break;
	case EnemyType::Sniper:
		break;
	case EnemyType::Tank:
		break;
	case EnemyType::Boss:
		break;
	}

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


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
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


/// -------------------------------------------------------------
///				　			　ImGui描画処理
/// -------------------------------------------------------------
void Enemy::DrawImGui()
{
	Collider::DrawImGui();
	ImGui::Text("HP: %.1f", hp_);
	ImGui::Checkbox("IsDead", &isDead_);

	// Enemy::DrawImGui() に表示追加
	ImGui::Text("State: %s", currentStateName_.c_str());
}


/// -------------------------------------------------------------
/// 			　		ステート変更処理
/// -------------------------------------------------------------
void Enemy::ChangeState(std::unique_ptr<IEnemyState> newState)
{
	if (currentState_) currentState_->Exit(this);
	currentState_ = std::move(newState);
	if (currentState_) currentState_->Enter(this);
}


/// -------------------------------------------------------------
///				　			　リクエスト発射
/// -------------------------------------------------------------
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


/// -------------------------------------------------------------
///				　			　衝突処理
/// -------------------------------------------------------------
void Enemy::OnCollision(Collider* other)
{

}
