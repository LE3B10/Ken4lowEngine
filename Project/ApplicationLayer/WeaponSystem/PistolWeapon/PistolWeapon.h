#pragma once
#include "BaseWeapon.h"
#include "Object3D.h"
#include "WorldTransformEx.h"
#include "BallisticEffect.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

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
	void OnCollision(K4E::Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	K4E::Vector3 GetCenterPosition() const override;

public: /// ---------- アクセサー関数 ---------- ///

	// 親Transformを設定
	void SetParentTransform(const K4E::WorldTransformEx* parent) {
		parentTransform_ = parent;
		transform_.parent_ = const_cast<K4E::WorldTransformEx*>(parent);
	}

	// ワールド変換を設定
	void SetWorldTransform(const K4E::WorldTransformEx& transform) { transform_ = transform; }

	// ワールド変換を取得
	K4E::WorldTransformEx& GetWorldTransform() { return transform_; }

	// 座標を設定
	void SetPosition(const K4E::Vector3& pos) { position_ = pos; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<K4E::Object3D> model_; // ピストルモデル

	// ワールド変換
	K4E::WorldTransformEx transform_;
	const K4E::WorldTransformEx* parentTransform_ = nullptr;
	K4E::Vector3 offset_ = { 0.0f, 0.33f, 1.7f };

	static inline K4E::Vector3 position_;
};
