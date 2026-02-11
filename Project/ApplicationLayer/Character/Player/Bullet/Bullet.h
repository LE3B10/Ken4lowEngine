#pragma once
#include "Collider.h"
#include "ContactRecord.h"
#include "Object3D.h"
#include <Vector3.h>
#include <Vector4.h>

#include <memory>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine { class Input; }

/// -------------------------------------------------------------
///							弾クラス
/// -------------------------------------------------------------
class Bullet : public K4E::Collider
{
public: /// ---------- メンバ関数 ---------- ///

	Bullet() = default;

	// 生成（startPos: 生成位置, velocity: 1フレームあたりの移動量）
	void Initialize(const K4E::Vector3& startPos, const K4E::Vector3& velocity, int damage = 1, float lifeTimeSec = 3.0f);

	void Update(float dt);
	void Draw();
	void DrawImGui();

	// 衝突状態（Enter/Stay/Exit）
	void OnCollisionEnter(K4E::Collider* other) override;

	bool IsDead() const { return isDead_; }
	bool IsRemovable() const { return removable_; }
	int GetDamage() const { return damage_; }

private: /// ---------- メンバ関数 ---------- ///

	// 即死して遠くへ移動させる（衝突時など）
	void KillAndMoveFar();

private: /// ---------- メンバ変数 ---------- ///

	K4E::Vector3 moveVelocity_ = { 0.0f, 0.0f, 0.0f };
	K4E::Vector4 debugColor_ = { 1.0f, 1.0f, 0.0f, 1.0f };

	std::unique_ptr<K4E::Object3D> model_ = nullptr;

	// 接触中の相手ID（多段ヒット防止用）
	K4E::ContactRecord contactRecord_{};

	// 弾の状態
	bool isDead_ = false;
	bool removable_ = false;
	int deadFrames_ = 0; // Exit解決のため 1フレーム猶予

	int damage_ = 1;
	float lifeTimer_ = 0.0f;
	float lifeTimeSec_ = 3.0f;

	K4E::Vector3 prevPos_ = { 0.0f, 0.0f, 0.0f };
};

