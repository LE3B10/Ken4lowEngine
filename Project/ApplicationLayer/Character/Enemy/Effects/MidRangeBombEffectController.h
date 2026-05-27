#pragma once

#include <cstdint>
#include <string>

#include <json.hpp>

#include "Vector3.h"
#include "Vector4.h"
#include "ModelParticle.h"

namespace K4E = ::Ken4lowEngine;

class MidRangeBombEffectController
{
public:
    struct BombEffectSettings
    {
        // 追加: 爆発演出の最低表示高さを管理する。
        float effectPositionMinY = 0.2f;
        float effectHeightOffset = 0.02f;
        bool throwEffectEnabled = true;
        bool trailEffectEnabled = true;
        bool explosionEffectEnabled = true;
        bool suicideChargeEffectEnabled = true;
        bool suicideExplosionEffectEnabled = true;

        float throwParticleCount = 16.0f;
        float trailEmitInterval = 0.05f;
        float explosionParticleCount = 80.0f;
        float suicideChargeEmitInterval = 0.04f;
        float suicideExplosionParticleCount = 160.0f;

        float throwEffectScale = 1.0f;
        float trailEffectScale = 0.6f;
        float explosionEffectScale = 1.0f;
        float suicideChargeEffectScale = 1.0f;
        float suicideExplosionEffectScale = 1.5f;

        K4E::Vector4 throwColor{ 1.0f, 0.7f, 0.2f, 1.0f };
        K4E::Vector4 trailColor{ 0.5f, 0.5f, 0.5f, 1.0f };
        K4E::Vector4 explosionColor{ 1.0f, 0.25f, 0.05f, 1.0f };
        K4E::Vector4 suicideChargeColor{ 1.0f, 0.0f, 0.0f, 1.0f };
        K4E::Vector4 suicideExplosionColor{ 1.0f, 0.0f, 0.0f, 1.0f };
        bool meshExplosionEnabled = true;
        int meshDebrisCount = 18;
        int suicideMeshDebrisCount = 36;
        float meshDebrisSpeed = 8.0f;
        float suicideMeshDebrisSpeed = 13.0f;
        float meshDebrisUpPower = 7.0f;
        float meshDebrisGravity = 18.0f;
        float meshDebrisLifeTime = 0.8f;
        float meshDebrisScale = 0.25f;
        float suicideMeshDebrisScale = 0.35f;
        K4E::Vector4 meshDebrisColor{ 0.9f, 0.45f, 0.1f, 1.0f };
        // 追加: 中心フラッシュ設定を管理する。
        bool flashEnabled = true;
        float flashDuration = 0.12f;
        float flashScale = 1.2f;
        float suicideFlashScale = 1.8f;
        K4E::Vector4 flashColor{ 1.0f, 0.75f, 0.15f, 1.0f };
        // 追加: 衝撃波設定を管理する。
        bool shockwaveEnabled = true;
        float shockwaveDuration = 0.25f;
        float shockwaveStartRadiusRate = 0.2f;
        float shockwaveEndRadiusRate = 1.0f;
        float shockwaveHeightOffset = 0.05f;
        K4E::Vector4 shockwaveColor{ 1.0f, 0.55f, 0.1f, 0.8f };
        // 追加: 煙設定を管理する。
        bool smokeEnabled = true;
        float smokeScale = 0.8f;
        float smokeLifeTime = 0.9f;
        float smokeUpPower = 3.8f;
        float smokeSpreadRadius = 0.25f;
        K4E::Vector4 smokeColor{ 0.25f, 0.25f, 0.25f, 0.9f };
        // 追加: 通常爆弾と自爆のスケール差を管理する。
        float normalExplosionScale = 1.0f;
        float suicideExplosionScale = 1.35f;
    };

public:
    void Initialize();
    void Update(float deltaTime);
    void Draw();

    void PlayThrowEffect(const K4E::Vector3& position, const K4E::Vector3& direction);
    void PlayBombTrailEffect(const K4E::Vector3& position);
    void PlayBombExplosionEffect(const K4E::Vector3& position, float radius);
    void PlaySmokeEffect(const K4E::Vector3& position, float radius);
    void PlayExplosionFlashEffect(const K4E::Vector3& position, float radius);
    void PlayShockwaveEffect(const K4E::Vector3& position, float radius);
    void PlayBombMeshExplosionEffect(const K4E::Vector3& position, float radius);
    void PlayBombExplosionFullEffect(const K4E::Vector3& position, float radius);
    void PlaySuicideChargeEffect(const K4E::Vector3& position);
    void PlaySuicideExplosionEffect(const K4E::Vector3& position, float radius);
    void PlaySuicideMeshExplosionEffect(const K4E::Vector3& position, float radius);
    void PlaySuicideExplosionFullEffect(const K4E::Vector3& position, float radius);

    void DrawImGui();
    void LoadFromJson(const nlohmann::json& j);
    void SaveToJson(nlohmann::json& j) const;
    void ResetToDefault();

    const BombEffectSettings& GetSettings() const;

private:
    void CreateEmitters();
    void RequestEffect(const std::string& name, const K4E::Vector3& position, uint32_t count, float radiusScale);
    K4E::Vector3 SanitizeEffectPosition(const K4E::Vector3& position) const;
    void ApplyModelParticleTuning(float speed, float scale, float lifeTime);
    uint32_t GetActiveDebrisCount() const;

private:
    BombEffectSettings settings_{};
    ModelParticle meshDebrisEffect_{};
    ModelParticle flashEffect_{};
    ModelParticle shockwaveEffect_{};
    ModelParticle smokeEffect_{};
    K4E::Vector3 lastBombExplosionPosition_{};
    K4E::Vector3 lastSuicideExplosionPosition_{};
    bool lastBombExplosionPlayed_ = false;
    bool lastSuicideExplosionPlayed_ = false;
    bool initialized_ = false;
};
