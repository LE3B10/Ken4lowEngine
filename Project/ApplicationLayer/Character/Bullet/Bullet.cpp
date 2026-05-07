#define NOMINMAX
#include "Bullet.h"
#include "CollisionTypeIdDef.h"
#include "CollisionManager.h"
#include "EnemyBase.h"
#include "GpuParticleManager.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	constexpr uint32_t kRocketDebrisMeshId = 1000;
	constexpr const char* kRocketDebrisEmitterName = "RocketDebrisMesh";
	constexpr const char* kRocketDebrisMeshModelPath = "cube.gltf";

	float LengthSq(const K4E::Vector3& v)
	{
		return v.x * v.x + v.y * v.y + v.z * v.z;
	}

	float Length(const K4E::Vector3& v)
	{
		return std::sqrt(LengthSq(v));
	}

	K4E::Vector3 NormalizeSafe(const K4E::Vector3& v, const K4E::Vector3& fallback = { 0.0f, 1.0f, 0.0f })
	{
		const float len = Length(v);
		if (len <= 1.0e-5f) return fallback;
		return v * (1.0f / len);
	}

	K4E::GpuParticleEmitter* PrepareRocketDebrisMeshEmitter(const K4E::Vector3& position)
	{
		auto* particle = K4E::GpuParticleManager::GetInstance();
		if (!particle)
		{
			return nullptr;
		}

		static bool meshAssetLoadRequested = false;
		if (!meshAssetLoadRequested)
		{
			// AssimpLoader は内部で Resources/Models/ を付与するため、
			// まずは既存の cube.gltf を MeshParticle の確認用に使う。
			// 専用破片モデルを作ったら Sources/Effects/rocket_debris.gltf などへ差し替える。
			particle->LoadMeshAssetsFromAssimp(kRocketDebrisMeshId, kRocketDebrisMeshModelPath, true);
			meshAssetLoadRequested = true;
		}

		if (auto* emitter = particle->GetEmitter(kRocketDebrisEmitterName))
		{
			emitter->SetPosition(position);
			return emitter;
		}

		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Mesh;
		info.spriteType = K4E::GpuParticleType::Debris;
		info.drawType = static_cast<uint32_t>(K4E::GpuParticleType::Debris);
		info.billboardFlags = K4E::BillboardMode::None;
		info.textureFilePath = "Mesh:" + std::to_string(kRocketDebrisMeshId);
		info.radius = 0.8f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		K4E::GpuParticleEmitter* emitter = particle->CreateEmitter(kRocketDebrisEmitterName, info);
		if (!emitter)
		{
			emitter = particle->GetEmitter(kRocketDebrisEmitterName);
		}

		if (emitter)
		{
			emitter->SetPosition(position);
		}

		return emitter;
	}
}

void Bullet::Initialize(const K4E::Vector3& startPos,
	const K4E::Vector3& velocity,
	int damage,
	float lifeTimeSec,
	const K4E::Vector3& shooterPosition,
	uint32_t shooterColliderId,
	uint32_t typeId
)
{
	damage_ = damage;
	moveVelocity_ = velocity;
	lifeTimeSec_ = lifeTimeSec;
	lifeTimer_ = 0.0f;
	shooterPosition_ = shooterPosition;
	shooterColliderId_ = shooterColliderId;

	Collider::SetTypeID(typeId);
	Collider::SetOwner(this);

	// デバッグ表示したいなら
	model_ = std::make_unique<K4E::Object3D>();
	model_->Initialize("Test/cube.gltf");

	// 弾種で色を変えたいなら（任意）
	// if (typeId == (uint32_t)CollisionTypeIdDef::kEnemyBullet) debugColor_ = {1,0,0,1};

	// セグメント判定が主なので OBB は小さめでOK
	Collider::SetOBBHalfSize(scale_);

	prevPos_ = startPos;
	Collider::SetCenterPosition(startPos);
	if (model_) {
		model_->SetScale(scale_);
		model_->SetTranslate(startPos);
		model_->SetColor(debugColor_);
		model_->Update();
	}

	// 初期セグメント長さ0
	K4E::Segment seg{};
	seg.origin = startPos;
	seg.diff = { 0.0f, 0.0f, 0.0f };
	Collider::SetSegment(seg);

	isDead_ = false;
	removable_ = false;
	deadFrames_ = 0;
	splashTriggered_ = false;
	contactRecord_.Clear();
}

void Bullet::ConfigureSplashDamage(float radius, int damage, bool canDamageSelf)
{
	splashRadius_ = std::max(0.0f, radius);
	splashDamage_ = std::max(0, damage);
	splashCanDamageSelf_ = canDamageSelf;

	if (HasSplashDamage())
	{
		// デバッグ視認性を少し上げる
		debugColor_ = { 1.0f, 0.35f, 0.05f, 1.0f };
		if (model_) model_->SetColor(debugColor_);
	}
}

void Bullet::KillAndMoveFar()
{
	// Exit 解決用に 1フレーム残す
	isDead_ = true;
	deadFrames_ = 0;

	const K4E::Vector3 far_ = { 1e9f, 1e9f, 1e9f };
	Collider::SetCenterPosition(far_);
	if (model_) model_->SetTranslate(far_);

	K4E::Segment s{};
	s.origin = far_;
	s.diff = { 0.0f, 0.0f, 0.0f };
	Collider::SetSegment(s);

	if (model_) model_->Update();
}

