#define NOMINMAX
#include "Weapon.h"
#include <Bullet.h>
#include <imgui.h>

void Weapon::Initialize()
{
	bullets_.clear();
	ammoInClip_ = maxAmmo_;
	isReloading_ = false;
	reloadTimer_ = 0.0f;
}

void Weapon::Update()
{
	if (isReloading_)
	{
		reloadTimer_ += 1.0f / 60.0f; // フレーム基準の時間（仮）
		if (reloadTimer_ >= reloadTime_)
		{
			int needed = maxAmmo_ - ammoInClip_;
			int actualReload = std::min(needed, ammoReserve_);
			ammoInClip_ += actualReload;
			ammoReserve_ -= actualReload;
			isReloading_ = false;
		}
	}

	fireTimer_ += 1.0f / 60.0f;  // 60FPS前提。可変FPSなら deltaTime を使う

	// 弾の更新
	for (auto& bullet : bullets_) {
		bullet->Update();
	}

	// 死亡した弾を削除
	bullets_.erase(
		std::remove_if(bullets_.begin(), bullets_.end(),
			[](const std::unique_ptr<Bullet>& b) { return b->IsDead(); }),
		bullets_.end()
	);
	if (Input::GetInstance()->TriggerKey(DIK_R) && ammoInClip_ < maxAmmo_ && ammoReserve_ > 0 && !isReloading_) {
		Reload();
	}
}

void Weapon::Draw()
{
	for (auto& bullet : bullets_) {
		bullet->Draw();
	}
}

void Weapon::TryFire(const Vector3& position, const Vector3& direction)
{
	if (isReloading_ || ammoInClip_ <= 0) return;

	switch (type_)
	{
	case WeaponType::Rifle:
		if (fireTimer_ >= fireInterval_)
		{
			FireSingleBullet(position, direction, 25.0f);
			fireTimer_ = 0.0f; // タイマーリセット
		}
		break;

	case WeaponType::Shotgun:
		if (fireTimer_ >= fireInterval_)
		{
			FireShotgunSpread(position, direction, 5, 15.0f);
			fireTimer_ = 0.0f;
		}
		break;
	}
}

void Weapon::Reload()
{
	if (ammoInClip_ == maxAmmo_ || ammoReserve_ == 0) return;

	isReloading_ = true;
	reloadTimer_ = 0.0f;
}

void Weapon::AddReserveAmmo(int amount)
{
	ammoReserve_ = std::min(ammoReserve_ + amount, maxAmmoReserve_);
}

void Weapon::DrawImGui()
{
	ImGui::Text("Ammo: %d / %d", ammoInClip_, maxAmmo_);
	ImGui::Text("Reserve: %d / %d", ammoReserve_, maxAmmoReserve_);
	ImGui::Text("Ammo / Reserve : %d / %d", ammoInClip_, ammoReserve_);

	ImGui::Text("Reloading: %s", isReloading_ ? "Yes" : "No");
}

void Weapon::FireSingleBullet(const Vector3& pos, const Vector3& dir, float damage)
{
	auto bullet = std::make_unique<Bullet>();
	bullet->Initialize();
	bullet->SetPosition(pos);
	bullet->SetVelocity(dir * bulletSpeed_);
	bullet->SetDamage(damage);  // ← 新しく追加
	bullets_.push_back(std::move(bullet));
	--ammoInClip_;
}

void Weapon::FireShotgunSpread(const Vector3& pos, const Vector3& dir, int count, float damage)
{
	for (int i = 0; i < count; ++i)
	{
		Vector3 spreadDir = ApplyRandomSpread(dir, 5.0f);  // 小さな角度ずらし
		FireSingleBullet(pos, spreadDir, damage);
	}
}

Vector3 Weapon::ApplyRandomSpread(const Vector3& baseDir, float angleRangeDeg)
{
	float yawOffset = GetRandomFloat(-angleRangeDeg, angleRangeDeg);
	float pitchOffset = GetRandomFloat(-angleRangeDeg, angleRangeDeg);

	float yawRad = DegToRad(yawOffset);
	float pitchRad = DegToRad(pitchOffset);

	Matrix4x4 rotY = Matrix4x4::MakeRotateYMatrix(yawRad);
	Matrix4x4 rotX = Matrix4x4::MakeRotateXMatrix(pitchRad);

	return Vector3::Transform(baseDir, rotX * rotY);
}
