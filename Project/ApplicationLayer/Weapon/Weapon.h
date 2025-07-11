#pragma once
#include <memory>
#include <vector>
#include "Bullet.h"
#include "Input.h"
#include "WeaponType.h"

#include <random>
#include <numbers>


/// -------------------------------------------------------------
///				　		弾薬情報構造体
/// -------------------------------------------------------------
struct AmmoInfo
{
	int ammoInClip = 0;     // マガジン内の弾数
	int reserveAmmo = 0;    // 予備弾薬数
	int clipSize = 0;       // マガジン容量
	int maxReserve = 0;     // 最大所持弾数
	int firePerShot = 1;    // 1回で消費する弾数（ショットガン=8）
	float bulletDamage = 0;	// ★追加（必要なら）
	float range = 100;		// 弾丸の射程（メートル単位）
};

/// ---------- 前方宣言 ---------- ///
class Player;
class FpsCamera;


/// -------------------------------------------------------------
///				　		武器クラス
/// -------------------------------------------------------------
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

	void UpdateBulletsOnly();

	void CreateBullet(const Vector3& position, const Vector3& direction);

public: /// ---------- ゲッタ ---------- ///

	// 弾丸の取得
	const std::vector<std::unique_ptr<Bullet>>& GetBullets() const { return bullets_; }

	// リロード中かどうか
	bool IsReloading() const { return isReloading_; }

	// リロードの進捗を取得
	float GetReloadProgress() const { return std::clamp(static_cast<float>(reloadTimer_ / reloadTime_), 0.0f, 1.0f); }

	// 弾薬の取得
	int GetAmmoInClip() const { return ammoInfo_.ammoInClip; }

	WeaponType GetWeaponType() const { return type_; } // 武器の種類を取得

	const AmmoInfo& GetAmmoInfo() const { return ammoInfo_; }

	int GetAmmoReserve() const { return ammoInfo_.reserveAmmo; }

	Player* GetPlayer() const { return player_; }
	FpsCamera* GetFpsCamera() const { return fpsCamera_; }

public: /// ---------- セッター ---------- ///

	// 武器の種類を設定
	void SetWeaponType(WeaponType type) { type_ = type; }

	void SetPlayer(Player* player) { player_ = player; }

	void SetFpsCamera(FpsCamera* fpsCamera) { fpsCamera_ = fpsCamera; }

private: /// ---------- メンバ変数 ---------- ///

	// ライフル
	void FireSingleBullet(const Vector3& pos, const Vector3& dir);

	// ショットガン
	void FireShotgunSpread(const Vector3& pos, const Vector3& dir);

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

	AmmoInfo ammoInfo_; // 弾薬情報

	Player* player_ = nullptr;
	FpsCamera* fpsCamera_ = nullptr;

	// 弾丸のプレハブ
	std::vector<std::unique_ptr<Bullet>> bullets_;
	
	bool isReloading_ = false;	// リロード中かどうか
	float reloadTime_ = 0.0f;	// リロード時間（秒）
	float reloadTimer_ = 0.0f;	// リロードタイマー
	float bulletSpeed_ = 0.0f;	// 弾速
	float fireTimer_ = 0.0f;	// 発射タイマー
	float fireInterval_ = 0.1f; // 0.1秒に1発（10発/秒）

	// SEのパス
	std::string fireSEPath_;
};
