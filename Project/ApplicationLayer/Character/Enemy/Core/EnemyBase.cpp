#define NOMINMAX
#include "EnemyBase.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include "EnemyParticleEffectSystem.h"
#include <Bullet.h>

#include "CollisionTypeIdDef.h"
#include "CollisionPreset.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI


using namespace Ken4lowEngine;

const std::vector<Ken4lowEngine::AABB>* EnemyBase::g_worldAABBs_ = nullptr;
const std::vector<Ken4lowEngine::AABB>* EnemyBase::g_floorAABBs_ = nullptr;
const std::vector<Ken4lowEngine::AABB>* EnemyBase::g_navigationObstacleAABBs_ = nullptr;
float EnemyBase::s_spawnYOffset_ = 0.15f;
bool EnemyBase::s_deathExplosionEnabled_ = true;
float EnemyBase::s_deathExplodePower_ = 3.0f;
float EnemyBase::s_deathUpwardPower_ = 1.4f;
float EnemyBase::s_deathMaxSpeed_ = 7.0f;
float EnemyBase::s_deathMaxAngularSpeed_ = 5.0f;
float EnemyBase::s_deathPieceLifetime_ = 1.8f;
Vector3 EnemyBase::s_lastDebugPlayerPosition_ = { 0.0f, 0.0f, 0.0f };
Vector3 EnemyBase::s_lastDebugAttackCenter_ = { 0.0f, 0.0f, 0.0f };

namespace
{
	static float Clamp01(float v)
	{
		if (v < 0.0f) return 0.0f;
		if (v > 1.0f) return 1.0f;
		return v;
	}

	static float Length(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	static Vector3 NormalizeSafe(const Vector3& v, const Vector3& fallback = { 0.0f, 1.0f, 0.0f })
	{
		const float len = Length(v);
		if (len < 1e-6f) return fallback;
		return v * (1.0f / len);
	}

	static float Rand01()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	static float RandRange(float a, float b)
	{
		return a + (b - a) * Rand01();
	}

	static Vector3 RandomUnit()
	{
		// 簡易：立方体→正規化
		Vector3 v{ RandRange(-1.0f, 1.0f), RandRange(-1.0f, 1.0f), RandRange(-1.0f, 1.0f) };
		return NormalizeSafe(v, { 0.0f, 1.0f, 0.0f });
	}

	static Vector3 ClampVectorLength(const Vector3& v, float maxLength)
	{
		const float safeMaxLength = std::max(0.0f, maxLength);
		const float len = Length(v);
		if (len <= safeMaxLength || len < 1e-6f)
		{
			return v;
		}
		return v * (safeMaxLength / len);
	}

	static bool IsNearlyZeroVector(const Vector3& v)
	{
		return Length(v) < 1e-4f;
	}
}

/// -------------------------------------------------------------
/// 人型見た目の初期化
/// -------------------------------------------------------------
void EnemyBase::InitializeHumanoidVisual()
{
	body_ = {};
	parts_.clear();

	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Characters/body.gltf");
	body_.transform.translate_ = { 0.0f, 0.0f, 0.0f };
	body_.transform.rotate_ = orientation_;

	body_.object->SetTextureForAll("Characters/enemy.dds");

	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{ "Characters/head.gltf", { 0.0f, 0.75f, 0.0f } },
		{ "Characters/left_arm.gltf", { -0.75f, 0.75f, 0.0f } },
		{ "Characters/right_arm.gltf", { 0.75f, 0.75f, 0.0f } },
		{ "Characters/left_leg.gltf", { -0.25f, -0.75f, 0.0f } },
		{ "Characters/right_leg.gltf", { 0.25f, -0.75f, 0.0f } },
	};

	for (const auto& [modelPath, localPos] : partData)
	{
		BodyPart part{};
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.object->SetTextureForAll("Characters/enemy.dds");
		part.transform.translate_ = localPos;
		part.transform.parent_ = &body_.transform;
		parts_.push_back(std::move(part));
	}
}

/// -------------------------------------------------------------
/// 見た目階層更新（生存中用）
/// -------------------------------------------------------------
void EnemyBase::UpdateVisualHierarchy()
{
	if (!body_.object) return;

	body_.transform.rotate_ = orientation_;
	body_.transform.Update();

	body_.object->SetTranslate(body_.transform.translate_);
	body_.object->SetRotate(body_.transform.rotate_);
	body_.object->Update();

	for (auto& part : parts_)
	{
		if (!part.object) continue;

		part.transform.parent_ = &body_.transform;
		part.transform.worldRotate_ = body_.transform.worldRotate_;
		part.transform.Update();

		part.object->SetTranslate(part.transform.worldTranslate_);
		part.object->SetRotate(part.transform.worldRotate_);
		part.object->Update();
	}
}

/// -------------------------------------------------------------
/// 色を全部位へ適用
/// -------------------------------------------------------------
void EnemyBase::SetVisualColorAll(const Vector4& color)
{
	if (body_.object) body_.object->SetColor(color);

	for (auto& part : parts_)
	{
		if (part.object)
		{
			part.object->SetColor(color);
		}
	}
}

