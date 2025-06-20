#pragma once
#include <Object3D.h>
#include <WorldTransform.h>
#include "Collider.h"

#include <numbers>
#include <vector>
#include <memory>


class BaseCharacter : public Collider
{
protected: /// ---------- 構造体 ---------- ///

	/// ---------- 部位データ構造体 ---------- ///
	struct BodyPart
	{
		std::unique_ptr<Object3D> object;
		WorldTransform worldTransform_;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	virtual void Initialize();

	// パーツ初期化（派生クラスでオーバーライド可）
	virtual void InitializeParts() {};

	// 更新処理
	virtual void Update();

	// 描画処理
	virtual void Draw();

	// ワールド変換の取得
	const WorldTransform* GetWorldTransform() { return &body_.worldTransform_; }

	// ワールド変換の取得（const）
	Vector3 GetWorldPosition() const { return body_.worldTransform_.translate_; }

	// 中心座標を取得
	Vector3 GetCenterPosition() const override { return body_.worldTransform_.translate_; }

	// HPを設定
	void SetHP(float hp) { hp_ = hp; }

	// ダメージ量
	void TakeDamage(float amount)
	{
		hp_ -= amount;
		if (hp_ <= 0.0f) isDead_ = true;
	}

	// 死亡フラグを取得
	bool IsDead() const { return isDead_; }

protected: /// ---------- メンバ変数 ---------- ///

	// 体（親）
	BodyPart body_;

	// 他の部位（子）
	std::vector<BodyPart> parts_;

	float hp_ = 1000.0f;
	bool isDead_ = false;
};

