#pragma once
#include "Collider.h"
#include <WorldTransformEx.h>
#include "BallisticEffect.h"

#include <memory>

/// -------------------------------------------------------------
///				　		  武器基底クラス
/// -------------------------------------------------------------
class BaseWeapon : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~BaseWeapon() = default;

	// 初期化処理
	virtual void Initialize() = 0;

	// 更新処理
	virtual void Update(float deltaTime) = 0;

	// 描画処理
	virtual void Draw() = 0;

	// リロード
	virtual void Reload() = 0;

public: /// ---------- 仮想関数 ---------- ///

	// 衝突時に呼ばれる仮想関数
	virtual void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	virtual Vector3 GetCenterPosition() const override = 0;

public: /// ---------- メンバ関数取得系 ---------- ///

	// 武器設定の取得
	const WeaponConfig& GetWeaponConfig() const { return weaponConfig_; }

protected: /// ---------- メンバ変数 ---------- ///

	WeaponConfig weaponConfig_; // 武器設定
};

