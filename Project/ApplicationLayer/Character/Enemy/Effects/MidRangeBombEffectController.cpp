#define NOMINMAX
#include "MidRangeBombEffectController.h"

#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"
#include "GpuParticleType.h"

#include <imgui.h>

#include <algorithm>

using namespace Ken4lowEngine;

namespace
{
    constexpr const char* kTex = "Effects/white.dds";
}

void MidRangeBombEffectController::Initialize()
{
    if (initialized_)
    {
        return;
    }
    // 追加: 爆弾演出専用のGPUエミッターを初期化する。
    CreateEmitters();
    initialized_ = true;
}
void MidRangeBombEffectController::Update(float deltaTime)
{
    (void)deltaTime;
}
void MidRangeBombEffectController::Draw()
{
}
void MidRangeBombEffectController::CreateEmitters()
{
    auto* pm = GpuParticleManager::GetInstance();
    if (!pm)
    {
        return;
    }
    auto createIfNeeded = [pm](const std::string& name, float radius, GpuParticleType type)
    {
        if (pm->GetEmitter(name))
        {
            return;
        }
        GpuParticleEmitter::EmitterInfo info{};
        info.textureFilePath = kTex;
        info.radius = radius;
        info.loopCount = 0;
        info.loopFrequency = 0.0f;
        info.drawType = 0;
        info.kind = GpuParticleKind::Sprite;
        info.spriteType = type;
        info.billboardFlags = BillboardMode::Camera;
        pm->CreateEmitter(name, info);
    };
    // 追加: 各演出別に使い分けるエミッターを作成する。
    createIfNeeded("MidRangeBombThrow", 0.12f, GpuParticleType::Spark);
    createIfNeeded("MidRangeBombTrail", 0.08f, GpuParticleType::Smoke);
    createIfNeeded("MidRangeBombExplosion", 0.35f, GpuParticleType::DeathBurstCore);
    createIfNeeded("MidRangeBombSuicideCharge", 0.16f, GpuParticleType::Spark);
    createIfNeeded("MidRangeBombSuicideExplosion", 0.55f, GpuParticleType::Shockwave);
}
void MidRangeBombEffectController::RequestEffect(const std::string& name, const Vector3& position, uint32_t count, float radiusScale)
{
    auto* pm = GpuParticleManager::GetInstance();
    if (!pm)
    {
        return;
    }
    auto* emitter = pm->GetEmitter(name);
    if (!emitter)
    {
        return;
    }
    emitter->SetPosition(position);
    GpuParticleEmitter::EmitterInfo& info = emitter->GetInfoMutable();
    info.radius = std::max(0.01f, info.radius * radiusScale);
    emitter->RequestEmit(count);
}
void MidRangeBombEffectController::PlayThrowEffect(const Vector3& position, const Vector3& direction)
{
    (void)direction;
    if (!settings_.throwEffectEnabled)
    {
        return;
    }
    // 追加: 爆弾投擲時の小規模スパークを再生する。
    RequestEffect("MidRangeBombThrow", position, static_cast<uint32_t>(std::max(settings_.throwParticleCount, 1.0f)), settings_.throwEffectScale);
}
void MidRangeBombEffectController::PlayBombTrailEffect(const Vector3& position)
{
    if (!settings_.trailEffectEnabled)
    {
        return;
    }
    // 追加: 爆弾飛行中の煙トレイルを再生する。
    RequestEffect("MidRangeBombTrail", position, 6, settings_.trailEffectScale);
}
void MidRangeBombEffectController::PlayBombExplosionEffect(const Vector3& position, float radius)
{
    if (!settings_.explosionEffectEnabled)
    {
        return;
    }
    const float scale = settings_.explosionEffectScale * std::max(radius, 0.1f);
    // 追加: 通常爆弾の爆発パーティクルを再生する。
    RequestEffect("MidRangeBombExplosion", position, static_cast<uint32_t>(std::max(settings_.explosionParticleCount, 1.0f)), scale);
}
void MidRangeBombEffectController::PlaySuicideChargeEffect(const Vector3& position)
{
    if (!settings_.suicideChargeEffectEnabled)
    {
        return;
    }
    // 追加: 時限爆弾モード中の危険演出を再生する。
    RequestEffect("MidRangeBombSuicideCharge", position, 8, settings_.suicideChargeEffectScale);
}
void MidRangeBombEffectController::PlaySuicideExplosionEffect(const Vector3& position, float radius)
{
    if (!settings_.suicideExplosionEffectEnabled)
    {
        return;
    }
    const float scale = settings_.suicideExplosionEffectScale * std::max(radius, 0.1f);
    // 追加: 自爆時の大規模爆発演出を再生する。
    RequestEffect("MidRangeBombSuicideExplosion", position, static_cast<uint32_t>(std::max(settings_.suicideExplosionParticleCount, 1.0f)), scale);
}
void MidRangeBombEffectController::DrawImGui()
{
    ImGui::Checkbox("投げ演出を使う", &settings_.throwEffectEnabled);
    ImGui::Checkbox("飛行トレイルを使う", &settings_.trailEffectEnabled);
    ImGui::Checkbox("通常爆発演出を使う", &settings_.explosionEffectEnabled);
    ImGui::Checkbox("自爆準備演出を使う", &settings_.suicideChargeEffectEnabled);
    ImGui::Checkbox("自爆爆発演出を使う", &settings_.suicideExplosionEffectEnabled);
    ImGui::SliderFloat("投げ粒子数", &settings_.throwParticleCount, 1.0f, 128.0f);
    ImGui::SliderFloat("トレイル発生間隔", &settings_.trailEmitInterval, 0.01f, 0.3f);
    ImGui::SliderFloat("通常爆発粒子数", &settings_.explosionParticleCount, 1.0f, 300.0f);
    ImGui::SliderFloat("自爆準備発生間隔", &settings_.suicideChargeEmitInterval, 0.01f, 0.3f);
    ImGui::SliderFloat("自爆爆発粒子数", &settings_.suicideExplosionParticleCount, 1.0f, 500.0f);
    ImGui::SliderFloat("投げ演出スケール", &settings_.throwEffectScale, 0.1f, 4.0f);
    ImGui::SliderFloat("トレイル演出スケール", &settings_.trailEffectScale, 0.1f, 4.0f);
    ImGui::SliderFloat("通常爆発演出スケール", &settings_.explosionEffectScale, 0.1f, 4.0f);
    ImGui::SliderFloat("自爆準備演出スケール", &settings_.suicideChargeEffectScale, 0.1f, 4.0f);
    ImGui::SliderFloat("自爆爆発演出スケール", &settings_.suicideExplosionEffectScale, 0.1f, 6.0f);
    ImGui::ColorEdit4("投げ色", &settings_.throwColor.x);
    ImGui::ColorEdit4("トレイル色", &settings_.trailColor.x);
    ImGui::ColorEdit4("通常爆発色", &settings_.explosionColor.x);
    ImGui::ColorEdit4("自爆準備色", &settings_.suicideChargeColor.x);
    ImGui::ColorEdit4("自爆爆発色", &settings_.suicideExplosionColor.x);
    if (ImGui::Button("投げ演出を再生"))
    {
        PlayThrowEffect({ 0.0f, 1.5f, 0.0f }, { 0.0f, 0.0f, 1.0f });
    }
    if (ImGui::Button("通常爆発演出を再生"))
    {
        PlayBombExplosionEffect({ 0.0f, 0.2f, 0.0f }, 2.0f);
    }
    if (ImGui::Button("自爆準備演出を再生"))
    {
        PlaySuicideChargeEffect({ 0.0f, 1.0f, 0.0f });
    }
    if (ImGui::Button("自爆爆発演出を再生"))
    {
        PlaySuicideExplosionEffect({ 0.0f, 0.2f, 0.0f }, 4.0f);
    }
}