/// -------------------------------------------------------------
/// 全見た目を遠方へ移動（※現在は未使用。互換用に残置）
/// -------------------------------------------------------------
void EnemyBase::MoveVisualFar(const Vector3& pos)
{
	if (body_.object)
	{
		body_.transform.translate_ = pos;
		UpdateVisualHierarchy();
	}
}

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void EnemyBase::Initialize()
{
	ApplyCollisionPreset(*this, ECollisionPresetId::Enemy);
	SetOwner(this);

	SetOBBHalfSize(obbHalf_);
	SetSegment(Segment{});

	InitializeHumanoidVisual();
	SetColor(baseColor_);
	hitFlashTimer_ = 0.0f;

	SetCenterPosition({ 0.0f, 0.0f, 0.0f });

	isDead_ = false;
	removable_ = false;

	deathBreakActive_ = false;
	deathBreakInitialized_ = false;
	hasDeathEffectOrigin_ = false;
	hasDeathPartWorldTransforms_ = false;
	deathUsesMidRangeSuicideCollapseStyle_ = true;
	deathEffectInitializeCount_ = 0;
	deathEnemyPosition_ = GetCenterPosition();
	deathEffectOrigin_ = deathEnemyPosition_;
	deathEffectRotation_ = orientation_;
	deathDebugPlayerPosition_ = s_lastDebugPlayerPosition_;
	deathDebugAttackCenter_ = s_lastDebugAttackCenter_;
	deathInitialBodyPosition_ = deathEffectOrigin_;
	deathInitialBodyRotation_ = deathEffectRotation_;
	deathInitialPartPositions_.clear();
	deathInitialPartRotations_.clear();
	deathInitialPartLocalOffsets_.clear();
	deathDrawBodyPosition_ = deathEffectOrigin_;
	deathDrawPartPositions_.clear();
	deathTimer_ = 0.0f;
	deathPieces_.clear();
	lastHitDir_ = { 0, 0, 0 };
	lastHitPower_ = 1.0f;

	hp_ = maxHp_;

	useGravity_ = true;
	worldCol_.half = obbHalf_;
	worldCol_.centerOffset = { 0, 0, 0 };
	worldColOverride_ = true;
	spawnPosition_ = CorrectSpawnPosition(GetCenterPosition());
	lastSafePosition_ = spawnPosition_;
	consecutivePushOutFrames_ = 0;
	stuckDetectionCount_ = 0;
	stuckRecoveryCount_ = 0;
}

/// -------------------------------------------------------------
/// 中心座標
/// -------------------------------------------------------------
void EnemyBase::SetCenterPosition(const Vector3& pos)
{
	Collider::SetCenterPosition(pos);
	body_.transform.translate_ = pos;

	// 死亡演出中は階層に戻したくない
	if (!deathBreakActive_)
	{
		UpdateVisualHierarchy();
	}
}

/// -------------------------------------------------------------
/// 位置
/// -------------------------------------------------------------
void EnemyBase::SetPosition(const Vector3& p)
{
	// 初期配置で地面や障害物に埋まらないよう、足元と簡易障害物重なりを補正する。
	spawnPosition_ = CorrectSpawnPosition(p);
	lastSafePosition_ = spawnPosition_;
	SetCenterPosition(spawnPosition_);
}

float EnemyBase::FindGroundY(const Vector3& position) const
{
	float groundY = kGroundY;
	if (!g_floorAABBs_) { return groundY; }
	for (const AABB& floor : *g_floorAABBs_)
	{
		if (position.x >= floor.min.x - obbHalf_.x && position.x <= floor.max.x + obbHalf_.x &&
			position.z >= floor.min.z - obbHalf_.z && position.z <= floor.max.z + obbHalf_.z &&
			floor.max.y <= position.y + obbHalf_.y + 0.5f)
		{
			groundY = std::max(groundY, floor.max.y);
		}
	}
	return groundY;
}

bool EnemyBase::OverlapsNavigationObstacle(const Vector3& center) const
{
	const auto* obstacles = GetResolvedNavigationObstacleAABBs();
	if (!obstacles) { return false; }
	for (const AABB& obstacle : *obstacles)
	{
		if (center.x + obbHalf_.x > obstacle.min.x && center.x - obbHalf_.x < obstacle.max.x &&
			center.y + obbHalf_.y > obstacle.min.y && center.y - obbHalf_.y < obstacle.max.y &&
			center.z + obbHalf_.z > obstacle.min.z && center.z - obbHalf_.z < obstacle.max.z)
		{
			return true;
		}
	}
	return false;
}

Vector3 EnemyBase::CorrectSpawnPosition(const Vector3& requestedPosition) const
{
	const Vector3 offsets[] = {
		{ 0.0f, 0.0f, 0.0f }, { 2.5f, 0.0f, 0.0f }, { -2.5f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 2.5f }, { 0.0f, 0.0f, -2.5f }, { 3.5f, 0.0f, 3.5f },
		{ -3.5f, 0.0f, 3.5f }, { 3.5f, 0.0f, -3.5f }, { -3.5f, 0.0f, -3.5f }
	};
	for (const Vector3& offset : offsets)
	{
		Vector3 candidate = requestedPosition + offset;
		// 地面への埋まりを防ぐため、スポーン時にモデルの高さ分と微小なY補正を足す。
		candidate.y = FindGroundY(candidate) + obbHalf_.y + s_spawnYOffset_;
		if (!OverlapsNavigationObstacle(candidate)) { return candidate; }
	}
	Vector3 fallback = requestedPosition;
	fallback.y = FindGroundY(fallback) + obbHalf_.y + s_spawnYOffset_;
	return fallback;
}

