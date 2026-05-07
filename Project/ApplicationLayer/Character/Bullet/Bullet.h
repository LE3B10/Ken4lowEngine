#pragma once
#include "Collider.h"
#include "ContactRecord.h"
#include "Object3D.h"
#include "CollisionTypeIdDef.h"
#include <Vector3.h>
#include <Vector4.h>

#include <memory>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine { class Input; }
class CollisionManager;

/// -------------------------------------------------------------
///							弾クラス
/// -------------------------------------------------------------
class Bullet : public K4E::Collider
{
public: /// ---------- メンバ関数 ---------- ///

	Bullet() = default;

	// 生成（startPos: 生成位置, velocity: 速度, typeId: CollisionTypeIdDef の弾種）
	void Initialize(const K4E::Vector3& startPos,
		const K4E::Vector3& velocity,
		int damage = 1,
		float lifeTimeSec = 3.0f,
		const K4E::Vector3& shooterPosition = { 0.0f, 0.0f, 0.0f },
		uint32_t shooterColliderId = 0u,
		uint32_t typeId = static_cast<uint32_t>(CollisionTypeIdDef::kBullet)
	);

	void Update(float dt);
	void Draw();
	void DrawImGui();

	// 衝突状態（Enter/Stay/Exit）
	void OnCollisionEnter(K4E::Collider* other) override;

	bool IsDead() const { return isDead_; }
	bool IsRemovable() const { return removable_; }
	int GetDamage() const { return damage_; }

	void SetShooterPosition(const K4E::Vector3& pos) { shooterPosition_ = pos; }
	const K4E::Vector3& GetShooterPosition() const { return shooterPosition_; }
	void SetShooterColliderId(uint32_t id) { shooterColliderId_ = id; }
	uint32_t GetShooterColliderId() const { return shooterColliderId_; }

	void SetCollisionManager(CollisionManager* collisionManager) { collisionManager_ = collisionManager; }

	// 弾のObject3D描画だけを制御する。
	// 当たり判定・移動・寿命には影響させない。
	void SetModelDrawEnabled(bool enabled) { drawModel_ = enabled; }
	bool IsModelDrawEnabled() const { return drawModel_; }

	// 範囲ダメージ設定。radius <= 0 の場合は通常弾として扱う。
	void ConfigureSplashDamage(float radius, int damage, bool canDamageSelf = false);
	bool HasSplashDamage() const { return splashRadius_ > 0.0f && splashDamage_ > 0; }
	float GetSplashRadius() const { return splashRadius_; }
	int GetSplashDamage() const { return splashDamage_; }

private: /// ---------- メンバ関数 ---------- ///

	// 即死して遠くへ移動させる（衝突時など）
	void KillAndMoveFar();
	void TriggerSplashDamageAt(const K4E::Vector3& center);
	void ApplySplashDamageToType(uint32_t targetType, const K4E::Vector3& center);

private: /// ---------- メンバ変数 ---------- ///

	K4E::Vector3 moveVelocity_ = { 0.0f, 0.0f, 0.0f };
	K4E::Vector4 debugColor_ = { 1.0f, 1.0f, 0.0f, 1.0f };

	K4E::Vector3 shooterPosition_ = { 0.0f, 0.0f, 0.0f };
	uint32_t shooterColliderId_ = 0u;

	CollisionManager* collisionManager_ = nullptr;
	float splashRadius_ = 0.0f;
	int splashDamage_ = 0;
	bool splashCanDamageSelf_ = false;
	bool splashTriggered_ = false;

	std::unique_ptr<K4E::Object3D> model_ = nullptr;
	bool drawModel_ = true;

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
	K4E::Vector3 scale_ = { 0.1f, 0.1f, 0.1f };
};

