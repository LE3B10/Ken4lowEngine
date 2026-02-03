#pragma once
#include <Sprite.h>

#include <vector>
#include <string>
#include <memory>
#include <chrono>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// 				　		HUDマネージャー
/// -------------------------------------------------------------
class HUDManager
{
private: /// ---------- 構造体 ---------- ///

	// Hotbar(武器スロット) の矩形（左上基準）
	struct RectF
	{
		float x = 0.0f;
		float y = 0.0f;
		float w = 0.0f;
		float h = 0.0f;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

public: /// ---------- セッター ---------- ///

	// HP受け取り
	void SetPlayerHp(float hp, float maxHp) { hp_ = hp; maxHp_ = maxHp; }

	// 武器スロットのバー矩形
	void SetHotbarRect(const RectF& rect) { hotbarRect_ = rect; hasHotbarRect_ = true; }

	// 武器スロットのバー矩形クリア
	void ClearHotbarRect() { hasHotbarRect_ = false; }

private: /// ---------- メンバ関数 ---------- ///

	// スプライト初期化
	void InitializeSprites();

	// スプライト位置更新
	void UpdateSprites(float screenW, float screenH, float dt);

	// グリッドアイコン位置更新
	void UpdateGridPositions(float screenW, float screenH);

	// ハートHP 更新
	void UpdateHearts(float screenW, float screenH, float dt);

	// スプライト描画処理
	void DrawSprites();

private: /// ---------- メンバ変数 ---------- ///

	// --- 上段 ---
	std::unique_ptr<K4E::Sprite> reload_icon_;
	std::unique_ptr<K4E::Sprite> ammo_icon_;
	std::unique_ptr<K4E::Sprite> reticle_grid_icon_;

	// --- 下段 ---
	std::unique_ptr<K4E::Sprite> r_key_icon_;
	std::unique_ptr<K4E::Sprite> mouse_left_icon_;
	std::unique_ptr<K4E::Sprite> mouse_right_icon_;

private: /// ---------- メンバ変数 ---------- ///

	// ハートHP
	struct HeartSlot
	{
		std::unique_ptr<K4E::Sprite> spr;
		std::string currentPath; // いま貼ってるテクスチャ
	};
	std::vector<HeartSlot> hearts_;

	float hp_ = 0.0f;
	float maxHp_ = 0.0f;

	RectF hotbarRect_{};
	bool  hasHotbarRect_ = false;

private: /// ---------- シェイク ---------- ///

	float prevHp_ = 0.0f;

	float shakeTimer_ = 0.0f;     // 経過
	float shakeDuration_ = 0.0f;  // 持続
	float shakeAmp_ = 0.0f;       // 最大振幅
	float shakeFreq_ = 20.0f;     // 周波数(Hzっぽい)
	float shakePhase_ = 0.0f;     // 位相
	float shakeOffsetX_ = 0.0f;
	float shakeOffsetY_ = 0.0f;

private: /// ---------- 時間 ---------- ///

	std::chrono::steady_clock::time_point lastTick_{};

private: /// ---------- レイアウト ---------- ///

	// 0: Center(デバッグ用) / 1: RightBottom 2x3(本命)
	struct LayoutParams
	{
		int layoutMode = 1;
		bool showGrid = true;

		float gridIconSize = 64.0f;
		float centerReticleSize = 64.0f;

		float marginX = 24.0f;
		float marginY = 24.0f;
		float gapX = 10.0f;
		float gapY = 10.0f;

		float centerOffsetX = 0.0f;
		float centerOffsetY = 0.0f;

		// ハートHP
		bool  showHearts = true;
		float heartSize = 32.0f;
		float heartGapX = 6.0f;
		float heartGapY = 6.0f;
		int   heartsPerRow = 10;
		float heartOffsetX = -48.0f;
		float heartOffsetY = 32.0f;

		float hpPerHeart = 10.0f;

		// WeaponSlotからRectが来ない場合のフォールバック推定
		float estSlotCount = 6.0f;
		float estSlotSize = 110.0f;
		float estSlotGap = 10.0f;
		float estHotbarMarginBottom = 24.0f;

		// ダメージ時シェイク（瞬間）
		float shakeBaseAmp = 2.5f;
		float shakeBaseDuration = 0.12f;
		float shakeFreq = 26.0f;
		float shakeLowHpAmpMul = 2.5f;
		float shakeLowHpDurMul = 1.5f;
		float shakeDamageAmpMul = 1.8f;

		// 低HP常時シェイク（継続）
		int   lowHpAlwaysShakeHearts = 4; // 4個以下なら継続
		float lowHpAlwaysAmp = 1.0f;      // 継続の振幅
		float lowHpAlwaysFreq = 12.0f;    // 継続の周波数

		// 低HP脈動（拡縮）
		float lowHpPulseBase = 0.05f;     // 基本拡縮率(5%)
		float lowHpPulseMul = 0.12f;      // 瀕死で追加拡縮(最大+12%くらい)
		float lowHpPulseFreq = 2.2f;      // 脈動の速さ
	};

	LayoutParams layout_{};
	float lastScreenW_ = 0.0f;
	float lastScreenH_ = 0.0f;
};

