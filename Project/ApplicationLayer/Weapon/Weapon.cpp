#define NOMINMAX
#include "Weapon.h"
#include <Bullet.h>
#include <AudioManager.h>

#include <imgui.h>


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Weapon::Initialize()
{
	bullets_.clear();
	ammoInfo_.ammoInClip = ammoInfo_.clipSize;
	isReloading_ = false;
	reloadTimer_ = 0.0f;

	// 武器ごとのパラメータ設定
	switch (type_)
	{
	case WeaponType::Rifle:
		ammoInfo_ = { 30, 90, 30, 90, 1, 25.0f, 1000.0f }; // 30回撃てて、1回で1発出す
		reloadTime_ = 1.5f; // ライフルはリロード時間が短い
		bulletSpeed_ = 40.0f; // ライフルの弾速はショットガンより速い
		fireSEPath_ = "gun.mp3";
		break;

	case WeaponType::Shotgun:
		ammoInfo_ = { 6, 30, 6, 30, 8, 10.0f, 100.0f }; // 6回撃てて、1回で8粒出す
		reloadTime_ = 2.5f; // ショットガンはリロード時間が長い
		bulletSpeed_ = 24.0f; // ショットガンの弾速はライフルより遅い
		fireSEPath_ = "shotgunFire.mp3";
		break;
	}
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Weapon::Update()
{
	// リロード中の処理
	if (isReloading_)
	{
		reloadTimer_ += 1.0f / 60.0f;

		// リロード完了チェック
		if (reloadTimer_ >= reloadTime_)
		{
			int needed = ammoInfo_.clipSize - ammoInfo_.ammoInClip;
			int toLoad = std::min(needed, ammoInfo_.reserveAmmo);
			ammoInfo_.ammoInClip += toLoad;
			ammoInfo_.reserveAmmo -= toLoad;
			isReloading_ = false;
		}
	}

	fireTimer_ += 1.0f / 60.0f;  // 60FPS前提。可変FPSなら deltaTime を使う

	// 弾の更新
	for (auto& bullet : bullets_) bullet->Update();

	// 死亡した弾を削除
	bullets_.erase(
		std::remove_if(bullets_.begin(), bullets_.end(),
			[](const std::unique_ptr<Bullet>& b) { return b->IsDead(); }),
		bullets_.end()
	);
	if (Input::GetInstance()->TriggerKey(DIK_R) &&
		ammoInfo_.ammoInClip < ammoInfo_.clipSize && ammoInfo_.reserveAmmo > 0 && !isReloading_)
	{
		Reload();
	}
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Weapon::Draw()
{
	for (auto& bullet : bullets_)
	{
		if (!bullet->IsDead())
		{
			bullet->Draw();
		}
	}
}


/// -------------------------------------------------------------
///				　			　 発射試行
/// -------------------------------------------------------------
void Weapon::TryFire(const Vector3& position, const Vector3& direction)
{
	if (isReloading_ || ammoInfo_.ammoInClip <= 0) return;

	if (fireTimer_ < fireInterval_) return;

	fireTimer_ = 0.0f;

	switch (type_)
	{
	case WeaponType::Rifle: // ライフルの場合は単発弾を発射
		FireSingleBullet(position, direction);

		// サウンド再生
		AudioManager::GetInstance()->PlaySE(fireSEPath_, 0.5f, 2.0f);

		break;

	case WeaponType::Shotgun: // ショットガンの場合は散弾を発射
		FireShotgunSpread(position, direction);

		// サウンド再生
		AudioManager::GetInstance()->PlaySE(fireSEPath_);

		break;
	}
}


/// -------------------------------------------------------------
///				　			　 リロード開始
/// -------------------------------------------------------------
void Weapon::Reload()
{
	if (ammoInfo_.ammoInClip == ammoInfo_.clipSize || ammoInfo_.reserveAmmo == 0) return;

	isReloading_ = true;
	reloadTimer_ = 0.0f;
}


/// -------------------------------------------------------------
///				　			　 弾丸補充
/// -------------------------------------------------------------
void Weapon::AddReserveAmmo(int amount)
{
	ammoInfo_.reserveAmmo = std::min(ammoInfo_.reserveAmmo + amount, ammoInfo_.maxReserve);
}


/// -------------------------------------------------------------
///				　			　 ImGui描画処理
/// -------------------------------------------------------------
void Weapon::DrawImGui()
{
	ImGui::Text("Ammo: %d / %d", ammoInfo_.ammoInClip, ammoInfo_.clipSize);
	ImGui::Text("Reserve: %d / %d", ammoInfo_.reserveAmmo, ammoInfo_.maxReserve);
	ImGui::Text("Ammo / Reserve : %d / %d", ammoInfo_.ammoInClip, ammoInfo_.reserveAmmo);

	ImGui::Text("Reloading: %s", isReloading_ ? "Yes" : "No");
}


/// -------------------------------------------------------------
///				　			　 弾丸のみ更新
/// -------------------------------------------------------------
void Weapon::UpdateBulletsOnly()
{
	for (auto& bullet : bullets_) {
		bullet->Update();
	}
	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
		[](const std::unique_ptr<Bullet>& b) { return b->IsDead(); }),
		bullets_.end());
}


/// -------------------------------------------------------------
///				　			　 単発弾発射
/// -------------------------------------------------------------
void Weapon::FireSingleBullet(const Vector3& pos, const Vector3& dir)
{
	auto bullet = std::make_unique<Bullet>();
	bullet->Initialize();
	bullet->SetPosition(pos);
	bullet->SetVelocity(dir * bulletSpeed_);
	bullet->SetDamage(ammoInfo_.bulletDamage);  // ← 新しく追加
	bullets_.push_back(std::move(bullet));

	// ショットガン発射時は FireShotgunSpread 側で消費するのでここでは減らさない
	if (type_ == WeaponType::Rifle) {
		--ammoInfo_.ammoInClip;
	}
}


/// -------------------------------------------------------------
///				　			　 散弾発射
/// -------------------------------------------------------------
void Weapon::FireShotgunSpread(const Vector3& pos, const Vector3& dir)
{
	for (int i = 0; i < ammoInfo_.firePerShot; ++i)
	{
		Vector3 spreadDir = ApplyRandomSpread(dir, 5.0f);
		FireSingleBullet(pos, spreadDir); // この中で 1発分発射処理
	}

	--ammoInfo_.ammoInClip; // 1回撃つのにマガジン1つ消費（散弾は8発出るけど1回分の扱い）
}


/// -------------------------------------------------------------
///				　		ランダムスプレッド適用
/// -------------------------------------------------------------
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
