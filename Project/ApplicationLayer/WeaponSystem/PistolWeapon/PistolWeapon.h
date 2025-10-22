#pragma once
#include "BaseWeapon.h"
#include "Object3D.h"
#include "WorldTransformEx.h"
#include "BallisticEffect.h"

#include <memory>

/// -------------------------------------------------------------
///				　		  ピストル武器クラス
/// -------------------------------------------------------------
class PistolWeapon : public BaseWeapon
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const WeaponConfig& config) override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui();

	// リロード
	void Reload() override;

public: /// ---------- 衝突処理 ---------- ///

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	Vector3 GetCenterPosition() const override;

public: /// ---------- アクセサー関数 ---------- ///

	// 親Transformを設定
	void SetParentTransform(const WorldTransformEx* parent) {
		parentTransform_ = parent;
		transform_.parent_ = const_cast<WorldTransformEx*>(parent);
	}

	void SetWorldTransform(const WorldTransformEx& transform) { transform_ = transform; }
	WorldTransformEx& GetWorldTransform() { return transform_; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> model_; // ピストルモデル

	// ワールド変換
	WorldTransformEx transform_;
	const WorldTransformEx* parentTransform_ = nullptr;
	Vector3 offset_ = { 0.0f, 0.33f, 1.7f };

	static inline Vector3 position_;
};

/// -------------------------------------------------------------
///				　		  ピストル武器設定作成
/// -------------------------------------------------------------
static inline WeaponConfig MakePistolConfig()
{
	WeaponConfig pistol{};
	pistol.muzzle.sparksEnabled = true;
	pistol.muzzle.sparkCount = 14;
	pistol.muzzle.sparkLifeMin = 0.08f;
	pistol.muzzle.sparkLifeMax = 0.14f;
	pistol.muzzle.sparkSpeedMin = 8.0f;
	pistol.muzzle.sparkSpeedMax = 16.0f;
	pistol.muzzle.sparkConeDeg = 28.0f;
	pistol.muzzle.sparkGravityY = -25.0f;
	pistol.tracer.startOffsetForward = 2.10f;  // 弾/トレーサの開始点を少し前へ
	pistol.muzzle.offsetForward = 0.00f;  // フラッシュの根元 = 銃口
	pistol.muzzle.sparkOffsetForward = 0.02f;  // 火花は少し前から

	return pistol;
}
