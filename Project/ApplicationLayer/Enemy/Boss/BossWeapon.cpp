#include "BossWeapon.h"

void BossWeapon::Initialize()
{
	bullets_.clear();
	fireTimer_ = 0.0f;
}

void BossWeapon::Update()
{
	fireTimer_ += deltaTime_;
	burstTimer_ += deltaTime_;

	// バースト処理
	if (isBursting_ && burstCount_ > 0 && burstTimer_ >= burstInterval_) {
		burstTimer_ = 0.0f;
		TryFire(firePos_, fireDir_);
		burstCount_--;
		if (burstCount_ <= 0) {
			isBursting_ = false;
		}
	}

	for (auto& bullet : bullets_) bullet->Update();

	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
		[](const std::unique_ptr<BossBullet>& b) { return b->IsDead(); }),
		bullets_.end());
}

void BossWeapon::TryFire(const Vector3& pos, const Vector3& dir)
{
	if (fireTimer_ < fireInterval_) return;
	fireTimer_ = 0.0f;

	auto bullet = std::make_unique<BossBullet>();
	bullet->Initialize();
	bullet->SetPosition(pos);
	bullet->SetVelocity(Vector3::Normalize(dir) * bulletSpeed_);
	bullet->SetDamage(bulletDamage_);
	bullet->SetBoss(boss_);

	bullets_.push_back(std::move(bullet));
}

void BossWeapon::Draw()
{
	for (const auto& bullet : bullets_)
	{
		if (!bullet->IsDead()) bullet->Draw();
	}
}

void BossWeapon::StartBurstFire(const Vector3& pos, const Vector3& dir)
{
	if (isBursting_) return; // バースト中なら無視
	isBursting_ = true;
	burstCount_ = maxBurstCount_;
	fireDir_ = dir;
	firePos_ = pos;
	burstTimer_ = 0.0f;
}
