#pragma once
#include <cmath>
#include <numbers>
#include <algorithm>
#include "Vector4.h"

/// -------------------------------------------------------------
///						線形補間を行う関数
/// -------------------------------------------------------------

/// ---------- 線形補間を行う関数 ---------- ///
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vector4 Lerp(const Vector4& a, const Vector4& b, float t)
{
	return {
		Lerp(a.x, b.x, t),
		Lerp(a.y, b.y, t),
		Lerp(a.z, b.z, t),
		Lerp(a.w, b.w, t)
	};
}

/// ---------- 値を0〜1にクランプする関数 ---------- ///
inline float Saturate(float x) { return std::clamp(x, 0.0f, 1.0f); }

/// ---------- スムースステップ関数 ---------- ///
inline float Smoothstep01(float x) { x = Saturate(x);	return x * x * (3.0f - 2.0f * x); }

/// ---------- 角度を正規化する関数 ---------- ///
inline float NormalizeAngle(float angle)
{
	// -π 〜 +π に収める
	angle = std::fmod(angle + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
	if (angle < 0) angle += 2.0f * std::numbers::pi_v<float>;
	return angle - std::numbers::pi_v<float>;
}

/// ---------- 角度の線形補間を行う関数 ---------- ///
inline float LerpAngle(float a, float b, float t)
{
	a = NormalizeAngle(a);
	b = NormalizeAngle(b);

	float diff = b - a;

	// 最短方向へ補間（例: -179° → +179°）
	if (diff > std::numbers::pi_v<float>)
		diff -= 2.0f * std::numbers::pi_v<float>;
	else if (diff < -std::numbers::pi_v<float>)
		diff += 2.0f * std::numbers::pi_v<float>;

	return a + diff * t;
}

/// ---------- イージング関数 ---------- ///
inline float EaseInSine(float t) { return 1.0f - cosf(t * (std::numbers::pi_v<float> / 2.0f)); }

/// ---------- イーズアウト関数 ---------- ///
inline float EaseOutSine(float t) { return sinf(t * (std::numbers::pi_v<float> / 2.0f)); }

/// ---------- イーズインアウト関数 ---------- ///
inline float EaseInOutSine(float t) { return (cosf(std::numbers::pi_v<float> *t) - 1.0f) / 2.0f; }

/// ---------- イーズクアッド関数 ---------- ///
inline float EaseInQuad(float t) { return t * t; }

/// ---------- イーズアウトクアッド関数 ---------- ///
inline float EaseOutQuad(float t) { return 1.0f - pow(1.0f - t, 2.0f); }

/// ---------- イーズインアウトクアッド関数 ---------- ///
inline float EaseInOutQuad(float t) { return (t < 0.5f) ? 2.0f * t * t : pow(-2.0f * t + 2.0f, 2.0f) / 2.0f; }

/// ---------- イーズインキュービック関数 ---------- ///
inline float EaseInCubic(float t) { return t * t * t; }

/// ---------- イーズアウトキュービック関数 ---------- ///
inline float EaseOutCubic(float t) { return 1.0f - pow(1.0f - t, 3.0f); }

/// ---------- イーズインアウトキュービック関数 ---------- ///
inline float EaseInOutCubic(float t) { return (t < 0.5f) ? 4.0f * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 3.0f) / 2.0f; }

/// ---------- イーズインクォート関数 ---------- ///
inline float EaseInQuart(float t) { return t * t * t * t; }

/// ---------- イーズアウトクォート関数 ---------- ///
inline float EaseOutQuart(float t) { return 1.0f - pow(1.0f - t, 4.0f); }

/// ---------- イーズインアウトクォート関数 ---------- ///
inline float EaseInOutQuart(float t) { return (t < 0.5f) ? 8.0f * t * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 4.0f) / 2.0f; }

/// ---------- イーズインクインテック関数 ---------- ///
inline float EaseInQuint(float t) { return t * t * t * t * t; }

/// ---------- イーズアウトクインテック関数 ---------- ///
inline float EaseOutQuint(float t) { return 1.0f - pow(1.0f - t, 5.0f); }

/// ---------- イーズインアウトクインテック関数 ---------- ///
inline float EaseInOutQuint(float t) { return (t < 0.5f) ? 16.0f * t * t * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 5.0f) / 2.0f; }

/// ---------- イーズインエクスポネンシャル関数 ---------- ///
inline float EaseInExpo(float t) { return (t == 0.0f) ? 0.0f : pow(2.0f, 10.0f * t - 10.0f); }

/// ---------- イーズアウトエクスポネンシャル関数 ---------- ///
inline float EaseOutExpo(float t) { return (t == 1.0f) ? 1.0f : 1.0f - pow(2.0f, -10.0f * t); }