void MidRangeBombEffectController::LoadFromJson(const nlohmann::json& j)
{
    if (!j.is_object())
    {
        return;
    }
    settings_.throwEffectEnabled = j.value("throwEffectEnabled", settings_.throwEffectEnabled);
    settings_.trailEffectEnabled = j.value("trailEffectEnabled", settings_.trailEffectEnabled);
    settings_.explosionEffectEnabled = j.value("explosionEffectEnabled", settings_.explosionEffectEnabled);
    settings_.suicideChargeEffectEnabled = j.value("suicideChargeEffectEnabled", settings_.suicideChargeEffectEnabled);
    settings_.suicideExplosionEffectEnabled = j.value("suicideExplosionEffectEnabled", settings_.suicideExplosionEffectEnabled);
}

void MidRangeBombEffectController::SaveToJson(nlohmann::json& j) const
{
    j["throwEffectEnabled"] = settings_.throwEffectEnabled;
    j["trailEffectEnabled"] = settings_.trailEffectEnabled;
    j["explosionEffectEnabled"] = settings_.explosionEffectEnabled;
    j["suicideChargeEffectEnabled"] = settings_.suicideChargeEffectEnabled;
    j["suicideExplosionEffectEnabled"] = settings_.suicideExplosionEffectEnabled;
}

const MidRangeBombEffectController::BombEffectSettings& MidRangeBombEffectController::GetSettings() const
{
    return settings_;
}
