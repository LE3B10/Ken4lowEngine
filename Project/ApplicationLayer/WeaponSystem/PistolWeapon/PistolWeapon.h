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
	void Initialize() override;

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

	// ワールド変換を設定
	void SetWorldTransform(const WorldTransformEx& transform) { transform_ = transform; }

	// ワールド変換を取得
	WorldTransformEx& GetWorldTransform() { return transform_; }

	// 座標を設定
	void SetPosition(const Vector3& pos) { position_ = pos; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> model_; // ピストルモデル

	// ワールド変換
	WorldTransformEx transform_;
	const WorldTransformEx* parentTransform_ = nullptr;
	Vector3 offset_ = { 0.0f, 0.33f, 1.7f };

	static inline Vector3 position_;
};