void Bullet::ApplySplashDamageToType(uint32_t targetType, const K4E::Vector3& center)
{
	if (!collisionManager_ || !HasSplashDamage()) return;

	const auto& targets = collisionManager_->GetCollidersByType(targetType);
	const float radiusSq = splashRadius_ * splashRadius_;

	for (K4E::Collider* col : targets)
	{
		if (!col) continue;
		if (!splashCanDamageSelf_ && shooterColliderId_ != 0u && col->GetUniqueID() == shooterColliderId_) continue;

		K4E::Vector3 toTarget = col->GetCenterPosition() - center;
		const float distSq = LengthSq(toTarget);
		if (distSq > radiusSq) continue;

		const float dist = std::sqrt(std::max(0.0f, distSq));
		const float t = (splashRadius_ > 0.0f) ? std::clamp(dist / splashRadius_, 0.0f, 1.0f) : 1.0f;
		const float damageRate = 1.0f - t;
		const int finalDamage = std::max(1, static_cast<int>(std::round(static_cast<float>(splashDamage_) * damageRate)));

		if (auto* enemy = col->GetOwner<EnemyBase>())
		{
			const K4E::Vector3 hitDir = NormalizeSafe(toTarget, NormalizeSafe(moveVelocity_, { 0.0f, 0.0f, 1.0f }));
			enemy->TakeDamage(finalDamage, hitDir, 1.8f);
			enemy->SpawnHitEffectAt(col->GetCenterPosition());
		}
	}
}

void Bullet::TriggerSplashDamageAt(const K4E::Vector3& center)
{
	if (!HasSplashDamage()) return;
	if (splashTriggered_) return;
	splashTriggered_ = true;

	ApplySplashDamageToType(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy), center);
	ApplySplashDamageToType(static_cast<uint32_t>(CollisionTypeIdDef::kBoss), center);

	K4E::GpuParticleManager::GetInstance()->EmitBurst(
		"HeavySplashImpact",
		K4E::GpuParticleType::DeathBurstCore,
		center,
		80);

	K4E::GpuParticleManager::GetInstance()->EmitBurst(
		"HeavySplashSmoke",
		K4E::GpuParticleType::Smoke,
		center,
		60);

	// ロケットランチャー用：着弾時に小さなメッシュ破片を飛ばす
	if (auto* debrisEmitter = PrepareRocketDebrisMeshEmitter(center))
	{
		debrisEmitter->RequestEmit(24);
	}
}

void Bullet::Update(float dt)
{
	if (removable_) return;

	// 死亡済み：Exit 解決のため 1フレーム残す
	if (isDead_)
	{
		++deadFrames_;
		if (deadFrames_ >= 2) removable_ = true;
		return;
	}

	lifeTimer_ += dt;
	if (lifeTimer_ >= lifeTimeSec_)
	{
		if (HasSplashDamage())
		{
			TriggerSplashDamageAt(GetCenterPosition());
		}
		KillAndMoveFar();
		return;
	}

	const K4E::Vector3 current = GetCenterPosition();
	const K4E::Vector3 delta = moveVelocity_ * dt;
	const K4E::Vector3 next = current + delta;

	// このフレームの移動分を Segment にする（すり抜け防止）
	K4E::Segment seg{};
	seg.origin = current;
	seg.diff = delta;
	SetSegment(seg);

	prevPos_ = current;
	Collider::SetCenterPosition(next);
	if (model_) model_->SetTranslate(next);

	// ざっくり範囲外で消す（必要なら後で world bounds に置換）
	if (next.x > 1000.0f || next.x < -1000.0f || next.z > 1000.0f || next.z < -1000.0f)
	{
		KillAndMoveFar();
		return;
	}

	if (model_) model_->Update();
}

void Bullet::Draw()
{
	if (removable_) return;
	if (model_) model_->Draw(); // デバッグ用に見たいならON
}

void Bullet::DrawImGui()
{
	if (model_) model_->DrawImGui();
}

void Bullet::OnCollisionEnter(K4E::Collider* other)
{
	if (!other) return;
	if (isDead_ || removable_) return;
	if (shooterColliderId_ != 0u && other->GetUniqueID() == shooterColliderId_) return; // 自分を撃ったコライダーとの接触は無視

	const uint32_t selfType = GetTypeID();
	const uint32_t otherType = other->GetTypeID();

	// どこに当たったら消すかは「弾種」で決める
	const uint32_t kPlayer = static_cast<uint32_t>(CollisionTypeIdDef::kPlayer);
	const uint32_t kEnemy = static_cast<uint32_t>(CollisionTypeIdDef::kEnemy);
	const uint32_t kBoss = static_cast<uint32_t>(CollisionTypeIdDef::kBoss);
	const uint32_t kWorld = static_cast<uint32_t>(CollisionTypeIdDef::kWorld);

	bool shouldHit = false;
	if (selfType == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		// プレイヤー弾：敵/ボス/ワールド
		shouldHit = (otherType == kEnemy || otherType == kBoss || otherType == kWorld);
	}
	else if (selfType == static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet) ||
		selfType == static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet))
	{
		// 敵弾/ボス弾：プレイヤー/ワールド
		shouldHit = (otherType == kPlayer || otherType == kWorld);
	}

	if (!shouldHit) return;

	// 多段ヒット防止（基本は当たったら即死なので保険）
	const uint32_t otherId = other->GetUniqueID();
	if (contactRecord_.Check(otherId)) return;
	contactRecord_.Add(otherId);

	if (HasSplashDamage())
	{
		TriggerSplashDamageAt(other->GetCenterPosition());
	}

	KillAndMoveFar();
}