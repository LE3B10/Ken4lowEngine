#pragma once

#include "Vector3.h"
#include "Object3D.h"

#include <memory>

class Player;

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
    enum class PlayerHitResult
    {
        None,
        Direct,
        Explosion,
        DirectAndExplosion,
    };

    void Initialize();

    void Launch(
        const Ken4lowEngine::Vector3& start,
        const Ken4lowEngine::Vector3& target,
        const BombProjectileSettings& settings
    );

    void Update(float deltaTime);
    void Draw() const;
    void Explode();
    PlayerHitResult TryApplyPlayerDamage(Player& player);
    bool IsAlive() const;

    const Ken4lowEngine::Vector3& GetPosition() const { return position_; }
    static bool IsDebugCubeVisible() { return s_debugCubeVisible_; }
    static void SetDebugCubeVisible(bool visible) { s_debugCubeVisible_ = visible; }
    static float GetDebugCubeSize() { return s_debugCubeSize_; }
    static void SetDebugCubeSize(float size);

private:
    void UpdateDebugCube();

private:
    Ken4lowEngine::Vector3 position_{};
    Ken4lowEngine::Vector3 velocity_{};
    Ken4lowEngine::Vector3 explosionPosition_{};
    BombProjectileSettings settings_{};
    float lifeTimer_ = 0.0f;
    float explosionDrawTimer_ = 0.0f;
    bool exploded_ = false;
    bool alive_ = false;
    bool directDamageApplied_ = false;
    bool explosionDamageApplied_ = false;
    std::unique_ptr<Ken4lowEngine::Object3D> debugCube_;

    static bool s_debugCubeVisible_;
    static float s_debugCubeSize_;
};
