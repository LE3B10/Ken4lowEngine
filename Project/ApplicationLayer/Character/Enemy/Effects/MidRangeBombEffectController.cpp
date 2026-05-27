#define NOMINMAX
#include "MidRangeBombEffectController.h"

#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"
#include "GpuParticleType.h"

#include <imgui.h>

#include <algorithm>
#include <numbers>
#include <cmath>

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
    // 追加: 爆発破片用のメッシュパーティクルを初期化する。
    meshDebrisEffect_.Initialize();
    flashEffect_.Initialize();
    shockwaveEffect_.Initialize();
    smokeEffect_.Initialize();
    initialized_ = true;
}
void MidRangeBombEffectController::Update(float deltaTime)
{
    // 追加: フレーム番号と経過時間を更新して再生回数デバッグに使う。
    frameCounter_++;
    lastExplosionPlayTime_ += std::max(0.0f, deltaTime);
    // 追加: メッシュ破片の生存時間と物理を更新する。
    meshDebrisEffect_.Update();
    flashEffect_.Update();
    shockwaveEffect_.Update();
    smokeEffect_.Update();
}
void MidRangeBombEffectController::Draw()
{
    // 追加: 爆発破片のメッシュ描画を実行する。
    meshDebrisEffect_.Draw();
    flashEffect_.Draw();
    shockwaveEffect_.Draw();
    smokeEffect_.Draw();
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
    createIfNeeded("MidRangeBombSmoke", 0.28f, GpuParticleType::Smoke);
    createIfNeeded("MidRangeBombShockwave", 0.2f, GpuParticleType::Shockwave);
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
    const Vector3 effectPosition = SanitizeEffectPosition(position);
    const float scale = settings_.explosionEffectScale * settings_.normalExplosionScale * std::max(radius, 0.1f);
    // 追加: 通常爆弾の爆発パーティクルを再生する。
    RequestEffect("MidRangeBombExplosion", effectPosition, static_cast<uint32_t>(std::max(settings_.explosionParticleCount, 1.0f)), scale);
    lastBombExplosionPosition_ = effectPosition;
    lastBombExplosionPlayed_ = true;
}
void MidRangeBombEffectController::PlaySmokeEffect(const Vector3& position, float radius)
{
    if (!settings_.smokeEnabled)
    {
        return;
    }
    // 追加: 爆発中心から少し上に煙を固定発生させる。
    Vector3 smokePosition = SanitizeEffectPosition(position);
    smokePosition.y += settings_.effectHeightOffset;
    const float scale = settings_.smokeScale * std::max(radius, 0.1f);
    RequestEffect("MidRangeBombSmoke", smokePosition, 14, scale);
    ApplyModelParticleTuning(settings_.smokeSpreadRadius, settings_.smokeScale, settings_.smokeLifeTime);
    smokeEffect_.SetParticleColor(settings_.smokeColor);
    // 追加: 煙の横方向速度を弱く固定して見失いを防ぐ。
    smokeEffect_.SpawnBurst(smokePosition, { settings_.smokeHorizontalPower, settings_.smokeUpPower, settings_.smokeHorizontalPower * 0.25f }, 8);
}
void MidRangeBombEffectController::PlayExplosionFlashEffect(const Vector3& position, float radius)
{
    if (!settings_.flashEnabled)
    {
        return;
    }
    // 追加: 爆発瞬間を強調する中心フラッシュを再生する。
    const Vector3 effectPosition = SanitizeEffectPosition(position);
    const float safeRadius = std::max(radius, 0.1f);
    const float flashScale = std::min(safeRadius * settings_.flashNormalRadiusRate, settings_.flashMaxScale);
    flashEffect_.SetBurstTuning(0.08f, 0.0f, settings_.flashDuration * 0.3f, settings_.flashDuration, flashScale);
    Vector4 flashColor = settings_.flashColor;
    flashColor.w = settings_.flashAlpha;
    flashEffect_.SetParticleColor(flashColor);
    flashEffect_.SpawnBurst(effectPosition, { 0.0f, 1.0f, 0.0f }, 12);
}
void MidRangeBombEffectController::PlayShockwaveEffect(const Vector3& position, float radius)
{
    if (!settings_.shockwaveEnabled)
    {
        return;
    }
    // 追加: 爆発半径に追従する衝撃波リングを再生する。
    Vector3 shockwavePosition = SanitizeEffectPosition(position);
    shockwavePosition.y += settings_.shockwaveHeightOffset;
    const float ringScale = std::max(settings_.shockwaveEndRadiusRate, settings_.shockwaveStartRadiusRate) * std::max(radius, 0.1f);
    RequestEffect("MidRangeBombShockwave", shockwavePosition, 12, ringScale);
    shockwaveEffect_.SetBurstTuning(ringScale, 0.0f, settings_.shockwaveDuration * 0.5f, settings_.shockwaveDuration, 0.06f);
    Vector4 shockwaveColor = settings_.shockwaveColor;
    shockwaveColor.w = settings_.shockwaveAlpha;
    shockwaveEffect_.SetParticleColor(shockwaveColor);
    shockwaveEffect_.SpawnBurst(shockwavePosition, { 0.0f, 0.0f, 1.0f }, 16);
}
void MidRangeBombEffectController::PlayBombMeshExplosionEffect(const Vector3& position, float radius)
{
    if (!settings_.meshExplosionEnabled)
    {
        return;
    }
    // 追加: 通常爆弾向けのメッシュ破片チューニングを適用する。
    const Vector3 effectPosition = SanitizeEffectPosition(position);
    ApplyModelParticleTuning(settings_.meshDebrisSpeed + (radius * 0.8f), settings_.meshDebrisScale, settings_.meshDebrisLifeTime);
    const Vector3 normal = { 0.0f, settings_.meshDebrisUpPower, 0.0f };
    meshDebrisEffect_.SpawnBurst(effectPosition, normal, static_cast<uint32_t>(std::max(settings_.meshDebrisCount, 1)));
}
void MidRangeBombEffectController::PlayBombExplosionFullEffect(const Vector3& position, float radius)
{
    if (!settings_.enableProductionExplosionEffect)
    {
        return;
    }
    // 追加: 通常爆弾の4段構成爆発演出を一本化して再生する。
    const float scaledRadius = radius * settings_.normalExplosionScale;
    PlayExplosionFlashEffect(position, scaledRadius);
    PlayShockwaveEffect(position, radius * settings_.normalExplosionScale);
    PlayBombMeshExplosionEffect(position, radius);
    PlayBombExplosionEffect(position, radius);
    PlaySmokeEffect(position, radius);
    // 追加: 通常爆発の呼び出し回数を記録して多重再生を監視する。
    normalExplosionPlayCount_++;
    lastExplosionPlayFrame_ = frameCounter_;
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
    const Vector3 effectPosition = SanitizeEffectPosition(position);
    const float scale = settings_.suicideExplosionEffectScale * settings_.suicideExplosionScale * std::max(radius, 0.1f);
    // 追加: 自爆時の大規模爆発演出を再生する。
    RequestEffect("MidRangeBombSuicideExplosion", effectPosition, static_cast<uint32_t>(std::max(settings_.suicideExplosionParticleCount, 1.0f)), scale);
    lastSuicideExplosionPosition_ = effectPosition;
    lastSuicideExplosionPlayed_ = true;
}
void MidRangeBombEffectController::PlaySuicideMeshExplosionEffect(const Vector3& position, float radius)
{
    if (!settings_.meshExplosionEnabled)
    {
        return;
    }
    // 追加: 自爆向けに大きめのメッシュ破片チューニングを適用する。
    const Vector3 effectPosition = SanitizeEffectPosition(position);
    ApplyModelParticleTuning(settings_.suicideMeshDebrisSpeed + radius, settings_.suicideMeshDebrisScale, settings_.meshDebrisLifeTime + 0.2f);
    const Vector3 normal = { 0.0f, settings_.meshDebrisUpPower * 1.3f, 0.0f };
    meshDebrisEffect_.SpawnBurst(effectPosition, normal, static_cast<uint32_t>(std::max(settings_.suicideMeshDebrisCount, 1)));
}
void MidRangeBombEffectController::PlaySuicideExplosionFullEffect(const Vector3& position, float radius)
{
    if (!settings_.enableProductionExplosionEffect)
    {
        return;
    }
    // 追加: 自爆の4段構成爆発演出を一本化して再生する。
    const float safeRadius = std::max(radius, 0.1f);
    const float flashScale = std::min(safeRadius * settings_.flashSuicideRadiusRate, settings_.flashMaxScale);
    const Vector3 effectPosition = SanitizeEffectPosition(position);
    flashEffect_.SetBurstTuning(0.08f, 0.0f, settings_.flashDuration * 0.3f, settings_.flashDuration, flashScale);
    Vector4 flashColor = settings_.flashColor;
    flashColor.w = settings_.flashAlpha;
    flashEffect_.SetParticleColor(flashColor);
    flashEffect_.SpawnBurst(effectPosition, { 0.0f, 1.0f, 0.0f }, 12);
    PlayShockwaveEffect(position, radius * settings_.suicideExplosionScale);
    PlaySuicideMeshExplosionEffect(position, radius);
    PlaySuicideExplosionEffect(position, radius);
    PlaySmokeEffect(position, radius * 1.2f);
    // 追加: 自爆爆発の呼び出し回数を記録して多重再生を監視する。
    suicideExplosionPlayCount_++;
    lastExplosionPlayFrame_ = frameCounter_;
}
void MidRangeBombEffectController::DrawImGui()
{
#ifdef USE_IMGUI
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
    ImGui::Checkbox("本番爆発エフェクトを使う", &settings_.enableProductionExplosionEffect);
    ImGui::Checkbox("デバッグ爆発範囲を表示", &settings_.showExplosionRadius);
    ImGui::Checkbox("デバッグ自爆範囲を表示", &settings_.showSuicideRadius);
    ImGui::Checkbox("メッシュ破片を使う", &settings_.meshExplosionEnabled);
    ImGui::SliderInt("通常爆弾の破片数", &settings_.meshDebrisCount, 1, 128);
    ImGui::SliderInt("自爆の破片数", &settings_.suicideMeshDebrisCount, 1, 256);
    ImGui::SliderFloat("通常破片速度", &settings_.meshDebrisSpeed, 0.5f, 24.0f);
    ImGui::SliderFloat("自爆破片速度", &settings_.suicideMeshDebrisSpeed, 0.5f, 32.0f);
    ImGui::SliderFloat("破片上方向力", &settings_.meshDebrisUpPower, 0.0f, 24.0f);
    ImGui::SliderFloat("破片重力", &settings_.meshDebrisGravity, 0.1f, 48.0f);
    ImGui::SliderFloat("破片寿命", &settings_.meshDebrisLifeTime, 0.1f, 2.5f);
    ImGui::SliderFloat("通常破片スケール", &settings_.meshDebrisScale, 0.05f, 0.6f);
    ImGui::SliderFloat("自爆破片スケール", &settings_.suicideMeshDebrisScale, 0.05f, 0.8f);
    ImGui::SliderFloat("破片最大スケール", &settings_.meshDebrisMaxScale, 0.1f, 1.0f);
    ImGui::ColorEdit4("破片色", &settings_.meshDebrisColor.x);
    ImGui::Checkbox("中心フラッシュを使う", &settings_.flashEnabled);
    ImGui::SliderFloat("フラッシュ時間", &settings_.flashDuration, 0.08f, 0.18f);
    ImGui::SliderFloat("通常フラッシュ半径率", &settings_.flashNormalRadiusRate, 0.25f, 0.45f);
    ImGui::SliderFloat("自爆フラッシュ半径率", &settings_.flashSuicideRadiusRate, 0.5f, 0.8f);
    ImGui::SliderFloat("フラッシュ最大スケール", &settings_.flashMaxScale, 0.2f, 6.0f);
    ImGui::SliderFloat("フラッシュ透明度", &settings_.flashAlpha, 0.0f, 1.0f);
    ImGui::ColorEdit4("フラッシュ色", &settings_.flashColor.x);
    ImGui::Checkbox("衝撃波を使う", &settings_.shockwaveEnabled);
    ImGui::SliderFloat("衝撃波時間", &settings_.shockwaveDuration, 0.05f, 0.8f);
    ImGui::SliderFloat("衝撃波開始半径率", &settings_.shockwaveStartRadiusRate, 0.05f, 1.0f);
    ImGui::SliderFloat("衝撃波終了半径率", &settings_.shockwaveEndRadiusRate, 0.2f, 2.0f);
    ImGui::SliderFloat("衝撃波高さ", &settings_.shockwaveHeightOffset, 0.0f, 0.5f);
    ImGui::SliderFloat("衝撃波透明度", &settings_.shockwaveAlpha, 0.0f, 1.0f);
    ImGui::ColorEdit4("衝撃波色", &settings_.shockwaveColor.x);
    ImGui::Checkbox("煙を使う", &settings_.smokeEnabled);
    ImGui::SliderFloat("煙スケール", &settings_.smokeScale, 0.1f, 1.2f);
    ImGui::SliderFloat("煙寿命", &settings_.smokeLifeTime, 0.2f, 1.8f);
    ImGui::SliderFloat("煙上方向力", &settings_.smokeUpPower, 0.2f, 4.0f);
    ImGui::SliderFloat("煙横方向力", &settings_.smokeHorizontalPower, 0.0f, 2.0f);
    ImGui::SliderFloat("煙拡散半径", &settings_.smokeSpreadRadius, 0.05f, 2.0f);
    ImGui::ColorEdit4("煙色", &settings_.smokeColor.x);
    ImGui::SliderFloat("通常爆発スケール", &settings_.normalExplosionScale, 0.2f, 2.5f);
    ImGui::SliderFloat("自爆爆発スケール", &settings_.suicideExplosionScale, 0.5f, 3.5f);
    ImGui::SliderFloat("エフェクト最低Y", &settings_.effectPositionMinY, 0.0f, 2.0f);
    ImGui::SliderFloat("エフェクト高さオフセット", &settings_.effectHeightOffset, 0.0f, 0.5f);
    ImGui::Text("最後の通常爆発位置: (%.2f, %.2f, %.2f)", lastBombExplosionPosition_.x, lastBombExplosionPosition_.y, lastBombExplosionPosition_.z);
    ImGui::Text("最後の自爆爆発位置: (%.2f, %.2f, %.2f)", lastSuicideExplosionPosition_.x, lastSuicideExplosionPosition_.y, lastSuicideExplosionPosition_.z);
    ImGui::Text("最後に通常爆発を再生したか: %s", lastBombExplosionPlayed_ ? "はい" : "いいえ");
    ImGui::Text("最後に自爆爆発を再生したか: %s", lastSuicideExplosionPlayed_ ? "はい" : "いいえ");
    ImGui::Text("現在のメッシュ破片数: %u", GetActiveDebrisCount());
    ImGui::Text("現在のフラッシュ数: %u", flashEffect_.GetActiveCount());
    ImGui::Text("現在の衝撃波数: %u", shockwaveEffect_.GetActiveCount());
    ImGui::Text("現在の煙エミッター数: %u", smokeEffect_.GetActiveCount());
    ImGui::Text("通常爆発再生回数: %u", normalExplosionPlayCount_);
    ImGui::Text("自爆爆発再生回数: %u", suicideExplosionPlayCount_);
    ImGui::Text("最後に爆発を再生したフレーム: %llu", static_cast<unsigned long long>(lastExplosionPlayFrame_));
    ImGui::Text("最後に爆発を再生した時間: %.2f", lastExplosionPlayTime_);
    ImGui::Text("effectControllerが有効か: %s", initialized_ ? "はい" : "いいえ");
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
    if (ImGui::Button("通常メッシュ破片を再生"))
    {
        PlayBombMeshExplosionEffect({ 0.0f, 0.2f, 0.0f }, 2.0f);
    }
    if (ImGui::Button("自爆メッシュ破片を再生"))
    {
        PlaySuicideMeshExplosionEffect({ 0.0f, 0.2f, 0.0f }, 4.0f);
    }
    if (ImGui::Button("安全な爆発設定に戻す"))
    {
        ApplySafeExplosionDefaults();
    }
    if (ImGui::Button("全部まとめて通常爆発テスト"))
    {
        PlayBombExplosionFullEffect({ 0.0f, 0.2f, 0.0f }, 2.0f);
    }
    if (ImGui::Button("全部まとめて自爆爆発テスト"))
    {
        PlaySuicideExplosionFullEffect({ 0.0f, 0.2f, 0.0f }, 4.0f);
    }
    ValidateSettings();

#endif // USE_IMGUI
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
    settings_.meshExplosionEnabled = j.value("meshExplosionEnabled", settings_.meshExplosionEnabled);
    settings_.meshDebrisCount = j.value("meshDebrisCount", settings_.meshDebrisCount);
    settings_.suicideMeshDebrisCount = j.value("suicideMeshDebrisCount", settings_.suicideMeshDebrisCount);
    settings_.meshDebrisSpeed = j.value("meshDebrisSpeed", settings_.meshDebrisSpeed);
    settings_.suicideMeshDebrisSpeed = j.value("suicideMeshDebrisSpeed", settings_.suicideMeshDebrisSpeed);
    settings_.meshDebrisUpPower = j.value("meshDebrisUpPower", settings_.meshDebrisUpPower);
    settings_.meshDebrisGravity = j.value("meshDebrisGravity", settings_.meshDebrisGravity);
    settings_.meshDebrisLifeTime = j.value("meshDebrisLifeTime", settings_.meshDebrisLifeTime);
    settings_.meshDebrisScale = j.value("meshDebrisScale", settings_.meshDebrisScale);
    settings_.suicideMeshDebrisScale = j.value("suicideMeshDebrisScale", settings_.suicideMeshDebrisScale);
    settings_.flashEnabled = j.value("flashEnabled", settings_.flashEnabled);
    settings_.flashDuration = j.value("flashDuration", settings_.flashDuration);
    settings_.flashNormalRadiusRate = j.value("flashNormalRadiusRate", settings_.flashNormalRadiusRate);
    settings_.flashSuicideRadiusRate = j.value("flashSuicideRadiusRate", settings_.flashSuicideRadiusRate);
    settings_.flashMaxScale = j.value("flashMaxScale", settings_.flashMaxScale);
    settings_.flashAlpha = j.value("flashAlpha", settings_.flashAlpha);
    settings_.shockwaveEnabled = j.value("shockwaveEnabled", settings_.shockwaveEnabled);
    settings_.shockwaveDuration = j.value("shockwaveDuration", settings_.shockwaveDuration);
    settings_.shockwaveStartRadiusRate = j.value("shockwaveStartRadiusRate", settings_.shockwaveStartRadiusRate);
    settings_.shockwaveEndRadiusRate = j.value("shockwaveEndRadiusRate", settings_.shockwaveEndRadiusRate);
    settings_.shockwaveHeightOffset = j.value("shockwaveHeightOffset", settings_.shockwaveHeightOffset);
    settings_.shockwaveAlpha = j.value("shockwaveAlpha", settings_.shockwaveAlpha);
    settings_.smokeEnabled = j.value("smokeEnabled", settings_.smokeEnabled);
    settings_.smokeScale = j.value("smokeScale", settings_.smokeScale);
    settings_.smokeLifeTime = j.value("smokeLifeTime", settings_.smokeLifeTime);
    settings_.smokeUpPower = j.value("smokeUpPower", settings_.smokeUpPower);
    settings_.smokeHorizontalPower = j.value("smokeHorizontalPower", settings_.smokeHorizontalPower);
    settings_.smokeSpreadRadius = j.value("smokeSpreadRadius", settings_.smokeSpreadRadius);
    settings_.normalExplosionScale = j.value("normalExplosionScale", settings_.normalExplosionScale);
    settings_.suicideExplosionScale = j.value("suicideExplosionScale", settings_.suicideExplosionScale);
    settings_.effectPositionMinY = j.value("effectPositionMinY", settings_.effectPositionMinY);
    settings_.effectHeightOffset = j.value("effectHeightOffset", settings_.effectHeightOffset);
    settings_.meshDebrisMaxScale = j.value("meshDebrisMaxScale", settings_.meshDebrisMaxScale);
    settings_.enableProductionExplosionEffect = j.value("enableProductionExplosionEffect", settings_.enableProductionExplosionEffect);
    settings_.showExplosionRadius = j.value("showExplosionRadius", settings_.showExplosionRadius);
    settings_.showSuicideRadius = j.value("showSuicideRadius", settings_.showSuicideRadius);
    if (j.contains("flashColor"))
    {
        const auto& color = j["flashColor"];
        if (color.is_array() && color.size() == 4)
        {
            settings_.flashColor = { color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>() };
        }
    }
    if (j.contains("shockwaveColor"))
    {
        const auto& color = j["shockwaveColor"];
        if (color.is_array() && color.size() == 4)
        {
            settings_.shockwaveColor = { color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>() };
        }
    }
    if (j.contains("smokeColor"))
    {
        const auto& color = j["smokeColor"];
        if (color.is_array() && color.size() == 4)
        {
            settings_.smokeColor = { color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>() };
        }
    }
    if (j.contains("meshDebrisColor"))
    {
        const auto& color = j["meshDebrisColor"];
        if (color.is_array() && color.size() == 4)
        {
            settings_.meshDebrisColor = { color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>() };
        }
    }
    ValidateSettings();
}

