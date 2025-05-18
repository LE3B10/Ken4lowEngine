#define NOMINMAX
#include "Weapon.h"
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

	for (auto& bullet : bullets_)
	{
		bullet->Update();
	}

	// 弾丸の寿命が切れたら削除
	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
		[](const std::unique_ptr<Bullet>& b) { return b->IsDead(); }), bullets_.end());

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

	auto bullet = std::make_unique<Bullet>();
	bullet->Initialize(); // 引数なし
	bullet->SetPosition(position);
	bullet->SetVelocity(direction * 1.5f);
	bullets_.push_back(std::move(bullet));

	--ammoInClip_;
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
