#define NOMINMAX
#include "WeaponInstance.h"

#include <algorithm>
#include <cmath>
#include <random>

// 既存プロジェクトのヘッダー
#include "BulletManager.h"
#include "Camera.h"

static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

// K4E::Vector3 はエンジン側の型を使用（Player.cpp で使っている前提）
static K4E::Vector3 NormalizeSafe(const K4E::Vector3& v)
{
	const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
	if (lenSq <= 1e-6f) return { 0,0,1 };
	const float inv = 1.0f / std::sqrt(lenSq);
	return { v.x * inv, v.y * inv, v.z * inv };
}

static K4E::Vector3 Cross(const K4E::Vector3& a, const K4E::Vector3& b)
{
	return { a.y * b.z - a.z * b.y,
			 a.z * b.x - a.x * b.z,
			 a.x * b.y - a.y * b.x };
}

static float Dot(const K4E::Vector3& a, const K4E::Vector3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static K4E::Vector3 Add(const K4E::Vector3& a, const K4E::Vector3& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

static K4E::Vector3 Mul(const K4E::Vector3& a, float s)
{
	return { a.x * s, a.y * s, a.z * s };
}

/// forward を「コーン状」にランダムに曲げる（簡易スプレッド）
static K4E::Vector3 ApplySpread(const K4E::Vector3& forward, float spreadDeg)
{
	if (spreadDeg <= 0.0f) return NormalizeSafe(forward);

	static thread_local std::mt19937 rng{ 12345u };
	std::uniform_real_distribution<float> u01(0.0f, 1.0f);

	const float spreadRad = spreadDeg * 3.14159265f / 180.0f;

	// 角度をランダム（面積一様）
	const float theta = 2.0f * 3.14159265f * u01(rng);
	const float cosPhi = 1.0f - u01(rng) * (1.0f - std::cos(spreadRad));
	const float sinPhi = std::sqrt(std::max(0.0f, 1.0f - cosPhi * cosPhi));

	// forward に直交する basis を作る
	K4E::Vector3 f = NormalizeSafe(forward);
	K4E::Vector3 up = { 0,1,0 };
	if (std::fabs(Dot(f, up)) > 0.98f) up = { 1,0,0 };
	K4E::Vector3 right = NormalizeSafe(Cross(up, f));
	K4E::Vector3 realUp = Cross(f, right);

	// コーン方向
	K4E::Vector3 dir = Add(
		Add(Mul(right, std::cos(theta) * sinPhi), Mul(realUp, std::sin(theta) * sinPhi)),
		Mul(f, cosPhi)
	);
	return NormalizeSafe(dir);
}

void WeaponInstance::Equip(const WeaponParams& p)
{
	params_ = p;
	fireModeAutomatic_ = params_.isAutomatic;

	st_.magAmmo = params_.magCapacity;
	st_.reserveAmmo = params_.maxReserveAmmo;

	st_.fireCooldown = 0.0f;
	st_.isReloading = false;
	st_.reloadTimer = 0.0f;

	st_.burstRemaining = 0;
	st_.burstTimer = 0.0f;

	st_.spread = 0.0f;
}

bool WeaponInstance::ToggleFireMode()
{
	if (!params_.canToggleFireMode) return false;
	fireModeAutomatic_ = !fireModeAutomatic_;
	return true;
}

void WeaponInstance::Tick(float dt)
{
	// 入力 ⇒ 要求ラッチ
	if (st_.reloadRequest)
	{
		st_.reloadRequested = true;
		st_.reloadRequest = false;
	}

	// リロード
	if (st_.isReloading)
	{
		st_.reloadTimer -= dt;
		if (st_.reloadTimer <= 0.0f)
		{
			FinishReload(); // 弾補充
			st_.reloadRequested = false; // 要求処理完了
			st_.reloadTimer = 0.0f;
		}

		// リロード中はここで終わり
		return;
	}

	// 開始判定
	if (st_.reloadRequested || st_.pendingReload)
	{
		if (CanStartReload())
		{
			StartReloadInternal();
		}
		else
		{
			// どうがんばあっても無理なら要求を捨てる
			const bool impossible = (st_.magAmmo >= params_.magCapacity) || (st_.reserveAmmo <= 0);

			if (impossible)
			{
				st_.reloadRequested = false;
				st_.pendingReload = false;
			}
			else
			{
				st_.pendingReload = true;
				st_.reloadRequested = false;
			}
		}
	}

	// クールダウン
	if (st_.fireCooldown > 0.0f) st_.fireCooldown -= dt;

	// バースト進行
	if (st_.burstRemaining > 0)
	{
		st_.burstTimer -= dt;
		if (st_.burstTimer <= 0.0f)
		{
			// 次弾を撃てる状態に
			st_.fireCooldown = 0.0f;
		}
	}

	// 拡散回復（簡易）
	const float rec = std::max(0.0f, params_.recoilRecovery);
	st_.spread = std::max(0.0f, st_.spread - dt * rec * 0.02f);
}

void WeaponInstance::StartReload()
{
	if (st_.isReloading) return;
	if (st_.magAmmo >= params_.magCapacity) return;
	if (st_.reserveAmmo <= 0) return;

	st_.isReloading = true;
	st_.reloadTimer = std::max(0.0f, params_.reloadSec);
}

void WeaponInstance::CancelReload()
{
	// 途中キャンセル : 弾は補充しない
	st_.isReloading = false;
	st_.reloadTimer = 0.0f;

	// 要求 / キューも全部捨てる（再開事故防止）
	st_.reloadRequested = false;
	st_.reloadRequest = false;
	st_.pendingReload = false;
}

bool WeaponInstance::CanStartReload() const
{
	if (st_.isReloading) return false;
	if (st_.magAmmo >= params_.magCapacity) return false;
	if (st_.reserveAmmo <= 0) return false;
	return true;
}

void WeaponInstance::StartReloadInternal()
{
	st_.isReloading = true;
	st_.reloadTimer = std::max(0.0f, params_.reloadSec);

	// 要求は消費して潰す
	st_.reloadRequest = false;
	st_.reloadRequested = false;
	st_.pendingReload = false;
}

bool WeaponInstance::WantFire(bool fireHeld, bool firePressed) const
{
	return fireModeAutomatic_ ? fireHeld : firePressed;
}

bool WeaponInstance::CanFire() const
{
	if (st_.isReloading) return false;
	if (st_.fireCooldown > 0.0f) return false;
	if (st_.magAmmo < params_.ammoPerShot) return false;

	// バースト中は burstTimer が 0以下のタイミングだけ撃てる
	if (st_.burstRemaining > 0 && st_.burstTimer > 0.0f) return false;

	return true;
}

void WeaponInstance::ConsumeAmmo()
{
	st_.magAmmo -= params_.ammoPerShot;
	if (st_.magAmmo < 0) st_.magAmmo = 0;
}

void WeaponInstance::FinishReload()
{
	st_.isReloading = false;

	const int need = params_.magCapacity - st_.magAmmo;
	const int add = std::min(need, st_.reserveAmmo);

	st_.magAmmo += add;
	st_.reserveAmmo -= add;
}

void WeaponInstance::TryFire(bool fireHeld, bool firePressed,
	K4E::Camera* cam,
	BulletManager* bulletMgr,
	CollisionManager* /*colMgr*/)
{
	const bool want = WantFire(fireHeld, firePressed);
	if (!want) return;

	// バースト開始条件：バースト武器で、現在バースト中でない
	if (params_.burstCount >= 2 && st_.burstRemaining == 0)
	{
		st_.burstRemaining = params_.burstCount;
		st_.burstTimer = 0.0f;
	}

	if (!CanFire())
	{
		// 弾切れで自動リロードしたいなら
		// if (st_.magAmmo < params_.ammoPerShot) StartReload();
		return;
	}

	// 実射
	ConsumeAmmo();

	// 拡散増加（accuracyが低いほど増えやすい、みたいな調整も可能）
	st_.spread += std::max(0.0f, params_.spreadIncrease);

	FireShot(cam, bulletMgr, nullptr);

	// 次弾まで
	if (st_.burstRemaining > 0)
	{
		st_.burstRemaining--;
		if (st_.burstRemaining > 0)
		{
			st_.burstTimer = std::max(0.0f, params_.burstIntervalSec);
			st_.fireCooldown = std::max(0.0f, params_.burstIntervalSec);
		}
		else
		{
			// バースト終了→武器の発射間隔へ
			st_.fireCooldown = std::max(0.0f, params_.secPerShot);
		}
	}
	else
	{
		st_.fireCooldown = std::max(0.0f, params_.secPerShot);
	}
}

void WeaponInstance::FireShot(K4E::Camera* cam, BulletManager* bulletMgr, CollisionManager* /*colMgr*/)
{
	if (!cam) return;

	// Projectile運用のみ実装（hitscanは必要になったら追加）
	if (!params_.isProjectile) return;
	if (!bulletMgr) return;

	K4E::Vector3 origin = cam->GetTranslate();
	K4E::Vector3 fwd = NormalizeSafe(cam->GetForward());

	// 生成位置を少し前へ
	origin = Add(origin, Mul(fwd, params_.muzzleForwardOffset));

	// spreadDeg を accuracy/spread から作る（簡易）
	// accuracy=1.0 ならほぼ0deg、accuracyが低いほどブレが増える
	const float acc = Clamp01(params_.accuracy);
	const float baseDeg = (1.0f - acc) * 3.0f;      // 調整用（必要ならマスターデータに明示項目を追加）
	const float spreadDeg = baseDeg + st_.spread * 2.0f;

	K4E::Vector3 dir = ApplySpread(fwd, spreadDeg);

	bulletMgr->Spawn(origin, dir, params_.projectileSpeed, static_cast<int>(params_.damage));
}
