#pragma once

#include "Vector3.h"

struct BombProjectileSettings
{
    float initialSpeed = 10.0f;
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
