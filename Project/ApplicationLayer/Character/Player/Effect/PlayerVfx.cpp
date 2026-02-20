#define NOMINMAX
#include "PlayerVfx.h"

#include "PostEffectManager.h"
#include "VignetteEffect.h"
#include "RadialBlurEffect.h"

#include <cmath>

using namespace Ken4lowEngine;

void PlayerVfx::OnDamaged(float damage, float maxHp)
{
	// ダメージ量から 0..1 の強さへ（HP100基準で 5〜25ダメージくらいが気持ちいい）
	const float safeMaxHp = std::max(maxHp, 1.0f); // maxHpが0の可能性はないと思うけど念のため
	const float s = std::clamp((damage / safeMaxHp) * 4.0f, 0.15f, 1.0f);

	// 連続被弾は「上書き＋強さは最大」を採用
	damagePostTimer_ = damagePostDuration_;
	damagePostStrength_ = std::max(damagePostStrength_, s);
}

void PlayerVfx::Update(float deltaTime)
{
	auto* pem = PostEffectManager::GetInstance();
	if (!pem) return;

	// エフェクト取得（無ければ何もしない）
	auto* vig = dynamic_cast<VignetteEffect*>(pem->GetEffect("VignetteEffect"));
	auto* blur = dynamic_cast<RadialBlurEffect*>(pem->GetEffect("RadialBlurEffect"));
	if (!vig || !blur) return;

	// 初回だけ「元の値」を保存（終わったら戻すため）
	if (!damagePostCapturedBase_)
	{
		baseVignettePower_ = vig->GetPower();
		baseVignetteRange_ = vig->GetRange();
		baseRadialBlurStrength_ = blur->GetBlurStrength();
		baseRadialBlurSamples_ = blur->GetSampleCount();
		damagePostCapturedBase_ = true;
	}

	if (damagePostTimer_ > 0.0f)
	{
		damagePostTimer_ -= deltaTime;
		if (damagePostTimer_ < 0.0f) damagePostTimer_ = 0.0f;

		// 1 -> 0 で減衰（開始が一番強い）
		const float t = (damagePostDuration_ > 0.0f) ? (damagePostTimer_ / damagePostDuration_) : 0.0f;
		const float ease = t * t; // smooth fade-out
		const float amp = ease * damagePostStrength_;

		// 被弾中だけ有効化
		pem->EnableEffect("VignetteEffect");
		pem->EnableEffect("RadialBlurEffect");

		// ---- パラメータ（好みに合わせて調整OK） ----
		const float maxVignettePower = 2.0f;  // 0.8 -> 2.0 くらい
		const float maxVignetteRange = 0.70f; // 0.5 -> 0.7 くらい
		const float maxBlurStrength = 0.65f; // 0.3 -> 0.65 くらい
		const float blurSamples = 16.0f;

		vig->SetPower(baseVignettePower_ + (maxVignettePower - baseVignettePower_) * amp);
		vig->SetRange(baseVignetteRange_ + (maxVignetteRange - baseVignetteRange_) * amp);

		blur->SetCenter(Vector2(0.5f, 0.5f));
		blur->SetSampleCount(blurSamples);
		blur->SetBlurStrength(baseRadialBlurStrength_ + (maxBlurStrength - baseRadialBlurStrength_) * amp);
	}
	else
	{
		// 終了：元の値へ戻す
		vig->SetPower(baseVignettePower_);
		vig->SetRange(baseVignetteRange_);
		blur->SetSampleCount(baseRadialBlurSamples_);
		blur->SetBlurStrength(baseRadialBlurStrength_);

		// プログラム側フラグだけOFF（ImGuiでONならそのまま残る）
		pem->DisableEffect("VignetteEffect");
		pem->DisableEffect("RadialBlurEffect");

		damagePostStrength_ = 0.0f;
	}
}

void PlayerVfx::Reset()
{
	damagePostTimer_ = 0.0f;
	damagePostStrength_ = 0.0f;
	damagePostCapturedBase_ = false;
}
