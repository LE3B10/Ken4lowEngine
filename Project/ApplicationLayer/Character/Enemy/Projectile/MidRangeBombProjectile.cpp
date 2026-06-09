#define NOMINMAX
#include "MidRangeBombProjectile.h"

#include "ApplicationLayer/Character/Player/Player.h"
#include "Wireframe.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

bool MidRangeBombProjectile::s_debugCubeVisible_ = true;
float MidRangeBombProjectile::s_debugCubeSize_ = 0.4f;

void MidRangeBombProjectile::Initialize()
{
    // 爆弾ランタイム状態の初期化。
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
    debugCube_->Initialize("Test/cube.gltf");
    debugCube_->SetColor({ 0.15f, 0.02f, 0.22f, 1.0f });
    UpdateDebugCube();
}

void MidRangeBombProjectile::Launch(
    const Vector3& start,
    const Vector3& target,
    const BombProjectileSettings& settings
)
{
    // 爆弾設定と放物線初速の設定。
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
        if (lifeTimer_ >= settings_.lifeTime)
        {
            shouldExplode = true;
        }
        if (position_.y <= kFloorY)
        {
            position_.y = kFloorY + s_debugCubeSize_ * 0.5f;
            UpdateDebugCube();
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
        // 中距離雑魚敵の爆弾挙動を確認しやすくするため、仮Cubeで描画する。
        if (s_debugCubeVisible_ && debugCube_)
        {
            debugCube_->Draw();
        }
    }

    if (exploded_ && explosionDrawTimer_ > 0.0f)
    {
        Wireframe::GetInstance()->DrawSphere(explosionPosition_, settings_.explosionRadius, { 1.0f, 0.1f, 0.1f, 0.85f });
    }
}

void MidRangeBombProjectile::Explode()
{
    // 爆発位置を記録して範囲表示タイマーを開始。
    explosionPosition_ = position_;
    if (explosionPosition_.y < 0.2f)
    {
        // 爆発演出が地面下に埋まらないよう高さを補正する。
        explosionPosition_.y = 0.2f;
    }
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
    if (!debugCube_)
    {
        return;
    }

    debugCube_->SetTranslate(position_);
    debugCube_->SetRotate({ lifeTimer_ * 2.0f, lifeTimer_ * 3.5f, lifeTimer_ * 1.5f });
    debugCube_->SetScale({ s_debugCubeSize_, s_debugCubeSize_, s_debugCubeSize_ });
    debugCube_->Update();
}


MidRangeBombProjectile::PlayerHitResult MidRangeBombProjectile::TryApplyPlayerDamage(Player& player)
{
    auto* playerTransform = player.GetWorldTransform();
    if (!playerTransform)
    {
        return PlayerHitResult::None;
    }

    PlayerHitResult result = PlayerHitResult::None;
    const Vector3 playerPosition = playerTransform->translate_;

    if (alive_ && !directDamageApplied_)
    {
        const float directDistance = Vector3::Length(playerPosition - position_);
        if (directDistance <= settings_.hitRadius)
        {
            // 中距離雑魚敵の爆弾Cubeがプレイヤーへ直撃した瞬間だけHPを減らす。
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
                // 中距離雑魚敵の爆弾範囲内にプレイヤーが入った場合のみダメージを与える。
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

bool MidRangeBombProjectile::IsAlive() const
{
    bool aliveOrExploding = alive_;
    if (exploded_ && explosionDrawTimer_ > 0.0f)
    {
        aliveOrExploding = true;
    }
    return aliveOrExploding;
}
