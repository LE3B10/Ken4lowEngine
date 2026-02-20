#pragma once
#include <algorithm>

/// -------------------------------------------------------------
///				プレイヤーの被弾時エフェクトクラス
/// -------------------------------------------------------------
class PlayerVfx
{
public: /// ---------- メンバ関数 ---------- ///

	// 被弾演出
	void OnDamaged(float damage, float maxHp);

	// 更新処理
	void Update(float deltaTime);

	// 状態リセット
	void Reset();

public: /// ---------- チューニング ---------- ///

	void SetDamageDuration(float sec) { damagePostDuration_ = sec; }
	float GetDamageDuration() const { return damagePostDuration_; }

private: /// ---------- メンバ変数 ---------- ///

	// ---- 被弾ポストエフェクト ----
	float damagePostTimer_ = 0.0f;			// 演出の残り時間（0のときは非表示）
	float damagePostDuration_ = 0.18f;		// 秒
	float damagePostStrength_ = 0.0f;		// 0..1（大きいほど強い）
	bool  damagePostCapturedBase_ = false;	// エフェクトの「元の値」を保存したかどうか
	float baseVignettePower_ = 0.8f;
	float baseVignetteRange_ = 0.5f;
	float baseRadialBlurStrength_ = 0.3f;
	float baseRadialBlurSamples_ = 16.0f;
};

