#define NOMINMAX
#include "MidRangeBombProjectile.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Character/Player/Player.h"
#include "Wireframe.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

IPlayerRuntime* MidRangeBombProjectile::s_targetPlayerRuntime_ = nullptr;
bool MidRangeBombProjectile::s_debugCubeVisible_ = true;
float MidRangeBombProjectile::s_debugCubeSize_ = 0.4f;

void MidRangeBombProjectile::Initialize()
{
    position_ = {};
    velocity_ = {};
    explosionPosition_ = {};
    lifeTimer_ = 0.0f;
    explosionDrawTimer_ = 0.0f;
    exploded_ = false;
    alive_ = false;
    directDamageApplied_ = false;
    explosionDamageApplied_ = false;

    debugCube_ = std::make_unique<Object3D>();
    debugCube_->Initialize("Sample/cube.gltf");
    debugCube_->SetColor({ 0.15f, 0.02f, 0.22f, 1.0f });
    UpdateDebugCube();
}

void MidRangeBombProjectile::Launch(const Vector3& start, const Vector3& target, const BombProjectileSettings& settings)
{
    settings_ = settings;
    position_ = start;
    explosionPosition_ = start;
    lifeTimer_ = 0.0f;
    explosionDrawTimer_ = 0.0f;
    exploded_ = false;
    alive_ = true;
    directDamageApplied_ = false;
    explosionDamageApplied_ = false;

    Vector3 directionXZ = target - start;
    directionXZ.y = 0.0f;
    float lengthXZ = std::sqrt(directionXZ.x * directionXZ.x + directionXZ.z * directionXZ.z);
    if (lengthXZ > 0.0001f)
    {
        directionXZ.x /= lengthXZ;
        directionXZ.z /= lengthXZ;
    }
    else
    {
        directionXZ = { 0.0f, 0.0f, 1.0f };
    }

    const float maxInitialSpeed = std::max(0.0f, settings_.maxInitialSpeed);
    velocity_ = directionXZ * std::clamp(settings_.initialSpeed, 0.0f, maxInitialSpeed);
    velocity_.y = std::clamp(settings_.upwardVelocity, 0.0f, 12.0f);
    UpdateDebugCube();
}

void MidRangeBombProjectile::Update(float deltaTime)
{
    constexpr float kFloorY = 0.0f;
    constexpr float kMaxDeltaTime = 1.0f / 30.0f;
    deltaTime = std::clamp(deltaTime, 0.0f, kMaxDeltaTime);

    if (alive_)
    {
        lifeTimer_ += deltaTime;
        velocity_.y -= std::clamp(settings_.gravity, 0.0f, 30.0f) * deltaTime;
        position_ += velocity_ * deltaTime;
        UpdateDebugCube();

        bool shouldExplode = false;
        if (lifeTimer_ >= settings_.lifeTime) shouldExplode = true;
        if (position_.y <= kFloorY)
        {
            position_.y = kFloorY + s_debugCubeSize_ * 0.5f;
            UpdateDebugCube();
            shouldExplode = true;
        }
        if (shouldExplode) Explode();
    }
    else if (exploded_)
    {
        explosionDrawTimer_ -= deltaTime;
        if (explosionDrawTimer_ < 0.0f) explosionDrawTimer_ = 0.0f;
    }

    if (s_targetPlayerRuntime_) TryApplyPlayerDamage(*s_targetPlayerRuntime_); // P13では旧MidRangeEnemyのPlayer所有者判定に依存せずRuntimeへ直接Damageを適用する。
}

void MidRangeBombProjectile::Draw() const
{
    if (alive_ && s_debugCubeVisible_ && debugCube_) debugCube_->Draw();
    if (exploded_ && explosionDrawTimer_ > 0.0f) Wireframe::GetInstance()->DrawSphere(explosionPosition_, settings_.explosionRadius, { 1.0f, 0.1f, 0.1f, 0.85f });
}

void MidRangeBombProjectile::Explode()
{
    explosionPosition_ = position_;
    if (explosionPosition_.y < 0.2f) explosionPosition_.y = 0.2f;
    exploded_ = true;
    alive_ = false;
    explosionDrawTimer_ = 0.2f;
}

void MidRangeBombProjectile::SetDebugCubeSize(float size)
{
    s_debugCubeSize_ = std::clamp(size, 0.1f, 2.0f);
}

void MidRangeBombProjectile::UpdateDebugCube()
{
    if (!debugCube_) return;
    debugCube_->SetTranslate(position_);
    debugCube_->SetRotate({ lifeTimer_ * 2.0f, lifeTimer_ * 3.5f, lifeTimer_ * 1.5f });
    debugCube_->SetScale({ s_debugCubeSize_, s_debugCubeSize_, s_debugCubeSize_ });
    debugCube_->Update();
}

MidRangeBombProjectile::PlayerHitResult MidRangeBombProjectile::TryApplyPlayerDamage(IPlayerRuntime& player)
{
    PlayerHitResult result = PlayerHitResult::None;
    const Vector3 playerPosition = player.GetWorldPosition();

    if (alive_ && !directDamageApplied_)
    {
        const float directDistance = Vector3::Length(playerPosition - position_);
        if (directDistance <= settings_.hitRadius)
        {
            player.ApplyDamage(static_cast<float>(settings_.directHitDamage), &position_);
            directDamageApplied_ = true;
            result = PlayerHitResult::Direct;
            Explode();
        }
    }

    if (exploded_ && !explosionDamageApplied_)
    {
        const float explosionDistance = Vector3::Length(playerPosition - explosionPosition_);
        if (explosionDistance <= settings_.explosionRadius)
        {
            if (result != PlayerHitResult::Direct || settings_.directHitAlsoExplosionDamage)
            {
                player.ApplyDamage(static_cast<float>(settings_.explosionDamage), &explosionPosition_);
                explosionDamageApplied_ = true;
                result = (result == PlayerHitResult::Direct) ? PlayerHitResult::DirectAndExplosion : PlayerHitResult::Explosion;
            }
            else
            {
                explosionDamageApplied_ = true;
            }
        }
    }
    return result;
}

MidRangeBombProjectile::PlayerHitResult MidRangeBombProjectile::TryApplyPlayerDamage(Player& player)
{
    return TryApplyPlayerDamage(static_cast<IPlayerRuntime&>(player)); // 旧Playerソース比較用の互換入口だけ残す。
}

bool MidRangeBombProjectile::IsAlive() const
{
    return alive_ || (exploded_ && explosionDrawTimer_ > 0.0f);
}
