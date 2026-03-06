#include "EnemyBase.h"
#include <cmath>
#include <vector>
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

const std::vector<Ken4lowEngine::AABB>* EnemyBase::g_worldAABBs_ = nullptr;

/// -------------------------------------------------------------
/// 人型見た目の初期化
/// -------------------------------------------------------------
void EnemyBase::InitializeHumanoidVisual()
{
	body_ = {};
	parts_.clear();

	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Character/body.gltf");
	body_.transform.translate_ = { 0.0f, 0.0f, 0.0f };
	body_.transform.rotate_ = orientation_;

	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"Character/head.gltf",      { 0.0f,  0.75f, 0.0f }},
		{"Character/left_arm.gltf",  {-0.75f, 0.75f, 0.0f }},
		{"Character/right_arm.gltf", { 0.75f, 0.75f, 0.0f }},
		{"Character/left_leg.gltf",  {-0.25f,-0.75f, 0.0f }},
		{"Character/right_leg.gltf", { 0.25f,-0.75f, 0.0f }},
	};

	for (const auto& [modelPath, localPos] : partData)
	{
		BodyPart part{};
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.transform.translate_ = localPos;
		part.transform.parent_ = &body_.transform;
		parts_.push_back(std::move(part));
	}
}

/// -------------------------------------------------------------
/// 見た目階層更新
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
/// 全見た目を遠方へ移動
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
void EnemyBase::Initialize(const Vector3& startPos, const std::string& modelPath)
{
	(void)modelPath; // いまは人型固定。必要なら後でスキン/種別に転用

	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
	SetOwner(this);

	SetOBBHalfSize(obbHalf_);
	SetSegment(Segment{});

	InitializeHumanoidVisual();
	SetColor(baseColor_);
	hitFlashTimer_ = 0.0f;

	SetCenterPosition(startPos);

	isDead_ = false;
	removable_ = false;
	deadFrames_ = 0;
	hp_ = maxHp_;

	useGravity_ = true;
	worldCol_.half = obbHalf_;
	worldCol_.centerOffset = { 0,0,0 };
	worldColOverride_ = true;
}

/// -------------------------------------------------------------
/// 中心座標
/// -------------------------------------------------------------
void EnemyBase::SetCenterPosition(const Vector3& pos)
{
	Collider::SetCenterPosition(pos);
	body_.transform.translate_ = pos;
	UpdateVisualHierarchy();
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
	UpdateVisualHierarchy();
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
void EnemyBase::Update(float dt)
{
	if (removable_) return;

	if (isDead_)
	{
		++deadFrames_;
		if (deadFrames_ >= 2) removable_ = true;
		return;
	}

	grounded_ = false;

	if (useGravity_) velocity_.y -= gravity_ * dt;

	const Vector3 oldPos = GetCenterPosition();
	Vector3 newPos = oldPos + velocity_ * dt;

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
	UpdateHitFlash(dt);
}

/// -------------------------------------------------------------
/// 描画
/// -------------------------------------------------------------
void EnemyBase::Draw()
{
	if (isDead_ || removable_) return;

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
	if (isDead_ || removable_) return;

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
/// ダメージ
/// -------------------------------------------------------------
void EnemyBase::TakeDamage(int amount)
{
	if (isDead_) return;

	StartHitFlash();
	hp_ -= amount;
	if (hp_ <= 0)
	{
		hp_ = 0;
		isDead_ = true;
		deadFrames_ = 0;
		OnKilled();
		DisableColliderAndMoveFar();
	}
}

void EnemyBase::SetGlobalStageWorldAABBs(const std::vector<K4E::AABB>* aabbs)
{
	g_worldAABBs_ = aabbs;
}

/// -------------------------------------------------------------
/// 死亡
/// -------------------------------------------------------------
void EnemyBase::OnKilled()
{
	// 派生で死亡演出
}

/// -------------------------------------------------------------
/// コライダー無効化
/// -------------------------------------------------------------
void EnemyBase::DisableColliderAndMoveFar()
{
	SetOBBHalfSize({ 0.0f, 0.0f, 0.0f });

	Segment s{};
	s.origin = { 0,0,0 };
	s.diff = { 0,0,0 };
	SetSegment(s);

	const Vector3 far_ = { 1e9f, 1e9f, 1e9f };
	Collider::SetCenterPosition(far_);
	MoveVisualFar(far_);
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
/// 弾ヒット
/// -------------------------------------------------------------
void EnemyBase::OnBulletHit(Collider* bulletCollider)
{
	(void)bulletCollider;
	TakeDamage(10);
}

/// -------------------------------------------------------------
/// 衝突開始
/// -------------------------------------------------------------
void EnemyBase::OnCollisionEnter(Collider* other)
{
	if (!other || isDead_) return;

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		OnBulletHit(other);
	}
}