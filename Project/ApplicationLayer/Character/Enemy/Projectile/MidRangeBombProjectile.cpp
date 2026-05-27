#include "MidRangeBombProjectile.h"

#include "Wireframe.h"

#include <cmath>

using namespace Ken4lowEngine;

void MidRangeBombProjectile::Initialize()
{
    // 追加: 爆弾ランタイム状態の初期化。
    position_ = {};
    velocity_ = {};
    explosionPosition_ = {};
    lifeTimer_ = 0.0f;
    explosionDrawTimer_ = 0.0f;
    exploded_ = false;
    alive_ = false;
}

void MidRangeBombProjectile::Launch(
    const Vector3& start,
    const Vector3& target,
    const BombProjectileSettings& settings
)
{
    // 追加: 爆弾設定と放物線初速の設定。
    settings_ = settings;
    position_ = start;
    explosionPosition_ = start;
    lifeTimer_ = 0.0f;
    explosionDrawTimer_ = 0.0f;
    exploded_ = false;
    alive_ = true;

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

    velocity_ = directionXZ * settings_.initialSpeed;
    velocity_.y = settings_.upwardVelocity;
}

void MidRangeBombProjectile::Update(float deltaTime)
{
    constexpr float kFloorY = 0.0f;

    if (alive_)
    {
        lifeTimer_ += deltaTime;
        velocity_.y -= settings_.gravity * deltaTime;
        position_ += velocity_ * deltaTime;

        bool shouldExplode = false;
        if (lifeTimer_ >= settings_.lifeTime)
        {
            shouldExplode = true;
        }
        if (position_.y <= kFloorY)
        {
            shouldExplode = true;
        }

        if (shouldExplode)
        {
            Explode();
        }
    }
    else if (exploded_)
    {
        explosionDrawTimer_ -= deltaTime;
        if (explosionDrawTimer_ < 0.0f)
        {
            explosionDrawTimer_ = 0.0f;
        }
    }
}

void MidRangeBombProjectile::Draw() const
{
    if (alive_)
    {
        Wireframe::GetInstance()->DrawSphere(position_, settings_.hitRadius, { 1.0f, 0.4f, 0.1f, 1.0f });
    }

    if (exploded_ && explosionDrawTimer_ > 0.0f)
    {
        Wireframe::GetInstance()->DrawSphere(explosionPosition_, settings_.explosionRadius, { 1.0f, 0.1f, 0.1f, 0.85f });
    }
}

void MidRangeBombProjectile::Explode()
{
    // 追加: 爆発位置を記録して範囲表示タイマーを開始。
    explosionPosition_ = position_;
    exploded_ = true;
    alive_ = false;
    explosionDrawTimer_ = 0.2f;
}

bool MidRangeBombProjectile::IsAlive() const
{
    bool aliveOrExploding = alive_;
    if (exploded_ && explosionDrawTimer_ > 0.0f)
    {
        aliveOrExploding = true;
    }
    return aliveOrExploding;
}
