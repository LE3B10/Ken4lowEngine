#define NOMINMAX
#include "EnemyBase.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include "EnemyParticleEffectSystem.h"
#include <Bullet.h>

#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

const std::vector<Ken4lowEngine::AABB>* EnemyBase::g_worldAABBs_ = nullptr;

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
		thread_local std::mt19937 engine{ std::random_device{}() };
		static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		return dist(engine);
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
	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
	SetOwner(this);

	SetOBBHalfSize(obbHalf_);
	SetSegment(Segment{});

	InitializeHumanoidVisual();
	SetColor(baseColor_);
	hitFlashTimer_ = 0.0f;

	SetCenterPosition({ 0.0f, 0.0f, 0.0f });

	isDead_ = false;
	removable_ = false;
	spawnProtectionActive_ = false;

	deathBreakActive_ = false;
	deathTimer_ = 0.0f;
	deathPieces_.clear();
	lastHitDir_ = { 0, 0, 0 };
	lastHitPower_ = 1.0f;

	hp_ = maxHp_;

	useGravity_ = true;
	worldCol_.half = obbHalf_;
	worldCol_.centerOffset = { 0, 0, 0 };
	worldColOverride_ = true;
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
	SetCenterPosition(p);
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
		newPos = res.fixedCenter + s.centerOffset;
	}

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
		body_.object->Draw();
	}

	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->Draw();
		}
	}
}

/// -------------------------------------------------------------
/// ImGui描画
/// -------------------------------------------------------------
void EnemyBase::DrawImGui()
{
	if (body_.object) body_.object->DrawImGui();

	for (auto& part : parts_)
	{
		if (part.object) part.object->DrawImGui();
	}
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
	if (isDead_ || spawnProtectionActive_) return;

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

void EnemyBase::SpawnHitEffectAt(const K4E::Vector3& worldPos)
{
	if (!particleEffectSystem_)
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
	// 死亡パーティクル
	if (particleEffectSystem_)
	{
		particleEffectSystem_->SpawnDeathEffect(GetCenterPosition());
	}

	StartBreakApartDeath();
}

/// -------------------------------------------------------------
/// コライダー無効化（見た目は残す）
/// -------------------------------------------------------------
void EnemyBase::DisableColliderOnly()
{
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

/// -------------------------------------------------------------
/// death: 開始
/// -------------------------------------------------------------
void EnemyBase::StartBreakApartDeath()
{
	deathPieces_.clear();
	deathBreakActive_ = true;
	deathTimer_ = deathSimDuration_;

	DetachAllPartsToWorldSpace();

	// 色を戻してからフェードを制御する
	SetVisualColorAll(baseColor_);

	const Vector3 center = GetCenterPosition();
	const Vector3 hitDir = NormalizeSafe(lastHitDir_, { 0, 0, 1 });
	const float power = (lastHitPower_ > 0.0f) ? lastHitPower_ : 1.0f;

	// 体（重め）
	{
		DeathPiece p{};
		p.part = &body_;
		p.hitBias = 0.75f;
		const Vector3 fromCenter = NormalizeSafe(body_.transform.translate_ - center);
		const Vector3 dir = NormalizeSafe(fromCenter + RandomUnit() * 0.35f + hitDir * p.hitBias);
		const float speed = RandRange(2.0f, 4.5f) * power;
		p.velocity = dir * speed + Vector3{ 0.0f, RandRange(1.0f, 3.0f) * power, 0.0f };
		p.angularVel = Vector3{ RandRange(-5.0f, 5.0f), RandRange(-7.0f, 7.0f), RandRange(-5.0f, 5.0f) };
		deathPieces_.push_back(p);
	}

	// 手足/頭
	for (size_t i = 0; i < parts_.size(); ++i)
	{
		DeathPiece p{};
		p.part = &parts_[i];

		// 部位ごとに少し差
		p.hitBias = (i == partIndices_.head) ? 0.9f : 0.55f;

		const Vector3 fromCenter = NormalizeSafe(parts_[i].transform.translate_ - center);
		const Vector3 dir = NormalizeSafe(fromCenter + RandomUnit() * 0.55f + hitDir * p.hitBias);

		const float speed = RandRange(3.0f, 6.5f) * power;
		const float up = (i == partIndices_.head) ? RandRange(2.0f, 4.0f) : RandRange(1.2f, 3.2f);

		p.velocity = dir * speed + Vector3{ 0.0f, up * power, 0.0f };
		p.angularVel = Vector3{ RandRange(-10.0f, 10.0f), RandRange(-14.0f, 14.0f), RandRange(-10.0f, 10.0f) };
		deathPieces_.push_back(p);
	}
}

/// -------------------------------------------------------------
/// death: 更新
/// -------------------------------------------------------------
void EnemyBase::UpdateBreakApartDeath(float dt)
{
	// 念のため：死亡した瞬間に OnKilled が呼ばれない派生があっても演出を走らせる
	if (!deathBreakActive_)
	{
		StartBreakApartDeath();
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

		// 減衰
		const float linD = std::max(0.0f, 1.0f - deathLinearDamping_ * dt);
		const float angD = std::max(0.0f, 1.0f - deathAngularDamping_ * dt);
		p.velocity = p.velocity * linD;
		p.angularVel = p.angularVel * angD;

		// 位置・回転
		p.part->transform.translate_ = p.part->transform.translate_ + p.velocity * dt;
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
		}
	}

	// 被弾パーティクル
	if (particleEffectSystem_)
	{
		particleEffectSystem_->SpawnHitEffect(hitPos);
	}

	TakeDamage(damage, hitDir, hitPower);
}

/// -------------------------------------------------------------
/// 衝突開始
/// -------------------------------------------------------------
void EnemyBase::OnCollisionEnter(Collider* other)
{
	if (!other || isDead_ || spawnProtectionActive_) return;

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		OnBulletHit(other);
	}
}