/// -------------------------------------------------------------
/// 向き
/// -------------------------------------------------------------
void EnemyBase::SetOrientation(const Vector3& rot)
{
	orientation_ = rot;

	if (!deathBreakActive_)
	{
		UpdateVisualHierarchy();
	}
}

/// -------------------------------------------------------------
/// 色
/// -------------------------------------------------------------
void EnemyBase::SetColor(const Vector4& color)
{
	baseColor_ = color;
	SetVisualColorAll(color);
}

/// -------------------------------------------------------------
/// 更新
/// -------------------------------------------------------------
void EnemyBase::Update(float deltaTime)
{
	if (removable_) return;

	// フレーム落ち時の移動暴走を防ぐため、敵更新用deltaTimeを制限する。
	deltaTime = std::clamp(deltaTime, 0.0f, kMaxUpdateDeltaTime);

	// 死亡演出
	if (isDead_)
	{
		UpdateBreakApartDeath(deltaTime);
		return;
	}

	grounded_ = false;

	if (useGravity_) velocity_.y -= gravity_ * deltaTime;

	const Vector3 oldPos = GetCenterPosition();
	Vector3 newPos = oldPos + velocity_ * deltaTime;

	const auto* aabbs = (worldAABBs_ ? worldAABBs_ : g_worldAABBs_);
	const auto& s = worldColOverride_ ? worldCol_ : worldCol_;

	if (useWorldResolve_ && aabbs && !aabbs->empty())
	{
		float vy = velocity_.y;

		auto res = Ken4lowEngine::WorldCollisionResolver::Resolve(
			*aabbs,
			s,
			oldPos,
			newPos,
			true,
			&vy
		);

		const Vector3 desiredCenter = newPos - s.centerOffset;
		if (std::fabs(res.fixedCenter.x - desiredCenter.x) > 0.0001f) velocity_.x = 0.0f;
		if (std::fabs(res.fixedCenter.z - desiredCenter.z) > 0.0001f) velocity_.z = 0.0f;

		velocity_.y = vy;
		grounded_ = res.grounded;
		const Vector3 resolvedPos = res.fixedCenter + s.centerOffset;
		Vector3 pushOut = resolvedPos - newPos;
		pushOut.y = 0.0f; // 地面補正はY、壁・障害物の押し出しはXZとして分離する。
		const float pushOutLength = std::sqrt(pushOut.x * pushOut.x + pushOut.z * pushOut.z);
		if (pushOutLength > kMaxPushOutPerFrame)
		{
			// 押し出し暴走を防ぐため、1フレームのXZ補正量を制限する。
			const float scale = kMaxPushOutPerFrame / pushOutLength;
			pushOut.x *= scale;
			pushOut.z *= scale;
		}
		newPos.x += pushOut.x;
		newPos.z += pushOut.z;
		newPos.y = resolvedPos.y;

		if (pushOutLength > 0.0001f)
		{
			++consecutivePushOutFrames_;
			++stuckDetectionCount_;
			velocity_.x = 0.0f;
			velocity_.z = 0.0f;
		}
		else
		{
			consecutivePushOutFrames_ = 0;
			lastSafePosition_ = newPos;
		}
	}

	if (consecutivePushOutFrames_ >= kStuckRecoveryThreshold)
	{
		newPos = !OverlapsNavigationObstacle(lastSafePosition_) ? lastSafePosition_ : spawnPosition_;
		velocity_ = {};
		consecutivePushOutFrames_ = 0;
		++stuckRecoveryCount_;
	}

	// 仮の安全処理として、敵の足元が基準床Yより下なら地面上へ戻す。
	const float minimumCenterY = kGroundY + obbHalf_.y;
	if (newPos.y < minimumCenterY)
	{
		newPos.y = minimumCenterY;
		velocity_.y = 0.0f;
		grounded_ = true;
	}

	// ステージ外へ落ち続けないよう、生存中の敵XZ座標を安全範囲へ制限する。
	newPos.x = std::clamp(newPos.x, kWorldBoundsMinX, kWorldBoundsMaxX);
	newPos.z = std::clamp(newPos.z, kWorldBoundsMinZ, kWorldBoundsMaxZ);

	SetCenterPosition(newPos);
	UpdateHitFlash(deltaTime);
}

/// -------------------------------------------------------------
/// 描画
/// -------------------------------------------------------------
void EnemyBase::Draw()
{
	if (removable_) return;

	// isDead_ でも deathBreakActive_ 中は描画する
	if (isDead_ && !deathBreakActive_) return;

	if (body_.active && body_.object)
	{
		deathDrawBodyPosition_ = body_.object->GetTranslate();
		body_.object->Draw();
	}

	if (deathDrawPartPositions_.size() != parts_.size())
	{
		deathDrawPartPositions_.assign(parts_.size(), {});
	}
	for (size_t i = 0; i < parts_.size(); ++i)
	{
		const auto& part = parts_[i];
		if (part.active && part.object)
		{
			deathDrawPartPositions_[i] = part.object->GetTranslate();
			part.object->Draw();
		}
	}
}

