#pragma once
#include <memory>
#include <cstdint>
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

/// ---------- 前方宣言 ---------- ///
class EnemyParticleEffectSystem;

/// -------------------------------------------------------------
/// EnemyBase
///  - HP / 描画 / Collider / 物理（位置・速度）
///  - 見た目は BaseCharacter 相当の人型パーツで管理
///  - 死亡時は「バラバラ崩壊」演出（簡易物理）
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

	virtual void Initialize();
	virtual void Update(float deltaTime);
	virtual void Draw();
	virtual void DrawImGui();
	virtual void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);
	virtual void DrawShadow();

public:
	// HP
	void SetMaxHp(int v) { maxHp_ = v; hp_ = v; }
	// 現在HPを安全な範囲へ補正して設定する
	void SetCurrentHp(int v);
	// 現在HPを返す
	int GetHp() const { return hp_; }

	// 最大HPを返す
	int GetMaxHp() const { return maxHp_; }

	// 死亡しているかどうか
	bool IsDead() const { return isDead_; }

	// HPバーを表示したいワールド座標を返す
	// 頭の少し上を想定
	K4E::Vector3 GetHpBarWorldPosition() const
	{
		// ここは敵の中心座標を基準にして上へオフセットする
		// GetWorldPosition() / GetCenterPosition() など、今使っている関数に合わせてください
		K4E::Vector3 pos = GetCenterPosition();

		// 頭上に表示するための高さ調整
		// 人型なら 2.5f～3.5f くらいを試すと良い
		pos.y += 3.0f;

		return pos;
	}

	float GetHpRate() const
	{
		if (maxHp_ <= 0) { return 0.0f; }
		return static_cast<float>(hp_) / static_cast<float>(maxHp_);
	}

	bool IsHpBarVisibleTarget() const { return !isDead_; }

	bool IsRemovable() const { return removable_; }

	// 物理（生存中のCollider用）
	void SetPosition(const K4E::Vector3& p);
	void SetVelocity(const K4E::Vector3& v) { velocity_ = v; }
	const K4E::Vector3& GetVelocity() const { return velocity_; }

	// Colliderと見た目を同期
	void SetCenterPosition(const K4E::Vector3& pos) override;

	// 見た目の向き
	void SetOrientation(const K4E::Vector3& rot);

	// ダメージ
	// 既存互換：方向なし（従来通り呼べる）
	virtual void TakeDamage(int amount);
	// 新：被弾方向（弾の進行方向）と強さ（演出用）を渡せる
	virtual void TakeDamage(int amount, const K4E::Vector3& hitDir, float hitPower);

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
	static void SetGlobalStageFloorAABBs(const std::vector<K4E::AABB>* aabbs);
	static void SetGlobalStageNavigationObstacleAABBs(const std::vector<K4E::AABB>* aabbs);
	static constexpr float GetMaxUpdateDeltaTime() { return kMaxUpdateDeltaTime; }
	static constexpr bool IsGroundSnapEnabled() { return true; }
	static constexpr bool IsWorldBoundsEnabled() { return true; }
	static constexpr float GetMaxPushOutPerFrame() { return kMaxPushOutPerFrame; }
	int GetStuckDetectionCount() const { return stuckDetectionCount_; }
	int GetStuckRecoveryCount() const { return stuckRecoveryCount_; }

	// 参照用
	BodyPart& GetBody() { return body_; }
	std::vector<BodyPart>& GetBodyParts() { return parts_; }
	const PartIndices& GetPartIndices() const { return partIndices_; }

	void SetParticleEffectSystem(EnemyParticleEffectSystem* effectSystem) { particleEffectSystem_ = effectSystem; }

	void SpawnHitEffectAt(const K4E::Vector3& worldPos);

	const std::vector<K4E::AABB>* GetResolvedWorldAABBs() const { return worldAABBs_ ? worldAABBs_ : g_worldAABBs_; }
	const std::vector<K4E::AABB>* GetResolvedNavigationObstacleAABBs() const { return g_navigationObstacleAABBs_ ? g_navigationObstacleAABBs_ : GetResolvedWorldAABBs(); }

