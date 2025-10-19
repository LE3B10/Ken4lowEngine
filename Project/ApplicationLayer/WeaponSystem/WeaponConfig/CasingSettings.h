#pragma once
#include <Vector3.h>
#include <Vector4.h>

// 薬莢エフェクト設定（将来用）
struct CasingSettings
{
	bool  enabled = true;
	// 銃口基準のローカルオフセット（右・上・後ろ）
	float offsetRight = 0.04f;
	float offsetUp = 0.06f;
	float offsetBack = 0.02f;

	// 速度と拡がり
	float speedMin = 1.5f;      // m/s
	float speedMax = 3.0f;      // m/s
	float coneDeg = 18.0f;     // 右方向を中心にした円錐拡がり

	// 自然落下
	float gravityY = -9.8f;     // m/s^2
	float life = 2.0f;      // 秒（消えるまで）
	float drag = 0.2f;      // 空気抵抗（簡易）
	float upKick = 1.2f;      // 真上方向への瞬間的な“キック”[m/s]
	float upBias = 0.25f;     // 方向ベクトルを上向きに寄せるブレンド(0..1)
	float spinMin = 12.0f;     // rad/s
	float spinMax = 28.0f;     // rad/s
	Vector4 color = { 0.9f, 0.8f, 0.3f, 1.0f }; // 真鍮色っぽい
	Vector3 scale = { 0.01f, 0.01f, 0.03f };    // 見た目スケール
};