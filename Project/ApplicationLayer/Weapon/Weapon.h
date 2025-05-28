#pragma once
#include <memory>
#include <vector>
#include "Bullet.h"
#include "Input.h"
#include "WeaponType.h"

#include <random>
#include <numbers>


class Weapon
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 発射試行
	void TryFire(const Vector3& position, const Vector3& direction); // 発射試行

	// リロード開始
	void Reload();

	// 弾丸補充
	void AddReserveAmmo(int amount);

	// ImGui描画処理
	void DrawImGui();

public: /// ---------- ゲッタ ---------- ///

	// 弾丸の取得
	const std::vector<std::unique_ptr<Bullet>>& GetBullets() const { return bullets_; }

	// リロード中かどうか
	bool IsReloading() const { return isReloading_; }

	// リロードの進捗を取得
	float GetReloadProgress() const { return std::clamp(static_cast<float>(reloadTimer_ / reloadTime_), 0.0f, 1.0f); }

	// 弾薬の取得
	int GetAmmoInClip() const { return ammoInClip_; }

	// 最大弾薬の取得
	int GetMaxAmmo() const { return maxAmmo_; }

	// 所持弾薬の取得
	int GetAmmoReserve() const { return ammoReserve_; }

	// 最大所持弾薬の取得
	int GetMaxAmmoReserve() const { return maxAmmoReserve_; }

	WeaponType GetWeaponType() const { return type_; } // 武器の種類を取得

public: /// ---------- セッター ---------- ///

	// 武器の種類を設定
	void SetWeaponType(WeaponType type) { type_ = type; }

private: /// ---------- メンバ変数 ---------- ///

	// ライフル
	void FireSingleBullet(const Vector3& pos, const Vector3& dir, float damage);

	// ショットガン
	void FireShotgunSpread(const Vector3& pos, const Vector3& dir, int count, float damage);

	// 散弾
	Vector3 ApplyRandomSpread(const Vector3& baseDir, float angleRangeDeg);

	constexpr float DegToRad(float degrees) { return degrees * (std::numbers::pi_v<float> / 180.0f); }

	float GetRandomFloat(float min, float max)
	{
		static std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(min, max);
		return dist(rng);
	}

private: /// ---------- メンバ変数 ---------- ///

	WeaponType type_ = WeaponType::Rifle; // 武器の種類

	// 弾丸のプレハブ
	std::vector<std::unique_ptr<Bullet>> bullets_;

	// 弾丸の発射位置
	int ammoInClip_ = 30;
	const int maxAmmo_ = 30;

	int ammoReserve_ = 90;           // 🔽 所持弾薬（新規）
	const int maxAmmoReserve_ = 90; // 🔽 最大所持弾薬

	bool isReloading_ = false;
	float reloadTime_ = 1.5f;
	float reloadTimer_ = 0.0f;

	float fireTimer_ = 0.0f;
	float fireInterval_ = 0.1f; // 0.1秒に1発（10発/秒）
	float bulletSpeed_ = 14.0f; // 弾速
};