protected:
	// 派生で差し替え可（デフォルトはバラバラ崩壊開始）
	virtual void OnKilled();
	virtual void OnBulletHit(K4E::Collider* bulletCollider);

	// 見た目初期化
	void InitializeHumanoidVisual();
	void UpdateVisualHierarchy();
	void SetVisualColorAll(const K4E::Vector4& color);
	void MoveVisualFar(const K4E::Vector3& pos);
	K4E::Vector3 CorrectSpawnPosition(const K4E::Vector3& requestedPosition) const;
	float FindGroundY(const K4E::Vector3& position) const;
	bool OverlapsNavigationObstacle(const K4E::Vector3& center) const;

private:
	// コライダーだけ無効化（見た目は残す）
	void DisableColliderOnly();
	// ヒットフラッシュ
	void UpdateHitFlash(float dt);

	// ---- death break apart ----
	struct DeathPiece
	{
		BodyPart* part = nullptr;
		K4E::Vector3 velocity{ 0, 0, 0 };
		K4E::Vector3 angularVel{ 0, 0, 0 };
		float hitBias = 0.5f; // 被弾方向の影響（部位ごとに調整）
	};

	void StartBreakApartDeath();
	void UpdateBreakApartDeath(float dt);
	void DetachAllPartsToWorldSpace();

protected:
	// ----- humanoid visual -----
	BodyPart body_;
	std::vector<BodyPart> parts_;
	PartIndices partIndices_{};
	K4E::Vector3 orientation_{ 0.0f, 0.0f, 0.0f };

	static constexpr float kMaxUpdateDeltaTime = 1.0f / 30.0f;
	static constexpr float kGroundY = 0.0f;
	static constexpr float kMaxPushOutPerFrame = 0.45f;
	static constexpr int kStuckRecoveryThreshold = 45;
	static constexpr float kWorldBoundsMinX = -100.0f;
	static constexpr float kWorldBoundsMaxX = 100.0f;
	static constexpr float kWorldBoundsMinZ = -100.0f;
	static constexpr float kWorldBoundsMaxZ = 100.0f;

	// HP
	int maxHp_ = 240;
	int hp_ = 240;

	bool isDead_ = false;
	bool removable_ = false;

	// 生存中の物理
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

	// 押し出し用（生存中だけ）
	K4E::WorldCollisionSettings worldCol_{};
	bool worldColOverride_ = false;
	bool useWorldResolve_ = true;
	bool grounded_ = false;
	K4E::Vector3 spawnPosition_{};
	K4E::Vector3 lastSafePosition_{};
	int consecutivePushOutFrames_ = 0;
	int stuckDetectionCount_ = 0;
	int stuckRecoveryCount_ = 0;

	// ---- last hit info (for death impulse) ----
	K4E::Vector3 lastHitDir_{ 0.0f, 0.0f, 0.0f };
	float lastHitPower_ = 1.0f;
	float lastHitUpPower_ = 2.0f;

	// ---- break apart sim ----
	bool deathBreakActive_ = false;
	float deathTimer_ = 0.0f;
	float deathSimDuration_ = 1.8f;   // 破片が残る時間
	float deathFadeDuration_ = 0.6f;  // 最後にフェードする時間
	float deathLinearDamping_ = 2.0f; // 大きいほどすぐ止まる
	float deathAngularDamping_ = 2.5f;
	float deathBounce_ = 0.25f;       // 0..1
	float deathFriction_ = 0.7f;      // 0..1
	float deathGroundY_ = 0.0f;       // とりあえず床はY=0想定（必要なら拡張）
	std::vector<DeathPiece> deathPieces_;

	EnemyParticleEffectSystem* particleEffectSystem_ = nullptr;

private:
	static const std::vector<K4E::AABB>* g_worldAABBs_;
	static const std::vector<K4E::AABB>* g_floorAABBs_;
	static const std::vector<K4E::AABB>* g_navigationObstacleAABBs_;
};
