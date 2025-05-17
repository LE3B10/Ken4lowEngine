#pragma once
#include <Object3D.h>
#include <WorldTransform.h>
#include "Collider.h"
#include "ParticleEmitter.h"

#include <numbers>
#include <vector>
#include <memory>


class BaseCharacter
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
	Vector3 GetWorldPosition() const { return body_.worldTransform_.worldTranslate_; }

protected: /// ---------- メンバ変数 ---------- ///

	// 体（親）
	BodyPart body_;

	// 他の部位（子）
	std::vector<BodyPart> parts_;
};

