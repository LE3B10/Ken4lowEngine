#pragma once

#include "Vector3.h"
struct BombProjectileSettings
{
    // 追加: 距離に応じた初速調整の基本値。
    float initialSpeed = 10.0f;
    // 追加: 距離に応じて初速を変えるかどうか。
    bool useDistanceBasedSpeed = true;
    // 追加: 距離可変時の最小初速。
    float minInitialSpeed = 7.0f;
    // 追加: 距離可変時の最大初速。
    float maxInitialSpeed = 16.0f;
    // 追加: 基準距離を超えた分の距離1mあたり初速加算量。
    float speedPerDistance = 0.55f;
    // 追加: 初速加算を始める基準距離。
    float speedBaseDistance = 6.0f;
    float upwardVelocity = 7.0f;
    float gravity = 18.0f;
    float lifeTime = 4.0f;
    float hitRadius = 0.45f;
    float explosionRadius = 3.0f;
    int directHitDamage = 25;
    int explosionDamage = 12;
    bool directHitAlsoExplosionDamage = false;
};

class MidRangeBombProjectile
{
public:
    void Initialize();

    void Launch(
        const Ken4lowEngine::Vector3& start,
        const Ken4lowEngine::Vector3& target,
        const BombProjectileSettings& settings
    );

    void Update(float deltaTime);
    void Draw() const;
    void Explode();
    bool IsAlive() const;

private:
    Ken4lowEngine::Vector3 position_{};
    Ken4lowEngine::Vector3 velocity_{};
    Ken4lowEngine::Vector3 explosionPosition_{};
    BombProjectileSettings settings_{};
    float lifeTimer_ = 0.0f;
    float explosionDrawTimer_ = 0.0f;
    bool exploded_ = false;
    bool alive_ = false;
};