void MidRangeBombEffectController::SaveToJson(nlohmann::json& j) const
{
    j["throwEffectEnabled"] = settings_.throwEffectEnabled;
    j["trailEffectEnabled"] = settings_.trailEffectEnabled;
    j["explosionEffectEnabled"] = settings_.explosionEffectEnabled;
    j["suicideChargeEffectEnabled"] = settings_.suicideChargeEffectEnabled;
    j["suicideExplosionEffectEnabled"] = settings_.suicideExplosionEffectEnabled;
    j["meshExplosionEnabled"] = settings_.meshExplosionEnabled;
    j["meshDebrisCount"] = settings_.meshDebrisCount;
    j["suicideMeshDebrisCount"] = settings_.suicideMeshDebrisCount;
    j["meshDebrisSpeed"] = settings_.meshDebrisSpeed;
    j["suicideMeshDebrisSpeed"] = settings_.suicideMeshDebrisSpeed;
    j["meshDebrisUpPower"] = settings_.meshDebrisUpPower;
    j["meshDebrisGravity"] = settings_.meshDebrisGravity;
    j["meshDebrisLifeTime"] = settings_.meshDebrisLifeTime;
    j["meshDebrisScale"] = settings_.meshDebrisScale;
    j["suicideMeshDebrisScale"] = settings_.suicideMeshDebrisScale;
    j["meshDebrisColor"] = { settings_.meshDebrisColor.x, settings_.meshDebrisColor.y, settings_.meshDebrisColor.z, settings_.meshDebrisColor.w };
    j["flashEnabled"] = settings_.flashEnabled;
    j["flashDuration"] = settings_.flashDuration;
    j["flashNormalRadiusRate"] = settings_.flashNormalRadiusRate;
    j["flashSuicideRadiusRate"] = settings_.flashSuicideRadiusRate;
    j["flashMaxScale"] = settings_.flashMaxScale;
    j["flashAlpha"] = settings_.flashAlpha;
    j["flashColor"] = { settings_.flashColor.x, settings_.flashColor.y, settings_.flashColor.z, settings_.flashColor.w };
    j["shockwaveEnabled"] = settings_.shockwaveEnabled;
    j["shockwaveDuration"] = settings_.shockwaveDuration;
    j["shockwaveStartRadiusRate"] = settings_.shockwaveStartRadiusRate;
    j["shockwaveEndRadiusRate"] = settings_.shockwaveEndRadiusRate;
    j["shockwaveHeightOffset"] = settings_.shockwaveHeightOffset;
    j["shockwaveAlpha"] = settings_.shockwaveAlpha;
    j["shockwaveColor"] = { settings_.shockwaveColor.x, settings_.shockwaveColor.y, settings_.shockwaveColor.z, settings_.shockwaveColor.w };
    j["smokeEnabled"] = settings_.smokeEnabled;
    j["smokeScale"] = settings_.smokeScale;
    j["smokeLifeTime"] = settings_.smokeLifeTime;
    j["smokeUpPower"] = settings_.smokeUpPower;
    j["smokeHorizontalPower"] = settings_.smokeHorizontalPower;
    j["smokeSpreadRadius"] = settings_.smokeSpreadRadius;
    j["smokeColor"] = { settings_.smokeColor.x, settings_.smokeColor.y, settings_.smokeColor.z, settings_.smokeColor.w };
    j["normalExplosionScale"] = settings_.normalExplosionScale;
    j["suicideExplosionScale"] = settings_.suicideExplosionScale;
    j["effectPositionMinY"] = settings_.effectPositionMinY;
    j["effectHeightOffset"] = settings_.effectHeightOffset;
    j["meshDebrisMaxScale"] = settings_.meshDebrisMaxScale;
    j["enableProductionExplosionEffect"] = settings_.enableProductionExplosionEffect;
    j["showExplosionRadius"] = settings_.showExplosionRadius;
    j["showSuicideRadius"] = settings_.showSuicideRadius;
}

