#define NOMINMAX
#include "Weapon.h"
#include <Bullet.h>
#include <AudioManager.h>
#include <Player.h>
#include <FpsCamera.h>

#include <imgui.h>
#include <ParticleManager.h>


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Weapon::Initialize()
{
	bullets_.clear();
	ammoInfo_.ammoInClip = ammoInfo_.clipSize;
	isReloading_ = false;
	reloadTimer_ = 0.0f;

	bullets_.push_back(std::make_unique<Bullet>());
	bullets_.back()->Initialize();

	// 武器ごとのパラメータ設定
	switch (type_)
	{
	case WeaponType::Rifle:
		ammoInfo_ = { 300000000, 90, 30, 90, 1, 0.1f, 1000.0f }; // 30回撃てて、1回で1発出す
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

	ParticleManager::GetInstance()->CreateParticleGroup("Laser", "particle.png", ParticleEffectType::LaserBeam);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Weapon::Update()
{
	// リロード中の処理
	if (isReloading_)
	{
		reloadTimer_ += player_->GetAnimationModel()->GetDeltaTime();

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

	fireTimer_ += player_->GetAnimationModel()->GetDeltaTime();  // 60FPS前提。可変FPSなら deltaTime を使う

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

		if (fpsCamera_) fpsCamera_->AddRecoil(0.004f, 0.0045f);

		// サウンド再生
		AudioManager::GetInstance()->PlaySE(fireSEPath_, 0.2f, 2.0f);

		break;

	case WeaponType::Shotgun: // ショットガンの場合は散弾を発射
		FireShotgunSpread(position, direction);

		if (fpsCamera_) fpsCamera_->AddRecoil(0.008f, 0.007f);

		// サウンド再生
		AudioManager::GetInstance()->PlaySE(fireSEPath_, 0.2);

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
	//auto bullet = std::make_unique<Bullet>();
	//bullet->Initialize();
	//bullet->SetPosition(pos);
	//bullet->SetVelocity(dir * bulletSpeed_);
	//bullet->SetDamage(ammoInfo_.bulletDamage);  // ← 新しく追加

	//// 命中通知用に Player を渡す
	//bullet->SetPlayer(player_);

	//bullets_.push_back(std::move(bullet));

	CreateBullet(pos, dir); // 弾丸を作成

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
	// 基準方向を正規化
	Vector3 forward = Vector3::Normalize(baseDir);

	// baseDir に垂直なベクトルを定義（右方向）
	Vector3 worldUp = { 0.0f, 1.0f, 0.0f };

	// forward と worldUp が平行すぎると右が定義できないので調整
	if (fabsf(Vector3::Dot(forward, worldUp)) > 0.99f) {
		worldUp = { 0.0f, 0.0f, 1.0f };
	}

	Vector3 right = Vector3::Normalize(Vector3::Cross(worldUp, forward));
	Vector3 up = Vector3::Normalize(Vector3::Cross(forward, right));

	// ランダム角度（度→ラジアン）
	float yawOffset = DegToRad(GetRandomFloat(-angleRangeDeg, angleRangeDeg));
	float pitchOffset = DegToRad(GetRandomFloat(-angleRangeDeg, angleRangeDeg));

	// ベース方向に対してスプレッドを適用
	Vector3 spreadDir = forward;
	spreadDir += right * tanf(yawOffset);   // 水平方向に拡がる
	spreadDir += up * tanf(pitchOffset);    // 垂直方向に拡がる

	return Vector3::Normalize(spreadDir);
}


void Weapon::CreateBullet(const Vector3& position, const Vector3& direction)
{
	// 弾を作成する処理（既存コード）
	std::unique_ptr<Bullet> bullet = std::make_unique<Bullet>();
	Vector3 bulletVelocity = direction * bulletSpeed_;
	bullet->Initialize();
	bullet->SetPosition(position);
	bullet->SetVelocity(bulletVelocity);
	bullet->SetDamage(ammoInfo_.bulletDamage);  // ダメージ
	bullets_.push_back(std::move(bullet));

	// ✅ レーザービーム演出
	float beamLength = 60.0f;
	int segmentCount = 30;
	Vector4 beamColor = { 0.0f, 1.0f, 1.0f, 0.8f };

	ParticleManager::GetInstance()->EmitLaserBeamFakeStretch(
		"Laser",
		position + direction, // 銃口から少し前に
		direction,
		bulletVelocity,
		beamLength,
		segmentCount,
		beamColor
	);
}
