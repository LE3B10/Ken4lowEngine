#define NOMINMAX
#include "MidRangeBombProjectile.h"
#include "ApplicationLayer/Character/Enemy/Effects/MidRangeBombEffectController.h"

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
    trailEffectTimer_ = 0.0f;
    effectController_ = nullptr;
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
    trailEffectTimer_ = 0.0f;

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
        if (effectController_)
        {
            // 追加: 飛行中は一定間隔でトレイル演出を出す。
            trailEffectTimer_ -= deltaTime;
            if (trailEffectTimer_ <= 0.0f)
            {
                effectController_->PlayBombTrailEffect(position_);
                const auto& settings = effectController_->GetSettings();
                trailEffectTimer_ = std::max(settings.trailEmitInterval, 0.01f);
            }
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
    if (explosionPosition_.y < 0.2f)
    {
        // 追加: 爆発演出が地面下に埋まらないよう高さを補正する。
        explosionPosition_.y = 0.2f;
    }
    if (effectController_)
    {
        // 追加: 通常爆弾はGPU爆発とメッシュ破片を同時再生する。
        effectController_->PlayBombExplosionFullEffect(explosionPosition_, settings_.explosionRadius);
    }
    exploded_ = true;
    alive_ = false;
    explosionDrawTimer_ = 0.2f;
}
void MidRangeBombProjectile::SetEffectController(MidRangeBombEffectController* effectController)
{
    // 追加: 所有しない演出コントローラー参照を設定する。
    effectController_ = effectController;
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