/// -------------------------------------------------------------
/// ImGui描画
/// -------------------------------------------------------------
void EnemyBase::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Text("敵死亡座標: %.2f, %.2f, %.2f", deathEnemyPosition_.x, deathEnemyPosition_.y, deathEnemyPosition_.z);
	ImGui::Text("死亡演出原点: %.2f, %.2f, %.2f", deathEffectOrigin_.x, deathEffectOrigin_.y, deathEffectOrigin_.z);
	ImGui::Text("胴体初期座標: %.2f, %.2f, %.2f", deathInitialBodyPosition_.x, deathInitialBodyPosition_.y, deathInitialBodyPosition_.z);
	ImGui::SeparatorText("死亡部位 Draw直前WorldTransform.translation");
	ImGui::Text("deathOrigin: %.2f, %.2f, %.2f", deathEffectOrigin_.x, deathEffectOrigin_.y, deathEffectOrigin_.z);
	ImGui::Text("body.worldTransform.translation: %.2f, %.2f, %.2f", deathDrawBodyPosition_.x, deathDrawBodyPosition_.y, deathDrawBodyPosition_.z);
	const char* deathPartLabels[] = { "head", "leftArm", "rightArm", "leftLeg", "rightLeg" };
	for (size_t i = 0; i < deathDrawPartPositions_.size() && i < 5; ++i)
	{
		const Vector3& p = deathDrawPartPositions_[i];
		ImGui::Text("%s.worldTransform.translation: %.2f, %.2f, %.2f", deathPartLabels[i], p.x, p.y, p.z);
	}
	ImGui::SeparatorText("死亡部位 初期World座標");
	for (size_t i = 0; i < deathInitialPartPositions_.size() && i < 5; ++i)
	{
		const Vector3& p = deathInitialPartPositions_[i];
		ImGui::Text("%s初期座標: %.2f, %.2f, %.2f", deathPartLabels[i], p.x, p.y, p.z);
	}
	ImGui::Text("死亡演出初期化回数: %d", deathEffectInitializeCount_);
	ImGui::Text("中距離自爆崩壊処理を使用中: %s", deathUsesMidRangeSuicideCollapseStyle_ ? "はい" : "いいえ");
	ImGui::Text("プレイヤー座標(比較のみ): %.2f, %.2f, %.2f", deathDebugPlayerPosition_.x, deathDebugPlayerPosition_.y, deathDebugPlayerPosition_.z);
	ImGui::Text("攻撃判定中心(比較のみ): %.2f, %.2f, %.2f", deathDebugAttackCenter_.x, deathDebugAttackCenter_.y, deathDebugAttackCenter_.z);
	ImGui::Text("DeathHitPower: %.2f Up: %.2f", lastHitPower_, lastHitUpPower_);
	ImGui::SeparatorText("死亡部位 爆散調整");
	ImGui::Checkbox("死亡部位 爆散有効", &s_deathExplosionEnabled_);
	ImGui::SliderFloat("死亡部位 爆散力", &s_deathExplodePower_, 0.0f, 8.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 上方向力", &s_deathUpwardPower_, 0.0f, 5.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 最大速度", &s_deathMaxSpeed_, 0.5f, 12.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 最大回転速度", &s_deathMaxAngularSpeed_, 0.5f, 10.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 寿命", &s_deathPieceLifetime_, 0.2f, 5.0f, "%.2f 秒");
	SetDeathExplodePower(s_deathExplodePower_);
	SetDeathUpwardPower(s_deathUpwardPower_);
	SetDeathMaxSpeed(s_deathMaxSpeed_);
	SetDeathMaxAngularSpeed(s_deathMaxAngularSpeed_);
	SetDeathPieceLifetime(s_deathPieceLifetime_);
	if (body_.object) body_.object->DrawImGui();

	for (auto& part : parts_)
	{
		if (part.object) part.object->DrawImGui();
	}
#endif // USE_IMGUI
}

void EnemyBase::UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
{
	if (body_.object) body_.object->UpdateShadowMatrix(lightViewProjection);

	for (auto& part : parts_)
	{
		if (part.object) part.object->UpdateShadowMatrix(lightViewProjection);
	}
}

void EnemyBase::DrawShadow()
{
	if (removable_) return;
	if (isDead_ && !deathBreakActive_) return;

	if (body_.active && body_.object)
	{
		body_.object->DrawShadow();
	}

	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->DrawShadow();
		}
	}
}

void EnemyBase::SetCurrentHp(int v)
{
	hp_ = std::max(0, std::min(v, maxHp_));
}

/// -------------------------------------------------------------
/// ダメージ（互換）
/// -------------------------------------------------------------
void EnemyBase::TakeDamage(int amount)
{
	// 方向なしは「最後に当たった方向」更新しない（= 既存挙動を壊さない）
	TakeDamage(amount, { 0.0f, 0.0f, 0.0f }, 50);
}

