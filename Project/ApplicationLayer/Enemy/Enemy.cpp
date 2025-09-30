#define NOMINMAX
#include "Enemy.h"
#include "EnemyBullet.h"
#include "Player.h"
#include <ScoreManager.h>
#include "LinearInterpolation.h"

#include "ItemManager.h"

#include <algorithm>
#include <cmath> // atan2f

std::vector<Enemy*> Enemy::sActives;

Enemy::~Enemy()
{
	// 破棄時にアクティブリストから削除
	sActives.erase(std::remove(sActives.begin(), sActives.end(), this), sActives.end());
}

void Enemy::Initialize(Player* player, const Vector3& position)
{
	player_ = player;
	hp_ = maxHp_;
	isDead_ = false;

	attackRange_ = 2.0f;
	attackCooldown_ = 1.0f;
	attackTimer_ = 0.0f;
	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));

	itemDropTable_.SetDropChance(60); // 全体ドロップ率60%
	itemDropTable_.AddEntry(ItemType::HealSmall, 30); // 小回復アイテムのドロップ率
	itemDropTable_.AddEntry(ItemType::AmmoSmall, 40); // 小弾薬アイテムのドロップ率
	itemDropTable_.AddEntry(ItemType::ScoreBonus, 20); // スコアボーナスアイテムのドロップ率
	itemDropTable_.AddEntry(ItemType::PowerUp, 10); // パワーアップアイテムのドロップ率

	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");

	// コライダー設定（半分沈み防止の基準に使う）
	SetOBBHalfSize({ 1.0f, 1.0f, 1.0f });
	halfHeight_ = 1.0f;                 // 上のSetOBBHalfSizeに合わせる
	targetGroundY_ = std::max(position.y, halfHeight_);

	// 下から出現する始点/終点を決定
	spawnStartY_ = targetGroundY_ - spawnRiseDepth_;
	spawnEndY_ = targetGroundY_;

	// ★ 地面“下”から出現 → 上昇 → 一瞬静止
	Vector3 spawnPos = position;
	spawnPos.y = spawnStartY_;
	model_->SetTranslate(spawnPos);

	spawning_ = true;
	spawnTimer_ = 0.0f;
	spawnHoldTimer_ = spawnHoldTime_;

	SetOwner(this);
	SetCenterPosition(spawnPos);
	sActives.push_back(this);
}

void Enemy::Update()
{
	if (isDead_ || !player_) return;

	const float dt = player_->GetDeltaTime();
	Vector3 pos = model_->GetTranslate();

	// --- スポーン演出（下からせり上がる→少し停止）---
	if (spawning_)
	{
		spawnTimer_ += dt;
		float t = Saturate(spawnTimer_ / spawnDuration_);
		if (spawnEase_) { t = t * t * (3.0f - 2.0f * t); } // smoothstep(0,1,t)

		Vector3 pos = model_->GetTranslate();
		pos.y = Lerp(spawnStartY_, spawnEndY_, t);
		model_->SetTranslate(pos);
		model_->Update();
		SetCenterPosition(pos);

		if (t >= 1.0f) {
			// 上がり切った後、少し留まる
			spawnHoldTimer_ -= dt;
			if (spawnHoldTimer_ <= 0.0f) { spawning_ = false; }
		}
		return; // 召喚中は移動・射撃しない
	}

	// ノックバック
	pos += knockVel_ * dt;
	knockVel_ *= 0.82f;

	// ★ 接地維持（半分沈まない）：常に最低でも targetGroundY_
	if (pos.y < targetGroundY_) pos.y = targetGroundY_;

	// --- プレイヤーとの距離で“間合い維持”（近接しない）---
	Vector3 toP = PlayerWorldPosition() - pos;
	Vector3 flat = { toP.x, 0.0f, toP.z };
	float dist = Vector3::Length(flat);
	if (dist > 1e-4f) {
		flat = flat / dist;

		if (dist < keepMinDist_) {
			// 近すぎ → 後退して距離を取る
			pos -= flat * (speed_ * dt);
		}
		else if (dist > keepMaxDist_) {
			// 遠すぎ → 射程に入れるため前進
			pos += flat * (speed_ * dt);
		} // ちょうどいい距離なら停止

		float yaw = std::atan2f(-flat.x, flat.z);
		model_->SetRotate({ 0.0f, yaw, 0.0f });
	}

	// 近い仲間とは水平に分離
	SeparateFromNeighbors(pos, dt);

	fireTimer_ -= dt;
	TryRangedAttack(dist, dt);

	model_->SetTranslate(pos);
	model_->Update();
	SetCenterPosition(pos);
}

