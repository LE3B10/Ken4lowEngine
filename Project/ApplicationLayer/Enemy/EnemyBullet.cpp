#include "EnemyBullet.h"
#include <CollisionTypeIdDef.h>
#include <Player.h>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void EnemyBullet::Initialize()
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet)); // 新たなIDを定義
	Collider::SetOBBHalfSize({ 0.5f, 0.5f, 0.5f });

	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf"); // 敵弾用の異なるモデルにするなら
	model_->SetScale({ 0.5f, 0.5f, 0.5f });
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void EnemyBullet::Update()
{
	position_ += velocity_ * 8.0f;
	lifeTime_ -= 1.0f / 60.0f;

	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();

	Collider::SetCenterPosition(position_);
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void EnemyBullet::Draw()
{
	if (!isDead_) model_->Draw();
}


/// -------------------------------------------------------------
///				　			衝突処理
/// -------------------------------------------------------------
void EnemyBullet::OnCollision(Collider* other)
{
	// 衝突相手がプレイヤーかどうかを確認
	if (other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) return;

	// 衝突相手のユニークIDを取得
	uint32_t targetID = other->GetUniqueID();

	// すでに当たった相手かどうかを確認
	if (contactRecord_.Check(targetID)) {
		return; // 多段ヒット防止
	}

	// 初めて当たった相手として記録
	contactRecord_.Add(targetID);

	// プレイヤーにダメージを与える
	Player* player = dynamic_cast<Player*>(other);

	// プレイヤーが存在する場合、ダメージを与える
	if (player) {
		player->TakeDamage(25.0f); // 任意のダメージ
	}

	// パーティクルを表示（仮演出）

	// 死亡フラグを立てる
	isDead_ = true;
}
