#include "Bullet.h"
#include <CollisionTypeIdDef.h>
#include <ScoreManager.h>
#include <Wireframe.h>
#include <Player.h>
#include <Crosshair.h>
#include <ParticleManager.h>
#include <Boss.h>
#include <Enemy.h>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void Bullet::Initialize()
{
	// コライダーを初期化
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));

	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");
	model_->SetScale({ 0.001f,0.001f,0.001f });

	// ★ 初期位置に設定
	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();  // ★ モデル行列の初期化

	// 初期位置を前回位置として記録（重要）
	previousPosition_ = position_;

	static bool s_registerd = false;
	if (!s_registerd)
	{
		ParticleManager::GetInstance()->CreateParticleGroup("BloodEffect", "circle2.png", ParticleEffectType::Blood);
		ParticleManager::GetInstance()->CreateParticleGroup("FlashEffect", "flash.png", ParticleEffectType::Flash);
		ParticleManager::GetInstance()->CreateParticleGroup("SparkEffect", "spark.png", ParticleEffectType::Spark);
		ParticleManager::GetInstance()->CreateParticleGroup("SmokeEffect", "smoke.png", ParticleEffectType::Smoke);
		ParticleManager::GetInstance()->CreateParticleGroup("RingEffect", "gradationLine.png", ParticleEffectType::Ring);
		ParticleManager::GetInstance()->CreateParticleGroup("ExplosionEffect", "spark.png", ParticleEffectType::Explosion);
		s_registerd = true;
	}
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void Bullet::Update()
{
	// 位置更新前に記録
	previousPosition_ = position_;
	position_ += velocity_;

	// 飛距離計算
	distanceTraveled_ += Vector3::Length(velocity_);

	// セグメント更新（マージンを追加して通過を防ぐ）
	Vector3 direction = position_ - previousPosition_;
	Vector3 normalized = Vector3::Normalize(direction);
	float margin = 0.2f;
	segment_.origin = previousPosition_;
	segment_.diff = direction + normalized * margin;

	// 描画・当たり判定更新
	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();

	SetCenterPosition(position_);
	SetSegment(segment_);

	if (distanceTraveled_ >= maxDistance_)
	{
		isDead_ = true;
		return;
	}
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void Bullet::Draw()
{
	// 衝突したら描画しない
	if (!isDead_ && distanceTraveled_ < maxDistance_) {
		model_->Draw();
	}
}

void Bullet::Simulate()
{
	previousPosition_ = position_;
	position_ += velocity_;
	distanceTraveled_ += Vector3::Length(velocity_);
	Vector3 dir = position_ - previousPosition_;
	Vector3 n = Vector3::Normalize(dir);
	float margin = 0.2f;
	segment_.origin = previousPosition_;
	segment_.diff = dir + n * margin;
	if (distanceTraveled_ >= maxDistance_) isDead_ = true;
}


void Bullet::Commit()
{
	if (isDead_) return;
	model_->SetTranslate(position_);
	model_->SetRotate({ 0,0,0 });
	model_->Update();               // ← D3D操作はここだけ
	SetCenterPosition(position_);
	SetSegment(segment_);
}

/// -------------------------------------------------------------
///				　			衝突処理
/// -------------------------------------------------------------
void Bullet::OnCollision(Collider* other)
{
	// ★ すでにヒット確定していたら何もしない（同フレーム多重ヒット防止）
	if (isDead_) return;
	// 衝突相手が nullptrの場合は処理をスキップ
	if (other == nullptr) return;

	// 衝突相手が「敵系」以外なら無視 
	if (other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kBoss) &&
		other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) return;

	// 衝突相手のユニークIDを取得
	uint32_t targetID = other->GetUniqueID();

	// すでに当たった相手かどうかを確認
	if (contactRecord_.Check(targetID)) return; // すでに当たった相手なので無視

	contactRecord_.Add(targetID); // 初めて当たった相手として記録

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBoss))        // ★ 追加
	{
		if (auto boss = other->GetOwner<Boss>()) {
			boss->TakeDamage(GetDamage());
			ScoreManager::GetInstance()->AddScore(100);
		}
	}

	// 敵にダメージを与える
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
	{
		if (auto enemy = other->GetOwner<Enemy>())
		{
			enemy->TakeDamage(GetDamage());

			// ノックバックを与える
			const Vector3 knockbackDir = Vector3::Normalize(velocity_);
			const float knockbackPower = std::clamp(GetDamage() * 0.01f, 0.4f, 1.5f);
			enemy->ApplyKnockback(knockbackDir, knockbackPower);

			ScoreManager::GetInstance()->AddScore(100);
		}
	}

	// 🔽 ヒットマーカー通知
	if (player_) if (auto ch = player_->GetCrosshair()) ch->ShowHitMarker();


	// パーティクルを表示（仮演出）
	// ヒット位置
	Vector3 hitPos = position_;

	// 血飛沫
	//ParticleManager::GetInstance()->Emit("BloodEffect", hitPos, 15, ParticleEffectType::Blood);

	//// フラッシュ
	//ParticleManager::GetInstance()->Emit("FlashEffect", hitPos, 1, ParticleEffectType::Flash);

	//// 火花
	//ParticleManager::GetInstance()->Emit("SparkEffect", hitPos, 8, ParticleEffectType::Spark);

	//// 煙
	//ParticleManager::GetInstance()->Emit("SmokeEffect", hitPos, 3, ParticleEffectType::Smoke);

	// 円形波紋
	ParticleManager::GetInstance()->Emit("RingEffect", hitPos, 1, ParticleEffectType::Ring);

	//// 破片（軽め）
	//ParticleManager::GetInstance()->Emit("ExplosionEffect", hitPos, 5, ParticleEffectType::Explosion);

	isDead_ = true; // 単発弾の場合
	velocity_ = { 0,0,0 };
	segment_.origin = { 0,0,0 };
	segment_.diff = { 0,0,0 };
}