/// -------------------------------------------------------------
/// ダメージ（被弾方向つき）
/// hitDir: 弾の進行方向（正規化推奨）
/// hitPower: 演出用の強さ
/// -------------------------------------------------------------
void EnemyBase::TakeDamage(int amount, const Vector3& hitDir, float hitPower)
{
	if (isDead_) return;

	// 被弾情報を保存（死亡演出の初速に使う）
	const Vector3 nd = NormalizeSafe(hitDir, { 0.0f, 0.0f, 1.0f });
	if (Length(hitDir) > 1e-4f)
	{
		lastHitDir_ = nd;
	}
	lastHitPower_ = (hitPower > 0.0f) ? hitPower : 1.0f;

	StartHitFlash();
	hp_ -= amount;

	if (hp_ <= 0)
	{
		hp_ = 0;
		const Vector3 deathOrigin = GetCenterPosition();
		const Vector3 deathRotation = orientation_;
		CaptureDeathEffectOrigin(deathOrigin, deathRotation);
		isDead_ = true;
		removable_ = false;

		OnKilled();
		DisableColliderOnly();
	}
}

void EnemyBase::SetGlobalStageWorldAABBs(const std::vector<K4E::AABB>* aabbs)
{
	g_worldAABBs_ = aabbs;
}

void EnemyBase::SetGlobalStageFloorAABBs(const std::vector<K4E::AABB>* aabbs)
{
	g_floorAABBs_ = aabbs;
}

void EnemyBase::SetGlobalStageNavigationObstacleAABBs(const std::vector<K4E::AABB>* aabbs)
{
	g_navigationObstacleAABBs_ = aabbs;
}

void EnemyBase::SetSpawnYOffset(float offset)
{
	s_spawnYOffset_ = std::clamp(offset, 0.0f, 0.5f);
}

void EnemyBase::SetDeathExplosionEnabled(bool enabled)
{
	s_deathExplosionEnabled_ = enabled;
}

void EnemyBase::SetDeathExplodePower(float power)
{
	s_deathExplodePower_ = std::clamp(power, 0.0f, 12.0f);
}

void EnemyBase::SetDeathUpwardPower(float power)
{
	s_deathUpwardPower_ = std::clamp(power, 0.0f, 8.0f);
}

void EnemyBase::SetDeathMaxSpeed(float speed)
{
	s_deathMaxSpeed_ = std::clamp(speed, 0.5f, 20.0f);
}

void EnemyBase::SetDeathMaxAngularSpeed(float speed)
{
	s_deathMaxAngularSpeed_ = std::clamp(speed, 0.5f, 20.0f);
}

void EnemyBase::SetDeathPieceLifetime(float lifetime)
{
	s_deathPieceLifetime_ = std::clamp(lifetime, 0.2f, 5.0f);
}

void EnemyBase::SetDeathDebugComparePositions(const Vector3& playerPosition, const Vector3& attackCenter)
{
	s_lastDebugPlayerPosition_ = playerPosition;
	s_lastDebugAttackCenter_ = attackCenter;
}

void EnemyBase::SpawnHitEffectAt(const K4E::Vector3& worldPos)
{
	// 死亡済み/未初期化の敵から壊れたエフェクト参照を呼ばないようにする。
	if (isDead_ || removable_ || !particleEffectSystem_ || !particleEffectSystem_->IsInitialized())
	{
		return;
	}

	particleEffectSystem_->SpawnHitEffect(worldPos);
}

/// -------------------------------------------------------------
/// 死亡（デフォルトはバラバラ崩壊）
/// -------------------------------------------------------------
void EnemyBase::OnKilled()
{
	CaptureDeathEffectOrigin(deathEffectOrigin_, deathEffectRotation_);

	// 死亡パーティクル
	if (particleEffectSystem_ && particleEffectSystem_->IsInitialized())
	{
		particleEffectSystem_->SpawnDeathEffect(deathEffectOrigin_);
	}

	StartBreakApartDeath(deathEffectOrigin_, deathEffectRotation_);
}

/// -------------------------------------------------------------
/// コライダー無効化（見た目は残す）
/// -------------------------------------------------------------
void EnemyBase::DisableColliderOnly()
{
	SetEnabled(false);
	SetOBBHalfSize({ 0.0f, 0.0f, 0.0f });

	Segment s{};
	s.origin = { 0, 0, 0 };
	s.diff = { 0, 0, 0 };
	SetSegment(s);
}

/// -------------------------------------------------------------
/// ヒットフラッシュ開始
/// -------------------------------------------------------------
void EnemyBase::StartHitFlash()
{
	if (!hitFlashEnabled_) return;
	hitFlashTimer_ = hitFlashDuration_;
}

