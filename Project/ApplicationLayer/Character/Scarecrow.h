#pragma once
#include "Collider.h"
#include "Object3D.h"

#include <memory>

/// -------------------------------------------------------------
///					　		案山子クラス
/// -------------------------------------------------------------
class Scarecrow : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~Scarecrow() = default;

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	Vector3 GetCenterPosition() const override;

private: /// ---------- メンバ変数 ---------- ///

	// 3Dモデル
	std::unique_ptr<Object3D> model_ = nullptr;

	// 位置
	Vector3 position_{ 0.0f, 2.0f, 15.0f };

	// スケール
	Vector3 scale_{ 1.0f, 2.0f, 1.0f };
};