void MidRangeBombEffectController::ResetToDefault()
{
    // 追加: 爆弾エフェクト設定をデフォルトへ戻す。
    settings_ = BombEffectSettings{};
    ValidateSettings();
}

const MidRangeBombEffectController::BombEffectSettings& MidRangeBombEffectController::GetSettings() const
{
    return settings_;
}

void MidRangeBombEffectController::ApplyModelParticleTuning(float speed, float scale, float lifeTime)
{
    // 追加: メッシュ破片の速度・重力・寿命・サイズ・色を爆弾設定から反映する。
    const float safeSpeed = std::max(0.1f, speed);
    const float safeScale = std::min(std::max(0.01f, scale), settings_.meshDebrisMaxScale);
    const float safeLife = std::max(0.05f, lifeTime);
    meshDebrisEffect_.SetBurstTuning(safeSpeed, -std::abs(settings_.meshDebrisGravity), safeLife * 0.6f, safeLife, safeScale);
    meshDebrisEffect_.SetParticleColor(settings_.meshDebrisColor);
}

void MidRangeBombEffectController::ValidateSettings()
{
    // 追加: 旧JSONの危険値を安全範囲に収めて巨大化とチラつきを防ぐ。
    settings_.flashDuration = std::clamp(settings_.flashDuration, 0.02f, 0.5f);
    settings_.flashNormalRadiusRate = std::clamp(settings_.flashNormalRadiusRate, 0.05f, 2.0f);
    settings_.flashSuicideRadiusRate = std::clamp(settings_.flashSuicideRadiusRate, 0.05f, 2.0f);
    settings_.flashMaxScale = std::clamp(settings_.flashMaxScale, 0.1f, 6.0f);
    settings_.shockwaveEndRadiusRate = std::clamp(settings_.shockwaveEndRadiusRate, 0.1f, 2.0f);
    settings_.smokeScale = std::clamp(settings_.smokeScale, 0.05f, 2.0f);
    settings_.meshDebrisScale = std::clamp(settings_.meshDebrisScale, 0.02f, 0.6f);
    settings_.suicideMeshDebrisScale = std::clamp(settings_.suicideMeshDebrisScale, 0.02f, 0.8f);
    settings_.normalExplosionScale = std::clamp(settings_.normalExplosionScale, 0.1f, 2.5f);
    settings_.suicideExplosionScale = std::clamp(settings_.suicideExplosionScale, 0.1f, 3.5f);
    settings_.explosionParticleCount = std::clamp(settings_.explosionParticleCount, 1.0f, 500.0f);
    settings_.suicideExplosionParticleCount = std::clamp(settings_.suicideExplosionParticleCount, 1.0f, 800.0f);
}

