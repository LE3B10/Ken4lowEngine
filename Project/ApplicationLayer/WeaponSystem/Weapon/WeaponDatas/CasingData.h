#pragma once
#include <Vector3.h>
#include <Vector4.h>

/// ---------- 薬莢エフェクト設定 ---------- ///
struct CasingData
{
	bool  enabled = true;	   // 薬莢エフェクト有効
	float offsetRight = 0.10f; // 銃口基準のローカルオフセット（右）
	float offsetUp = 0.06f;	   // 銃口基準のローカルオフセット（上）
	float offsetBack = 0.03f;  // 銃口基準のローカルオフセット（後ろ）
	float speedMin = 1.8f;	   // m/s : 初速最小
	float speedMax = 3.2f;	   // m/s : 初速最大
	float coneDeg = 20.0f;	   // 度 : 右方向を中心にした円錐拡がり
	float gravityY = -9.8f;	   // m/s^2 : 自然落下
	float drag = 0.2f;		   // 空気抵抗（簡易）
	float life = 1.5f;		   // 秒 : 寿命
	float upKick = 1.2f;	   // [m/s] : 真上方向への瞬間的な“キック”
	float upBias = 0.25f;	   // 方向ベクトルを上向きに寄せるブレンド(0..1)
	float spinMin = 12.0f;	   // rad/s : 回転速度最小
	float spinMax = 28.0f;	   // rad/s : 回転速度最大


	Vector4 color = { 0.9f,0.8f,0.3f,1.0f };  // 真鍮色っぽく
	Vector3 scale = { 0.016f,0.016f,0.048f }; // ピストル用に小さめ
};