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
///                     エネミークラス
/// -------------------------------------------------------------
class Enemy : public K4E::Collider
{
public: /// ---------- メンバ関数 ---------- ///

	Enemy() = default;
	void Initialize();
	void Update();
	void Draw();
	void DrawImGui();

	// 衝突状態（Enter/Exit）
	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionExit(K4E::Collider* other) override;

	// ダメージ
	void TakeDamage(int damage);
	int GetHP() const { return hp_; }

	void SetCenterPosition(const K4E::Vector3& pos) override;

	bool IsRemovable() const { return removable_; }
	bool IsDead() const { return isDead_; }

private:

	void KillAndDisableCollider();

private:
	K4E::Input* input_ = nullptr;

	K4E::Vector3 moveVelocity_ = { 0.0f, 0.0f, 0.0f };
	K4E::Vector4 debugColor_ = { 0.0f, 1.0f, 0.0f, 1.0f };

	std::unique_ptr<K4E::Object3D> model_ = nullptr;

	// 接触中の相手（色管理・多重接触対策）
	K4E::ContactRecord contactRecord_{};

	int hp_ = 10;

	bool isDead_ = false;
	bool removable_ = false;
	int deadFrames_ = 0;
};

