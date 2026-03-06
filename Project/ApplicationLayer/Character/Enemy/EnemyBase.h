#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Collider.h"
#include "Object3D.h"
#include "WorldTransformEx.h"
#include "Vector3.h"
#include "Vector4.h"

#include "AABB.h"
#include "WorldCollisionResolver.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// EnemyBase
///  - HP / 描画 / Collider / 物理（位置・速度）
///  - 見た目は BaseCharacter 相当の人型パーツで管理
/// -------------------------------------------------------------
class EnemyBase : public K4E::Collider
{
public:
	struct BodyPart
	{
		std::unique_ptr<K4E::Object3D> object;
		K4E::WorldTransformEx transform;
		bool active = true;
	};

	struct PartIndices
	{
		const uint32_t head = 0;
		const uint32_t leftArm = 1;
		const uint32_t rightArm = 2;
		const uint32_t leftLeg = 3;
		const uint32_t rightLeg = 4;
	};

public:
	EnemyBase() = default;
	virtual ~EnemyBase() = default;

	virtual void Initialize(const K4E::Vector3& startPos, const std::string& modelPath = "cube.gltf");
	virtual void Update(float dt);
	virtual void Draw();
	virtual void DrawImGui();
	virtual void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);
	virtual void DrawShadow();

public:
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

	// Colliderと見た目を同期
	void SetCenterPosition(const K4E::Vector3& pos) override;

	// 見た目の向き
	void SetOrientation(const K4E::Vector3& rot);
	//const K4E::Vector3& GetOrientation() const { return orientation_; }

	// ダメージ
	virtual void TakeDamage(int amount);
	virtual void SetColor(const K4E::Vector4& color);

	// ヒット時の赤点滅
	void EnableHitFlash(bool enable) { hitFlashEnabled_ = enable; }
	void SetHitFlashDuration(float sec) { hitFlashDuration_ = sec; }
	void SetHitFlashFrequency(float hz) { hitFlashFrequencyHz_ = hz; }
	void SetHitFlashColor(const K4E::Vector4& c) { hitFlashColor_ = c; }
	void StartHitFlash();

	// Collider events
	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionStay(K4E::Collider* other) override { OnCollisionEnter(other); }
	void OnCollisionExit(K4E::Collider* other) override { (void)other; }

	static void SetGlobalStageWorldAABBs(const std::vector<K4E::AABB>* aabbs);

	// 参照用
	BodyPart& GetBody() { return body_; }
	std::vector<BodyPart>& GetBodyParts() { return parts_; }
	const PartIndices& GetPartIndices() const { return partIndices_; }

protected:
	virtual void OnKilled();
	virtual void OnBulletHit(K4E::Collider* bulletCollider);

	// 見た目初期化
	void InitializeHumanoidVisual();
	void UpdateVisualHierarchy();
	void SetVisualColorAll(const K4E::Vector4& color);
	void MoveVisualFar(const K4E::Vector3& pos);

private:
	void DisableColliderAndMoveFar();
	void UpdateHitFlash(float dt);

protected:
	// ----- humanoid visual -----
	BodyPart body_;
	std::vector<BodyPart> parts_;
	PartIndices partIndices_{};
	K4E::Vector3 orientation_{ 0.0f, 0.0f, 0.0f };

	// HP
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
	K4E::Vector3 obbHalf_{ 1.0f, 2.0f, 1.0f };

	// Hit flash
	K4E::Vector4 baseColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	K4E::Vector4 hitFlashColor_{ 1.0f, 0.0f, 0.0f, 1.0f };
	float hitFlashTimer_ = 0.0f;
	float hitFlashDuration_ = 0.12f;
	float hitFlashFrequencyHz_ = 18.0f;
	bool hitFlashEnabled_ = true;

	// stage AABB
	const std::vector<K4E::AABB>* worldAABBs_ = nullptr;

	// 押し出し用
	K4E::WorldCollisionSettings worldCol_{};
	bool worldColOverride_ = false;
	bool useWorldResolve_ = true;
	bool grounded_ = false;

private:
	static const std::vector<K4E::AABB>* g_worldAABBs_;
};