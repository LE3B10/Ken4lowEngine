#pragma once

#include <cstdint>
#include <string>

#include <json.hpp>

#include "Vector3.h"
#include "Vector4.h"

namespace K4E = ::Ken4lowEngine;

class MidRangeBombEffectController
{
public:
    struct BombEffectSettings
    {
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
    };

public:
    void Initialize();
    void Update(float deltaTime);
    void Draw();

    void PlayThrowEffect(const K4E::Vector3& position, const K4E::Vector3& direction);
    void PlayBombTrailEffect(const K4E::Vector3& position);
    void PlayBombExplosionEffect(const K4E::Vector3& position, float radius);
    void PlaySuicideChargeEffect(const K4E::Vector3& position);
    void PlaySuicideExplosionEffect(const K4E::Vector3& position, float radius);

    void DrawImGui();
    void LoadFromJson(const nlohmann::json& j);
    void SaveToJson(nlohmann::json& j) const;

    const BombEffectSettings& GetSettings() const;

private:
    void CreateEmitters();
    void RequestEffect(const std::string& name, const K4E::Vector3& position, uint32_t count, float radiusScale);

private:
    BombEffectSettings settings_{};
    bool initialized_ = false;
};