void Enemy::Draw()
{
	if (isDead_ || !model_) return;
	model_->Draw();
}

void Enemy::OnCollision(Collider* other)
{
	if (isDead_ || !other) return;
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		// ★ 近接ダメージはオプション化
		if (enableMelee_) {
			ApplyMeleeDamage();
			ApplyKnockback(PlayerWorldPosition() - model_->GetTranslate(), knockback_);
		}
	}
}

void Enemy::TakeDamage(int damage)
{
	if (isDead_) return;
	hp_ -= damage;
	if (hp_ <= 0.0f)
	{
		hp_ = 0.0f;
		isDead_ = true;

		// ★死亡が確定した瞬間に一度だけキル加算
		ScoreManager::GetInstance()->AddKill();

		// ★★ 死亡が確定した“この瞬間”にだけドロップ判定 ★★
		if (!dropProcessed_ && itemManager_) {
			ItemType dropType;
			if (itemDropTable_.RollForDrop(dropType)) {
				Vector3 p = model_->GetTranslate();
				p.y += 0.5f; // 少し浮かせて埋まり防止＆見やすく
				itemManager_->Spawn(dropType, p);
			}
			dropProcessed_ = true; // 二重発火防止
		}
	}
}

Vector3 Enemy::GetWorldPosition() const
{
	return model_->GetTranslate(); // 使っている座標の取得に合わせてOK
}

Vector3 Enemy::PlayerWorldPosition()
{
	if (!player_) return Vector3{};            // ★ 必ず戻り値を返す
	auto* model = player_->GetAnimationModel();
	return model ? model->GetTranslate() : Vector3{};
}

void Enemy::ApplyMeleeDamage()
{
	if (!player_) return;
	const float dt = player_->GetDeltaTime();  // 実dtでDPS換算
	player_->TakeDamage(dps_ * dt);
}

void Enemy::SeparateFromNeighbors(Vector3& currentPos, float dt)
{
	const float selfR = avoidRadius_;
	const float maxPushPerSec = 6.0f;          // 1秒あたり最大押し戻し量（調整用）
	const float eps = 1e-4f;

	Vector3 accum{}; // 積算補正

	for (Enemy* e : sActives) {
		if (e == this || e->isDead_) continue;
		Vector3 op = e->model_->GetTranslate();   // 相手の位置
		Vector3 d = currentPos - op; d.y = 0.0f;         // 水平だけを見る
		float L = Vector3::Length(d);
		const float minDist = selfR + e->avoidRadius_; // 望ましい最小距離

		if (L > eps && L < minDist) {
			Vector3 n = d / L;                    // 押し戻し方向
			float overlap = (minDist - L);        // 重なり量
			accum += n * overlap;                 // まず合力で貯める
		}
	}

	// 積算補正を時間でスケール＆クランプして適用
	float len = Vector3::Length(accum);
	if (len > eps) {
		Vector3 corr = (accum / len) * std::min(len, maxPushPerSec * dt);
		currentPos += corr;
		// 地面クランプ維持
		if (currentPos.y < 0.0f) currentPos.y = 0.0f;
	}
}

void Enemy::TryRangedAttack(float dist, float dt)
{
	if (!player_ || isDead_) return;

	// 視界判定などは後で足す。まずは距離＆クールダウンのみ
	if (dist <= fireRange_ && fireTimer_ <= 0.0f) {
		// マズル位置を少し高めに
		Vector3 from = model_->GetTranslate();
		from.y = targetGroundY_ + halfHeight_ * 0.8f;

		Vector3 to = PlayerWorldPosition();
		Vector3 dir = to - from;
		float len = Vector3::Length(dir);
		if (len > 1e-5f) dir = dir / len; else dir = { 0,0,1 };

		const float bulletSpeed = 24.0f;
		EnemyBullet::Create(from, dir * bulletSpeed, fireDamage_);

		fireTimer_ = fireCooldown_;
		// SEやトレーサーを足したければここで
	}
}

void Enemy::ApplyKnockback(const Vector3& direction, float power)
{
	if (isDead_) return;

	// 水平成分だけでノックバック（地面に押し込まない）
	Vector3 n = direction;
	n.y = 0.0f;
	float len = Vector3::Length(n);
	if (len > 1e-5f) n = n / len; else n = { 0,0,0 };

	// 0.1秒くらい効くインパルスに換算して速度に足す
	const float impulse = power / 0.1f;
	knockVel_ += n * impulse;
}