/// -------------------------------------------------------------
/// ヒットフラッシュ更新
/// -------------------------------------------------------------
void EnemyBase::UpdateHitFlash(float dt)
{
	if (hitFlashTimer_ > 0.0f)
	{
		hitFlashTimer_ -= dt;
		if (hitFlashTimer_ < 0.0f) hitFlashTimer_ = 0.0f;

		const float t = (hitFlashDuration_ > 0.0f) ? (hitFlashTimer_ / hitFlashDuration_) : 0.0f;
		const float elapsed = hitFlashDuration_ - hitFlashTimer_;
		const float phase = elapsed * hitFlashFrequencyHz_ * 6.28318530718f;
		const float blink = 0.5f * (1.0f + std::sinf(phase));
		const float a = blink * t;

		Vector4 c{};
		c.x = baseColor_.x + (hitFlashColor_.x - baseColor_.x) * a;
		c.y = baseColor_.y + (hitFlashColor_.y - baseColor_.y) * a;
		c.z = baseColor_.z + (hitFlashColor_.z - baseColor_.z) * a;
		c.w = baseColor_.w + (hitFlashColor_.w - baseColor_.w) * a;

		SetVisualColorAll(c);
	}
	else
	{
		SetVisualColorAll(baseColor_);
	}
}

/// -------------------------------------------------------------
/// death: 階層を外して「全部位をワールド空間」に固定
/// -------------------------------------------------------------
void EnemyBase::DetachAllPartsToWorldSpace()
{
	// まず階層更新して worldTranslate_ / worldRotate_ を確定させる
	UpdateVisualHierarchy();

	// body は元々ワールド
	body_.transform.parent_ = nullptr;
	body_.transform.Update();

	// パーツは world を local に持ち替えて親を切る
	for (auto& part : parts_)
	{
		part.transform.parent_ = nullptr;

		part.transform.translate_ = part.transform.worldTranslate_;
		part.transform.rotate_ = part.transform.worldRotate_;
		part.transform.Update();
	}
}

Vector3 EnemyBase::ResolveDeathOrigin(const Vector3& requestedOrigin)
{
	if (!IsNearlyZeroVector(requestedOrigin))
	{
		return requestedOrigin;
	}

	UpdateVisualHierarchy();
	if (!IsNearlyZeroVector(body_.transform.worldTranslate_))
	{
		return body_.transform.worldTranslate_;
	}
	if (!IsNearlyZeroVector(GetCenterPosition()))
	{
		return GetCenterPosition();
	}
	if (!IsNearlyZeroVector(lastSafePosition_))
	{
		return lastSafePosition_;
	}
	return spawnPosition_;
}

void EnemyBase::CaptureDeathEffectOrigin(const Vector3& deathOrigin, const Vector3& deathRotation)
{
	if (hasDeathEffectOrigin_)
	{
		return;
	}

	// 部位が原点へ飛ばないよう、敵の死亡時World座標を基準に崩壊演出を生成する。
	deathEnemyPosition_ = ResolveDeathOrigin(deathOrigin);
	deathEffectOrigin_ = deathEnemyPosition_;
	deathEffectRotation_ = deathRotation;
	deathDebugPlayerPosition_ = s_lastDebugPlayerPosition_;
	deathDebugAttackCenter_ = s_lastDebugAttackCenter_;
	CaptureDeathPartWorldTransforms();
	hasDeathEffectOrigin_ = true;
}

void EnemyBase::CaptureDeathPartWorldTransforms()
{
	if (hasDeathPartWorldTransforms_)
	{
		return;
	}

	// localOffsetをWorld座標として使わず、死亡瞬間のdeathOriginからDraw用World座標を再構築する。
	UpdateVisualHierarchy();
	deathInitialBodyPosition_ = body_.transform.worldTranslate_;
	deathInitialBodyRotation_ = body_.transform.worldRotate_;
	deathInitialPartPositions_.clear();
	deathInitialPartRotations_.clear();
	deathInitialPartLocalOffsets_.clear();
	deathInitialPartPositions_.reserve(parts_.size());
	deathInitialPartRotations_.reserve(parts_.size());
	deathInitialPartLocalOffsets_.reserve(parts_.size());
	for (auto& part : parts_)
	{
		const Vector3 localOffset = part.transform.parent_ ? part.transform.translate_ : (part.transform.translate_ - deathEffectOrigin_);
		deathInitialPartLocalOffsets_.push_back(localOffset);
		deathInitialPartPositions_.push_back(BuildDeathPartWorldPosition(localOffset));
		deathInitialPartRotations_.push_back(deathEffectRotation_ + part.transform.rotate_);
	}
	hasDeathPartWorldTransforms_ = true;
}

Vector3 EnemyBase::RotateLocalOffsetByDeathRotation(const Vector3& localOffset) const
{
	const Matrix4x4 rotationMatrix = Matrix4x4::MakeRotateMatrix(deathEffectRotation_);
	return Matrix4x4::Transform(localOffset, rotationMatrix);
}

Vector3 EnemyBase::BuildDeathPartWorldPosition(const Vector3& localOffset) const
{
	return deathEffectOrigin_ + RotateLocalOffsetByDeathRotation(localOffset);
}

