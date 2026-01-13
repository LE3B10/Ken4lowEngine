#pragma once
#include <Sprite.h>
#include <NumberSpriteDrawer.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class WeaponManager;

/// -------------------------------------------------------------
///				　		武器スロットクラス
/// -------------------------------------------------------------
class WeaponSlot
{
public: /// ---------- メンバ関数 ---------- ///

	// スロット数定数
	static constexpr int kSlotCount = 6;

	// レイアウト設定構造体
	struct Layout
	{
		float slotSize = 128.0f; // 1枠のサイズ（正方形想定）
		float spacing = 8.0f;  // 枠間
		float marginBottom = 32.0f; // 画面下からの余白
	};

	// 弾薬情報構造体
	struct WeaponAmmoInfo
	{
		int currentAmmo = 0;
		int reserveAmmo = 0;
	};

public: /// ---------- メンバ関数 ---------- ///

	// frameTex/selectedTex は TextureManager 等で読み込んだハンドルを渡す想定
	void Initialize(const std::string& frameTex, const std::string& selectedTex, const Layout& layout = {});

	/// <summary>
	/// スロット番号表示用の数字スプライト初期化。
	/// </summary>
	void InitializeSlotNumbers(const std::string& numberTex,
		float srcDigitWidth = 50.0f, float srcDigitHeight = 50.0f,
		const Vector2& offset = { 8.0f, 8.0f },
		float spacing = 24.0f,
		float drawDigitWidth = -1.0f, float drawDigitHeight = -1.0f);

	/// <summary>
	/// 弾薬表示（残弾/予備弾）用の数字スプライト初期化。
	/// ※まずは Primary(スロット0)だけ表示する前提。
	/// </summary>
	void InitializeAmmoNumbers(const std::string& numberTex,
		float srcDigitW = 50.0f, float srcDigitH = 50.0f,
		const Vector2& padding = { 10.0f, 10.0f },
		float spacing = 2.0f,
		float drawDigitW = -1.0f, float drawDigitH = -1.0f);

	// 弾薬区切り文字スプライト初期化
	void InitializeAmmoDelimiter(const std::string& slashTex,
		const Vector2& size = { -1.0f, -1.0f },
		const Vector2& offset = { 0.0f, 0.0f });

	// 更新処理
	void Update(const WeaponManager& weaponManager);

	// 描画処理
	void Draw();

	// スロットアイコン（スロット0..5に対応して6枚渡す）
	void InitializeIcons(const std::array<std::string, kSlotCount>& iconTex);

	// 全スロット同じアイコンを使う場合（従来互換）
	void InitializeIcons(const std::string& iconTex);

private: /// ---------- メンバ関数 ---------- ///

	// レイアウト再構築処理
	void RebuildLayout();

private: /// ---------- メンバ変数 ---------- ///

	Layout layout_{};

	int selectedIndex_ = -1;

	std::string frameTex_;
	std::string selectedTex_;

	// 通常枠 / 選択枠（まずは枠だけ描画するので2枚持ちが安全）
	std::array<std::unique_ptr<Sprite>, kSlotCount> frame_;
	std::array<std::unique_ptr<Sprite>, kSlotCount> frameSelected_;

	// スロット番号表示用
	bool drawSlotNumbers_ = false;
	NumberSpriteDrawer numberDrawer_;
	Vector2 numberOffset_{ 8.0f, 8.0f };
	float numberSpacing_ = 24.0f;

	// --- 弾薬(残弾/予備弾)表示 ---
	bool drawAmmo_ = false;
	NumberSpriteDrawer ammoDrawer_;
	Vector2 ammoPadding_{ 10.0f, 10.0f };
	float ammoSpacing_ = 2.0f;
	float ammoDigitW_ = 24.0f;
	float ammoDigitH_ = 24.0f;

	// アイコン用スプライト配列
	std::array<std::unique_ptr<Sprite>, kSlotCount> icon_;

	// 弾薬情報配列（スロット0..5に対応して6個）
	std::vector<WeaponAmmoInfo> ammoInfos_;
	std::array<bool, kSlotCount> ammoUses_{};

	float numberDigitW_ = 50.0f;
	float numberDigitH_ = 50.0f;

	std::array<std::unique_ptr<Sprite>, kSlotCount> ammoSlash_;
	Vector2 ammoSlashSize_{ 0.0f, 0.0f };
	Vector2 ammoSlashOffset_{ 0.0f, 0.0f };
};

