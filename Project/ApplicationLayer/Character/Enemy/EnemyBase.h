#pragma once
#include <memory>
#include <string>

#include "Collider.h"
#include "Object3D.h"
#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///                     EnemyBase
///  - HP / 描画 / Collider / 物理（位置・速度）
///  - 現状は 1体 = 1 collider なので Collider 継承でOK
/// -------------------------------------------------------------
class EnemyBase : public K4E::Collider
{
public:
	EnemyBase() = default;
	virtual ~EnemyBase() = default;

	virtual void Initialize(const K4E::Vector3& startPos, const std::string& modelPath = "cube.gltf");
	virtual void Update(float dt);
	virtual void Draw();
	virtual void DrawImGui();

	// HP
	void SetMaxHp(int v) { maxHp_ = v; hp_ = v; }
	int  GetHp() const { return hp_; }
	int  GetMaxHp() const { return maxHp_; }
	bool IsDead() const { return isDead_; }
	bool IsRemovable() const { return removable_; }

	// 物理
	void SetPosition(const K4E::Vector3& p);
	void SetVelocity(const K4E::Vector3& v) { velocity_ = v; }
	const K4E::Vector3& GetVelocity() const { return velocity_; }

	// Colliderとモデルを同期
	void SetCenterPosition(const K4E::Vector3& pos) override;

	// ダメージ
	virtual void TakeDamage(int amount);

	// Collider events
	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionStay(K4E::Collider* other) override { OnCollisionEnter(other); }
	void OnCollisionExit(K4E::Collider* other) override { (void)other; }

protected:
	virtual void OnKilled();
	virtual void OnBulletHit(K4E::Collider* bulletCollider);

protected:
	std::unique_ptr<K4E::Object3D> model_;

	int maxHp_ = 30;
	int hp_ = 30;

	bool isDead_ = false;
	bool removable_ = false;
	int deadFrames_ = 0;

	// 物理
	K4E::Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
	bool useGravity_ = false;
	float gravity_ = 19.6f;

	// OBB半サイズ
	K4E::Vector3 obbHalf_{ 1.0f, 1.0f, 1.0f };

private:
	void DisableColliderAndMoveFar();
};