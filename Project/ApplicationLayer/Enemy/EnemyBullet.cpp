#include "EnemyBullet.h"
#include "Player.h"
#include "LinearInterpolation.h"
#include <cmath>

std::vector<std::unique_ptr<EnemyBullet>> EnemyBullet::sBullets_;

static inline Vector3 Normalize3(const Vector3& v)
{
	float L = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (L < 1e-6f) return { 0,0,1 };
	return { v.x / L, v.y / L, v.z / L };
}

void EnemyBullet::Create(const Vector3& pos, const Vector3& vel, float damage, float life)
{
	sBullets_.push_back(std::unique_ptr<EnemyBullet>(new EnemyBullet(pos, vel, damage, life)));
}

void EnemyBullet::UpdateAll(Player* player, float dt)
{
	for (auto it = sBullets_.begin(); it != sBullets_.end();) {
		auto& b = *it;
		b->life_ -= dt;
		if (b->life_ <= 0.0f) { it = sBullets_.erase(it); continue; }

		// 移動
		b->pos_ += b->vel_ * dt;

		// プレイヤーに命中？
		if (player) {
			Vector3 pp = player->GetAnimationModel()->GetTranslate();
			Vector3 d = b->pos_ - pp;
			float  L2 = d.x * d.x + d.y * d.y + d.z * d.z;
			float  R = b->radius_;
			if (L2 <= R * R) {
				player->TakeDamage(b->damage_);
				it = sBullets_.erase(it);
				continue;
			}
		}

		// 見た目更新
		b->model_->SetTranslate(b->pos_);
		b->model_->Update();
		++it;
	}
}

void EnemyBullet::DrawAll()
{
	for (auto& b : sBullets_) { b->model_->Draw(); }
}

void EnemyBullet::ClearAll()
{
	sBullets_.clear();
}

EnemyBullet::EnemyBullet(const Vector3& pos, const Vector3& vel, float dmg, float life)
	: pos_(pos), vel_(vel), damage_(dmg), life_(life)
{
	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");                 // 代替メッシュでOK
	model_->SetScale({ 0.12f, 0.12f, 0.6f });          // 細長くして弾感
	model_->SetTranslate(pos_);
	// 進行方向へ向ける（yaw/pitch）
	Vector3 d = Normalize3(vel_);
	float yaw = std::atan2f(-d.x, d.z);
	float pitch = -std::asinf(d.y);
	model_->SetRotate({ pitch, yaw, 0.0f });
}