/// -------------------------------------------------------------
/// death: 開始
/// -------------------------------------------------------------
void EnemyBase::StartBreakApartDeath(const Vector3& deathOrigin, const Vector3& deathRotation)
{
	if (deathBreakInitialized_)
	{
		return;
	}

	CaptureDeathEffectOrigin(deathOrigin, deathRotation);
	if (!hasDeathPartWorldTransforms_)
	{
		CaptureDeathPartWorldTransforms();
	}
	deathPieces_.clear();
	deathBreakActive_ = true;
	deathBreakInitialized_ = true;
	++deathEffectInitializeCount_;
	deathSimDuration_ = s_deathPieceLifetime_;
	deathTimer_ = deathSimDuration_;

	// 死亡部位の最終描画座標が原点へ戻らないよう、deathOriginから各部位のWorldTransformを直接構築する。
	body_.transform.parent_ = nullptr;
	body_.transform.translate_ = deathEffectOrigin_;
	body_.transform.rotate_ = deathInitialBodyRotation_;
	deathInitialBodyPosition_ = body_.transform.translate_;
	body_.transform.Update();
	if (body_.object)
	{
		body_.object->SetTranslate(body_.transform.translate_);
		body_.object->SetRotate(body_.transform.rotate_);
		body_.object->Update();
	}

	for (size_t i = 0; i < parts_.size(); ++i)
	{
		auto& part = parts_[i];
		part.transform.parent_ = nullptr;
		const Vector3 localOffset = (i < deathInitialPartLocalOffsets_.size()) ? deathInitialPartLocalOffsets_[i] : part.transform.translate_;
		part.transform.translate_ = BuildDeathPartWorldPosition(localOffset);
		part.transform.rotate_ = (i < deathInitialPartRotations_.size()) ? deathInitialPartRotations_[i] : deathEffectRotation_;
		part.transform.Update();
		if (part.object)
		{
			part.object->SetTranslate(part.transform.translate_);
			part.object->SetRotate(part.transform.rotate_);
			part.object->Update();
		}
	}

	// 色を戻してからフェードを制御する
	SetVisualColorAll(baseColor_);

	const Vector3 center = deathEffectOrigin_;
	deathGroundY_ = FindGroundY(center) + 0.05f;

	auto makeCollapsePiece = [this, &center](BodyPart* part, float outwardScale, float upwardScale, float angularScale) {
		DeathPiece p{};
		p.part = part;
		if (!part)
		{
			return p;
		}

		Vector3 fromCenter = part->transform.translate_ - center;
		Vector3 outward = NormalizeSafe(fromCenter, { 0.0f, 0.0f, 1.0f });
		Vector3 horizontalJitter = RandomUnit();
		horizontalJitter.y = 0.0f;
		horizontalJitter = NormalizeSafe(horizontalJitter, { outward.x, 0.0f, outward.z });
		const Vector3 dir = NormalizeSafe(outward * 0.9f + horizontalJitter * 0.1f, outward);
		const float enabledScale = s_deathExplosionEnabled_ ? 1.0f : 0.15f;
		const float speed = s_deathExplodePower_ * outwardScale * enabledScale * RandRange(0.85f, 1.15f);
		const float lift = s_deathUpwardPower_ * upwardScale * enabledScale * RandRange(0.85f, 1.15f);
		// 死亡位置を基準にしたまま、各部位へ外向きの初速度を与えて爆散感を出す。
		p.velocity = ClampVectorLength(dir * speed + Vector3{ 0.0f, lift, 0.0f }, s_deathMaxSpeed_);
		p.angularVel = ClampVectorLength(Vector3{
			RandRange(-1.1f, 1.1f) * angularScale,
			RandRange(-1.4f, 1.4f) * angularScale,
			RandRange(-1.1f, 1.1f) * angularScale }, s_deathMaxAngularSpeed_);
		return p;
	};

	deathPieces_.push_back(makeCollapsePiece(&body_, 0.20f, 0.25f, 0.35f));
	for (size_t i = 0; i < parts_.size(); ++i)
	{
		if (i == partIndices_.head)
		{
			deathPieces_.push_back(makeCollapsePiece(&parts_[i], 0.85f, 1.35f, 0.70f));
		}
		else if (i == partIndices_.leftArm || i == partIndices_.rightArm)
		{
			deathPieces_.push_back(makeCollapsePiece(&parts_[i], 1.15f, 0.85f, 1.00f));
		}
		else
		{
			deathPieces_.push_back(makeCollapsePiece(&parts_[i], 0.85f, 0.55f, 0.80f));
		}
	}
}

