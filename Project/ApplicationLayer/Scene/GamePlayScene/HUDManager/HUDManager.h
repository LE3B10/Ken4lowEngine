#pragma once
#include <Sprite.h>

#include <memory>

/// -------------------------------------------------------------
/// 				　		HUDマネージャー
/// -------------------------------------------------------------
class HUDManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

private: /// ---------- メンバ関数 ---------- ///

	// スプライト初期化
	void InitializeSprites();

	// スプライト位置更新
	void UpdateSprites(float screenW, float screenH);

	// グリッドアイコン位置更新
	void UpdateGridPositions(float screenW, float screenH);

	// スプライト描画処理
	void DrawSprites();

private: /// ---------- メンバ変数 ---------- ///

	// --- 上段 ---
	std::unique_ptr<Sprite> reload_icon_;
	std::unique_ptr<Sprite> ammo_icon_;
	std::unique_ptr<Sprite> reticle_grid_icon_;

	// --- 下段 ---
	std::unique_ptr<Sprite> r_key_icon_;
	std::unique_ptr<Sprite> mouse_left_icon_;
	std::unique_ptr<Sprite> mouse_right_icon_;

private: /// ---------- メンバ変数 ---------- ///

	// 0: Center(デバッグ用) / 1: RightBottom 2x3(本命)
	struct LayoutParams
	{
		int layoutMode = 1;

		bool showGrid = true;          // 右下 2x3

		float gridIconSize = 64.0f;    // グリッドのアイコンサイズ（正方形）
		float centerReticleSize = 64.0f;

		// RightBottom 2x3 用
		float marginX = 24.0f; // 右端からの余白
		float marginY = 24.0f; // 下端からの余白
		float gapX = 10.0f;    // 列間
		float gapY = 10.0f;    // 行間

		// Center(デバッグ用) の微調整
		float centerOffsetX = 0.0f;
		float centerOffsetY = 0.0f;
	};

	LayoutParams layout_{};
	float lastScreenW_ = 0.0f;
	float lastScreenH_ = 0.0f;
};

