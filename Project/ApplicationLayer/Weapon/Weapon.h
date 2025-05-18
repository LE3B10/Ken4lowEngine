#pragma once
#include <memory>
#include <vector>
#include "Bullet.h"
#include "Input.h"

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

	// 弾薬の取得
	int GetAmmoInClip() const { return ammoInClip_; }

	// 最大弾薬の取得
	int GetMaxAmmo() const { return maxAmmo_; }

	// 所持弾薬の取得
	int GetAmmoReserve() const { return ammoReserve_; }

	// 最大所持弾薬の取得
	int GetMaxAmmoReserve() const { return maxAmmoReserve_; }

private: /// ---------- メンバ変数 ---------- ///

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
};