void MidRangeBombEffectController::ApplySafeExplosionDefaults()
{
    // 追加: 巨大化しない安全な爆発演出プリセットを適用する。
    settings_.flashDuration = 0.12f;
    settings_.flashNormalRadiusRate = 0.35f;
    settings_.flashSuicideRadiusRate = 0.65f;
    settings_.flashMaxScale = 4.0f;
    settings_.flashAlpha = 0.8f;
    settings_.shockwaveDuration = 0.25f;
    settings_.shockwaveEndRadiusRate = 1.0f;
    settings_.shockwaveHeightOffset = 0.08f;
    settings_.shockwaveAlpha = 0.55f;
    settings_.smokeScale = 0.6f;
    settings_.smokeLifeTime = 0.7f;
    settings_.smokeUpPower = 1.5f;
    settings_.smokeSpreadRadius = 0.6f;
    settings_.smokeHorizontalPower = 0.4f;
    settings_.meshDebrisCount = 16;
    settings_.suicideMeshDebrisCount = 32;
    settings_.meshDebrisScale = 0.2f;
    settings_.suicideMeshDebrisScale = 0.3f;
    settings_.meshDebrisLifeTime = 0.9f;
    settings_.meshDebrisMaxScale = 0.45f;
    ValidateSettings();
}


K4E::Vector3 MidRangeBombEffectController::SanitizeEffectPosition(const Vector3& position) const
{
    // 追加: 爆発演出位置を地面下に入れないよう統一補正する。
    Vector3 adjusted = position;
    if (adjusted.y < settings_.effectPositionMinY)
    {
        adjusted.y = settings_.effectPositionMinY;
    }
    adjusted.y += settings_.effectHeightOffset;
    return adjusted;
}
uint32_t MidRangeBombEffectController::GetActiveDebrisCount() const
{
    return meshDebrisEffect_.GetActiveCount();
}
