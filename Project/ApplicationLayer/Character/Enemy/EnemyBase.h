#pragma once
#include <memory>
#include <string>

#include "Collider.h"
#include "Object3D.h"
#include "Vector3.h"
#include "Vector4.h"

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
	virtual void SetColor(const K4E::Vector4& color)
	{
		// 基本色として保持（ヒットフラッシュ終了後に戻すため）
		baseColor_ = color;
		if (model_) model_->SetColor(color);
	}

	// ヒット時の赤点滅（被弾フラッシュ）
	void EnableHitFlash(bool enable) { hitFlashEnabled_ = enable; }
	void SetHitFlashDuration(float sec) { hitFlashDuration_ = sec; }
	void SetHitFlashFrequency(float hz) { hitFlashFrequencyHz_ = hz; }
	void SetHitFlashColor(const K4E::Vector4& c) { hitFlashColor_ = c; }
	void StartHitFlash();

	// Collider events
	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionStay(K4E::Collider* other) override { OnCollisionEnter(other); }
	void OnCollisionExit(K4E::Collider* other) override { (void)other; }

protected:
	virtual void OnKilled();
	virtual void OnBulletHit(K4E::Collider* bulletCollider);

protected:
	std::unique_ptr<K4E::Object3D> model_;

	int maxHp_ = 240;
	int hp_ = 240;

	bool isDead_ = false;
	bool removable_ = false;
	int deadFrames_ = 0;

	// 物理
	K4E::Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
	bool useGravity_ = false;
	float gravity_ = 19.6f;

	// OBB半サイズ
	K4E::Vector3 obbHalf_{ 1.0f, 1.0f, 1.0f };

	// ---- Hit flash ----
	K4E::Vector4 baseColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	K4E::Vector4 hitFlashColor_{ 1.0f, 0.0f, 0.0f, 1.0f };
	float hitFlashTimer_ = 0.0f;
	float hitFlashDuration_ = 0.12f;     // 秒
	float hitFlashFrequencyHz_ = 18.0f;  // 点滅周波数
	bool hitFlashEnabled_ = true;

private:
	void DisableColliderAndMoveFar();
	void UpdateHitFlash(float dt);
};