/// ---------- イーズインアウトエクスポネンシャル関数 ---------- ///
inline float EaseInOutExpo(float t)
{
	if (t == 0.0f) return 0.0f;
	if (t == 1.0f) return 1.0f;
	return (t < 0.5f) ? pow(2.0f, 20.0f * t - 10.0f) / 2.0f : (2.0f - pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
}

/// ---------- イーズインカーシアン関数 ---------- ///
inline float EaseInCirc(float t) { return 1.0f - sqrt(1.0f - pow(t, 2.0f)); }

/// ---------- イーズアウトカーシアン関数 ---------- ///
inline float EaseOutCirc(float t) { return sqrt(1.0f - pow(t - 1.0f, 2.0f)); }

/// ---------- イーズインアウトカーシアン関数 ---------- ///
inline float EaseInOutCirc(float t) { return (t < 0.5f) ? (1.0f - sqrt(1.0f - pow(2.0f * t, 2.0f))) / 2.0f : (sqrt(1.0f - pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f; }

/// ---------- イーズインバック関数 ---------- ///
inline float EaseInBack(float t, float s = 1.70158f) { return t * t * ((s + 1.0f) * t - s); }

/// ---------- イーズアウトバック関数 ---------- ///
inline float EaseOutBack(float t, float s = 1.70158f) { return 1.0f + t * t * ((s + 1.0f) * t + s); }

/// ---------- イーズインアウトバック関数 ---------- ///
inline float EaseInOutBack(float t, float s = 1.70158f) { return (t < 0.5f) ? (2.0f * t * t * ((s + 1.0f) * 2.0f * t - s)) / 2.0f : (1.0f + 2.0f * t * t * ((s + 1.0f) * 2.0f * t + s)) / 2.0f; }

/// ---------- イーズインエラスティック関数 ---------- ///
inline float EaseInElastic(float t, float a = 1.0f, float p = 0.3f)
{
	if (t == 0.0f) return 0.0f;
	if (t == 1.0f) return 1.0f;
	float s = p / (2.0f * std::numbers::pi_v<float>) * asin(1.0f / a);
	return -(a * pow(2.0f, 10.0f * t - 10.0f) * sin((t * 10.0f - s) * (2.0f * std::numbers::pi_v<float>) / p));
}

/// ---------- イーズアウトエラスティック関数 ---------- ///
inline float EaseOutElastic(float t, float a = 1.0f, float p = 0.3f)
{
	if (t == 0.0f) return 0.0f;
	if (t == 1.0f) return 1.0f;
	float s = p / (2.0f * std::numbers::pi_v<float>) * asin(1.0f / a);
	return a * pow(2.0f, -10.0f * t) * sin((t * 10.0f - s) * (2.0f * std::numbers::pi_v<float>) / p) + 1.0f;
}

/// ---------- イーズインアウトエラスティック関数 ---------- ///
inline float EaseInOutElastic(float t, float a = 1.0f, float p = 0.3f)
{
	if (t == 0.0f) return 0.0f;
	if (t == 1.0f) return 1.0f;
	float s = p / (2.0f * std::numbers::pi_v<float>) * asin(1.0f / a);
	if (t < 0.5f)
	{
		return -(a * pow(2.0f, 20.0f * t - 10.0f) * sin((20.0f * t - s) * (2.0f * std::numbers::pi_v<float>) / p)) / 2.0f;
	}
	else
	{
		return a * pow(2.0f, -20.0f * t + 10.0f) * sin((20.0f * t - s) * (2.0f * std::numbers::pi_v<float>) / p) / 2.0f + 1.0f;
	}
}

/// ---------- イーズアウトボウンス関数 ---------- ///
inline float EaseOutBounce(float t)
{
	if (t < (1.0f / 2.75f))
	{
		return 7.5625f * t * t;
	}
	else if (t < (2.0f / 2.75f))
	{
		t -= (1.5f / 2.75f);
		return 7.5625f * t * t + 0.75f;
	}
	else if (t < (2.5f / 2.75f))
	{
		t -= (2.25f / 2.75f);
		return 7.5625f * t * t + 0.9375f;
	}
	else
	{
		t -= (2.625f / 2.75f);
		return 7.5625f * t * t + 0.984375f;
	}
}

/// ---------- イーズインボウンス関数 ---------- ///
inline float EaseInBounce(float t) { return 1.0f - EaseOutBounce(1.0f - t); }

/// ---------- イーズインアウトボウンス関数 ---------- ///
inline float EaseInOutBounce(float t)
{
	if (t < 0.5f)
	{
		return EaseInBounce(t * 2.0f) / 2.0f;
	}
	else
	{
		return EaseOutBounce(t * 2.0f - 1.0f) / 2.0f + 0.5f;
	}
}