/// -------------------------------------------------------------
/// death: 更新
/// -------------------------------------------------------------
void EnemyBase::UpdateBreakApartDeath(float dt)
{
	// 死亡演出が暴れないように、deltaTime・破片速度・回転速度を制限する。
	dt = std::clamp(dt, 0.0f, kMaxUpdateDeltaTime);
	// 念のため：死亡した瞬間に OnKilled が呼ばれない派生があっても演出を走らせる
	if (!deathBreakActive_)
	{
		StartBreakApartDeath(GetCenterPosition(), orientation_);
	}

	// タイマー進行
	deathTimer_ -= dt;
	if (deathTimer_ < 0.0f) deathTimer_ = 0.0f;

	// フェード
	float alpha = 1.0f;
	if (deathTimer_ <= deathFadeDuration_)
	{
		alpha = Clamp01(deathTimer_ / deathFadeDuration_);
	}

	Vector4 c = baseColor_;
	c.w *= alpha;
	SetVisualColorAll(c);

	// 破片物理
	for (auto& p : deathPieces_)
	{
		if (!p.part || !p.part->object) continue;

		// 重力
		p.velocity.y -= gravity_ * dt;
		p.velocity = ClampVectorLength(p.velocity, s_deathMaxSpeed_);
		p.angularVel = ClampVectorLength(p.angularVel, s_deathMaxAngularSpeed_);

		// 減衰
		const float linD = std::max(0.0f, 1.0f - deathLinearDamping_ * dt);
		const float angD = std::max(0.0f, 1.0f - deathAngularDamping_ * dt);
		p.velocity = ClampVectorLength(p.velocity * linD, s_deathMaxSpeed_);
		p.angularVel = ClampVectorLength(p.angularVel * angD, s_deathMaxAngularSpeed_);

		// 位置・回転
		Vector3 frameMove = p.velocity * dt;
		frameMove = ClampVectorLength(frameMove, deathMaxMovePerFrame_);
		p.part->transform.translate_ = p.part->transform.translate_ + frameMove;
		p.part->transform.rotate_ = p.part->transform.rotate_ + p.angularVel * dt;

		// 簡易床
		if (p.part->transform.translate_.y < deathGroundY_)
		{
			p.part->transform.translate_.y = deathGroundY_;

			if (p.velocity.y < 0.0f)
			{
				p.velocity.y = -p.velocity.y * deathBounce_;
				p.velocity.x *= deathFriction_;
				p.velocity.z *= deathFriction_;
			}
		}

		p.part->transform.parent_ = nullptr;
		p.part->transform.Update();

		p.part->object->SetTranslate(p.part->transform.translate_);
		p.part->object->SetRotate(p.part->transform.rotate_);
		p.part->object->Update();
	}

	// 終了
	if (deathTimer_ <= 0.0f)
	{
		removable_ = true;
		deathBreakActive_ = false;
	}
}

/// -------------------------------------------------------------
/// 弾ヒット（デフォルト）
/// - bulletCollider の位置から「進行方向っぽいベクトル」を作って渡す
/// -------------------------------------------------------------
void EnemyBase::OnBulletHit(Collider* bulletCollider)
{
	Vector3 hitDir{ 0, 0, 0 };
	float hitPower = 1.0f;
	int damage = 10;

	// 被弾位置
	Vector3 hitPos = GetCenterPosition();
	hitPos.y += 1.0f; // 弾位置が取れない時の保険

	if (bulletCollider)
	{
		hitPos = bulletCollider->GetCenterPosition();

		const Segment bulletSegment = bulletCollider->GetSegment();
		if (Length(bulletSegment.diff) > 1e-4f)
		{
			hitDir = bulletSegment.diff;
		}
		else
		{
			hitDir = GetCenterPosition() - bulletCollider->GetCenterPosition();
		}

		if (auto* bullet = bulletCollider->GetOwner<Bullet>())
		{
			damage = std::max(1, bullet->GetDamage());
			const Vector3 bulletDir = NormalizeSafe(bullet->GetMoveVelocity(), NormalizeSafe(hitDir, { 0.0f, 0.0f, 1.0f }));
			const Vector3 attackerDir = NormalizeSafe(GetCenterPosition() - bullet->GetShooterPosition(), bulletDir);
			const EDeathKnockbackType kbType = bullet->GetDeathKnockbackType();
			// 最後に受けた武器情報から死亡時の吹っ飛び方向と強さを決める
			switch (kbType)
			{
			case EDeathKnockbackType::Sniper:
			hitDir = bulletDir;
			hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
			lastHitUpPower_ = std::max(0.3f, bullet->GetDeathKnockbackUpPower());
			break;
			case EDeathKnockbackType::Heavy:
			hitDir = NormalizeSafe(bulletDir + Vector3{ 0.0f, 0.35f, 0.0f }, attackerDir);
			hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
			lastHitUpPower_ = bullet->GetDeathKnockbackUpPower();
			break;
			case EDeathKnockbackType::Explosion:
			{
				Vector3 outDir = NormalizeSafe(GetCenterPosition() - bulletCollider->GetCenterPosition(), attackerDir);
				hitDir = NormalizeSafe(outDir + Vector3{ 0.0f, 0.55f, 0.0f }, attackerDir);
				hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
				lastHitUpPower_ = std::max(2.0f, bullet->GetDeathKnockbackUpPower());
				break;
			}
			case EDeathKnockbackType::Light:
			case EDeathKnockbackType::Default:
			default:
			hitDir = attackerDir;
			hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
			lastHitUpPower_ = bullet->GetDeathKnockbackUpPower();
			break;
			}
		}
	}

	// 被弾パーティクル
	SpawnHitEffectAt(hitPos);

	TakeDamage(damage, hitDir, hitPower);
}

/// -------------------------------------------------------------
/// 衝突開始
/// -------------------------------------------------------------
void EnemyBase::OnCollisionEnter(Collider* other)
{
	if (!other || isDead_) return;

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		if (auto* bullet = other->GetOwner<Bullet>(); bullet && bullet->UsesPhysicsTrigger())
		{
			// PhysicsWorld移行済みBulletの二重処理を防ぐため、Enemy側の旧CollisionManager被弾処理をスキップする。
			return;
		}
		OnBulletHit(other);
	}
}
