#pragma once
#include <Object3D.h>
#include <WorldTransform.h>
#include "Collider.h"
#include "ParticleEmitter.h"

#include <numbers>
#include <vector>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class Input;
class Camera;
class ParticleManager;


/// -------------------------------------------------------------
///					　キャラクターの基底クラス
/// -------------------------------------------------------------
class BaseCharacter : public Collider
{
protected: /// ---------- 構造体 ---------- ///

	/// ---------- 部位データ構造体 ---------- ///
	struct BodyPart
	{
		std::unique_ptr<Object3D> object;
		WorldTransform transform;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	virtual void Initialize();

	// 更新処理
	virtual void Update();

	// 描画処理
	virtual void Draw();

	// ワールド変換の取得
	const WorldTransform* GetWorldTransform() { return &body_.transform; }

	// 衝突時に呼ばれる仮想関数
	virtual void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	virtual Vector3 GetCenterPosition() const override;

protected: /// ---------- メンバ変数 ---------- ///

	// 入力
	Input* input_ = nullptr;

	// カメラ
	Camera* camera_ = nullptr;

	// 体（親）
	BodyPart body_;

	// 他の部位（子）
	std::vector<BodyPart> parts_;

	ParticleManager* particleManager_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;
};
