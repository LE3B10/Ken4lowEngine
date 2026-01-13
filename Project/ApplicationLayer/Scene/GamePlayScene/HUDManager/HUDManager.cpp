#define NOMINMAX
#include "HUDManager.h"
#include "DirectXCommon.h"

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI

#include <algorithm>

namespace
{
	// テクスチャパス
	constexpr const char* kPathReload = "icon/reload_icon.png";
	constexpr const char* kPathAmmo = "icon/ammo_icon.png";
	constexpr const char* kPathReticle = "icon/reticle_icon.png";
	constexpr const char* kPathRKey = "icon/R_key_icon.png";
	constexpr const char* kPathMouseL = "icon/mouse_leftClick.png";
	constexpr const char* kPathMouseR = "icon/mouse_rightClick.png";

	// ハート
	constexpr const char* kPathHeartFull = "icon/heart_full.png";
	constexpr const char* kPathHeartHalf = "icon/heart_half.png";
	constexpr const char* kPathHeartDeath = "icon/heart_death.png";

	constexpr float kPi = 3.14159265358979323846f;

	inline float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
	inline float EaseOutQuad(float t) { t = Clamp01(t); return 1.0f - (1.0f - t) * (1.0f - t); }
}

/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void HUDManager::Initialize()
{
	InitializeSprites();

	lastTick_ = std::chrono::steady_clock::now();
	prevHp_ = hp_;
}

/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void HUDManager::Update()
{
	auto* dxCommon = DirectXCommon::GetInstance();
	const float screenW = static_cast<float>(dxCommon->GetClientWidth());
	const float screenH = static_cast<float>(dxCommon->GetClientHeight());

	lastScreenW_ = screenW;
	lastScreenH_ = screenH;

	// dt 計算（エンジン依存を避けて chrono）
	const auto now = std::chrono::steady_clock::now();
	float dt = std::chrono::duration<float>(now - lastTick_).count();
	lastTick_ = now;
	dt = std::clamp(dt, 0.0f, 1.0f / 15.0f); // 飛び防止

	UpdateSprites(screenW, screenH, dt);
}

/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void HUDManager::Draw()
{
	DrawSprites();
}

void HUDManager::DrawImGui()
{
#ifdef USE_IMGUI

	ImGui::Begin("HUDManager");

	ImGui::Text("Screen: %.0f x %.0f", lastScreenW_, lastScreenH_);
	ImGui::Separator();

	ImGui::Checkbox("Show Grid (2x3)", &layout_.showGrid);
	ImGui::Combo("Layout Mode", &layout_.layoutMode,
		"Center (Debug)\0RightBottom 2x3 (PixelGun-like)\0\0");

	ImGui::DragFloat("Grid Icon Size", &layout_.gridIconSize, 1.0f, 16.0f, 256.0f, "%.0f");

	ImGui::Separator();
	ImGui::Text("Hearts HP");
	ImGui::Checkbox("Show Hearts", &layout_.showHearts);
	ImGui::DragFloat("Heart Size", &layout_.heartSize, 1.0f, 8.0f, 128.0f, "%.0f");
	ImGui::DragFloat("Heart Gap X", &layout_.heartGapX, 0.5f, 0.0f, 64.0f, "%.1f");
	ImGui::DragFloat("Heart Gap Y", &layout_.heartGapY, 0.5f, 0.0f, 64.0f, "%.1f");
	ImGui::DragInt("Hearts Per Row", &layout_.heartsPerRow, 1, 1, 20);
	ImGui::DragFloat("Heart Offset X", &layout_.heartOffsetX, 1.0f, -300.0f, 300.0f, "%.0f");
	ImGui::DragFloat("Heart Offset Y", &layout_.heartOffsetY, 1.0f, 0.0f, 200.0f, "%.0f");
	ImGui::DragFloat("HP per Heart", &layout_.hpPerHeart, 0.5f, 1.0f, 200.0f, "%.1f");

	ImGui::Separator();
	ImGui::Text("Damage Shake (on hit)");
	ImGui::DragFloat("Shake Base Amp", &layout_.shakeBaseAmp, 0.1f, 0.0f, 30.0f, "%.1f");
	ImGui::DragFloat("Shake Base Duration", &layout_.shakeBaseDuration, 0.01f, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Shake Freq", &layout_.shakeFreq, 0.5f, 1.0f, 60.0f, "%.1f");
	ImGui::DragFloat("LowHP Amp Mul", &layout_.shakeLowHpAmpMul, 0.1f, 0.0f, 10.0f, "%.1f");
	ImGui::DragFloat("LowHP Dur Mul", &layout_.shakeLowHpDurMul, 0.1f, 0.0f, 10.0f, "%.1f");
	ImGui::DragFloat("Damage Amp Mul", &layout_.shakeDamageAmpMul, 0.1f, 0.0f, 10.0f, "%.1f");

	ImGui::Separator();
	ImGui::Text("LowHP Always (<= hearts)");
	ImGui::DragInt("LowHP Always Hearts", &layout_.lowHpAlwaysShakeHearts, 1, 1, 10);
	ImGui::DragFloat("LowHP Always Amp", &layout_.lowHpAlwaysAmp, 0.1f, 0.0f, 10.0f, "%.1f");
	ImGui::DragFloat("LowHP Always Freq", &layout_.lowHpAlwaysFreq, 0.5f, 1.0f, 30.0f, "%.1f");

	ImGui::Separator();
	ImGui::Text("LowHP Pulse (scale)");
	ImGui::DragFloat("Pulse Base", &layout_.lowHpPulseBase, 0.01f, 0.0f, 0.5f, "%.2f");
	ImGui::DragFloat("Pulse Mul", &layout_.lowHpPulseMul, 0.01f, 0.0f, 0.5f, "%.2f");
	ImGui::DragFloat("Pulse Freq", &layout_.lowHpPulseFreq, 0.1f, 0.1f, 10.0f, "%.1f");

	ImGui::Separator();
	ImGui::Text("HP: %.1f / %.1f", hp_, maxHp_);
	ImGui::Text("ShakeOffset: (%.2f, %.2f)", shakeOffsetX_, shakeOffsetY_);

	if (ImGui::Button("Reset"))
	{
		layout_ = LayoutParams{};
	}

	ImGui::End();

#endif // USE_IMGUI
}

void HUDManager::InitializeSprites()
{
	// --- 上段 ---
	reload_icon_ = std::make_unique<Sprite>();
	reload_icon_->Initialize(kPathReload);

	ammo_icon_ = std::make_unique<Sprite>();
	ammo_icon_->Initialize(kPathAmmo);

	reticle_grid_icon_ = std::make_unique<Sprite>();
	reticle_grid_icon_->Initialize(kPathReticle);

	// --- 下段 ---
	r_key_icon_ = std::make_unique<Sprite>();
	r_key_icon_->Initialize(kPathRKey);

	mouse_left_icon_ = std::make_unique<Sprite>();
	mouse_left_icon_->Initialize(kPathMouseL);

	mouse_right_icon_ = std::make_unique<Sprite>();
	mouse_right_icon_->Initialize(kPathMouseR);
}

void HUDManager::UpdateSprites(float screenW, float screenH, float dt)
{
	// サイズ即反映
	const float s = layout_.gridIconSize;
	auto setGridCommon = [&](std::unique_ptr<Sprite>& spr)
		{
			spr->SetSize({ s, s });
		};

	setGridCommon(reload_icon_);
	setGridCommon(ammo_icon_);
	setGridCommon(reticle_grid_icon_);
	setGridCommon(r_key_icon_);
	setGridCommon(mouse_left_icon_);
	setGridCommon(mouse_right_icon_);

	UpdateGridPositions(screenW, screenH);

	// ハートHP
	UpdateHearts(screenW, screenH, dt);
}

void HUDManager::UpdateGridPositions(float screenW, float screenH)
{
	// 2x3：上段 = reload/ammo/reticle、下段 = R/左/右
	const float s = layout_.gridIconSize;
	const float stepX = s + layout_.gapX;
	const float stepY = s + layout_.gapY;

	float xL = 0.0f, xM = 0.0f, xR = 0.0f;
	float yTop = 0.0f, yBottom = 0.0f;

	if (layout_.layoutMode == 1)
	{
		// RightBottom 2x3：Anchor=(1,1) で「右下座標」を指定
		const float xRight = screenW - layout_.marginX;
		const float xMid = xRight - stepX;
		const float xLeft = xRight - stepX * 2.0f;

		const float yB = screenH - layout_.marginY;
		const float yT = yB - stepY;

		xL = xLeft; xM = xMid; xR = xRight;
		yTop = yT; yBottom = yB;

		auto setRB = [&](std::unique_ptr<Sprite>& spr)
			{
				spr->SetAnchorPoint({ 1.0f, 1.0f });
			};
		setRB(reload_icon_);
		setRB(ammo_icon_);
		setRB(reticle_grid_icon_);
		setRB(r_key_icon_);
		setRB(mouse_left_icon_);
		setRB(mouse_right_icon_);
	}
	else
	{
		// Center 2x3(デバッグ)：Anchor=(0.5,0.5) で「中心座標」を指定
		const float cx = screenW * 0.5f + layout_.centerOffsetX;
		const float cy = screenH * 0.5f + layout_.centerOffsetY;

		const float gridW = s * 3.0f + layout_.gapX * 2.0f;
		const float gridH = s * 2.0f + layout_.gapY;

		// 各セルの中心座標
		const float startX = cx - gridW * 0.5f + s * 0.5f;
		const float startY = cy - gridH * 0.5f + s * 0.5f;

		xL = startX;
		xM = startX + stepX;
		xR = startX + stepX * 2.0f;

		yTop = startY;
		yBottom = startY + stepY;

		auto setC = [&](std::unique_ptr<Sprite>& spr)
			{
				spr->SetAnchorPoint({ 0.5f, 0.5f });
			};
		setC(reload_icon_);
		setC(ammo_icon_);
		setC(reticle_grid_icon_);
		setC(r_key_icon_);
		setC(mouse_left_icon_);
		setC(mouse_right_icon_);
	}

	// 上段（左→右）
	reload_icon_->SetPosition({ xL, yTop });
	ammo_icon_->SetPosition({ xM, yTop });
	reticle_grid_icon_->SetPosition({ xR, yTop });

	// 下段（左→右）
	r_key_icon_->SetPosition({ xL, yBottom });
	mouse_left_icon_->SetPosition({ xM, yBottom });
	mouse_right_icon_->SetPosition({ xR, yBottom });

	// Update
	reload_icon_->Update();
	ammo_icon_->Update();
	reticle_grid_icon_->Update();
	r_key_icon_->Update();
	mouse_left_icon_->Update();
	mouse_right_icon_->Update();
}

void HUDManager::UpdateHearts(float screenW, float screenH, float dt)
{
	if (!layout_.showHearts) return;
	if (maxHp_ <= 0.0f) return;

	const float hpPerHeart = std::max(1.0f, layout_.hpPerHeart);
	const float unitHp = hpPerHeart * 0.5f;

	// ハート数
	const int maxHearts = std::max(1, (int)std::ceil(maxHp_ / hpPerHeart));

	// hearts_確保
	if ((int)hearts_.size() != maxHearts)
	{
		hearts_.clear();
		hearts_.reserve(maxHearts);
		for (int i = 0; i < maxHearts; ++i)
		{
			HeartSlot hs;
			hs.spr = std::make_unique<Sprite>();
			hs.currentPath = "";
			hearts_.push_back(std::move(hs));
		}
	}

	// ---------------------------
	// 1) HP減少検知 → ダメージシェイク発火
	// ---------------------------
	const float eps = 0.0001f;
	if (hp_ < prevHp_ - eps)
	{
		const float dmg = std::max(0.0f, prevHp_ - hp_);
		const float hp01 = (maxHp_ > 0.0f) ? Clamp01(hp_ / maxHp_) : 0.0f;
		const float low = 1.0f - hp01; // 0(満タン)→1(瀕死)

		const float lowBoost = 1.0f + low * layout_.shakeLowHpAmpMul;
		const float dmgBoost = 1.0f + (dmg / std::max(1.0f, maxHp_)) * layout_.shakeDamageAmpMul;

		shakeAmp_ = layout_.shakeBaseAmp * lowBoost * dmgBoost;

		const float durBoost = 1.0f + low * layout_.shakeLowHpDurMul;
		shakeDuration_ = layout_.shakeBaseDuration * durBoost;

		shakeFreq_ = layout_.shakeFreq;
		shakeTimer_ = 0.0f;

		shakePhase_ += 1.2345f + dmg * 0.1f;
	}
	prevHp_ = hp_;

	// ---------------------------
	// 2) ダメージシェイクオフセット計算（減衰）
	// ---------------------------
	if (shakeTimer_ < shakeDuration_ && shakeDuration_ > 0.0f)
	{
		shakeTimer_ += dt;
		const float t01 = Clamp01(shakeTimer_ / shakeDuration_);
		const float envelope = 1.0f - EaseOutQuad(t01); // すぐ減衰

		const float ampNow = shakeAmp_ * envelope;
		const float w = 2.0f * kPi * shakeFreq_;
		const float time = shakeTimer_;

		shakeOffsetX_ = std::sin(w * time + shakePhase_) * ampNow;
		shakeOffsetY_ = std::cos(w * time * 1.37f + shakePhase_ * 0.73f) * ampNow;
	}
	else
	{
		shakeOffsetX_ = 0.0f;
		shakeOffsetY_ = 0.0f;
	}

	// ---------------------------
	// 3) Hotbar矩形（無ければ推定）
	// ---------------------------
	RectF bar = hotbarRect_;
	if (!hasHotbarRect_)
	{
		const float slot = layout_.estSlotSize;
		const float gap = layout_.estSlotGap;
		const float count = layout_.estSlotCount;

		bar.w = slot * count + gap * (count - 1.0f);
		bar.h = slot;
		bar.x = (screenW - bar.w) * 0.5f;
		bar.y = screenH - layout_.estHotbarMarginBottom - bar.h;
	}

	// ---------------------------
	// 4) 低HPなら「常時シェイク」＋「脈動(拡縮)」
	// ---------------------------
	const float clampedHp = std::clamp(hp_, 0.0f, maxHp_);
	const float heartsLeft = (hpPerHeart > 0.0f) ? (clampedHp / hpPerHeart) : 0.0f;

	float alwaysX = 0.0f;
	float alwaysY = 0.0f;
	float scaleMul = 1.0f;

	if (heartsLeft <= (float)layout_.lowHpAlwaysShakeHearts)
	{
		// 4個→弱、0個→強
		const float denom = std::max(1.0f, (float)layout_.lowHpAlwaysShakeHearts);
		const float t = Clamp01(1.0f - (heartsLeft / denom)); // 0..1

		// 常時シェイク
		const float amp = layout_.lowHpAlwaysAmp * (1.0f + t * 2.0f); // 最大3倍
		shakePhase_ += dt * 1.0f; // 常時で位相が進むようにする

		const float w = 2.0f * kPi * layout_.lowHpAlwaysFreq;
		const float time = shakePhase_; // 位相を時間扱い

		alwaysX = std::sin(w * time + 1.1f) * amp;
		alwaysY = std::cos(w * time * 1.27f + 0.4f) * amp;

		// 脈動（拡縮）
		const float pw = 2.0f * kPi * layout_.lowHpPulseFreq;
		const float pulse = (std::sin(pw * time) * 0.5f + 0.5f); // 0..1
		const float pulseAmp = layout_.lowHpPulseBase + layout_.lowHpPulseMul * t;
		scaleMul = 1.0f + pulseAmp * pulse;
	}

	// ---------------------------
	// 5) ハートのテクスチャ決定（半ハート単位）
	// ---------------------------
	int filledUnits = (unitHp > 0.0f) ? (int)std::floor((clampedHp / unitHp) + 1e-4f) : 0;
	filledUnits = std::clamp(filledUnits, 0, maxHearts * 2);

	auto setTexIfChanged = [&](HeartSlot& hs, const char* path)
		{
			if (hs.currentPath != path)
			{
				hs.spr->Initialize(path);
				hs.currentPath = path;
			}

			// ★拡縮（脈動）をサイズに反映
			const float baseSize = layout_.heartSize;
			const float s = baseSize * scaleMul;
			hs.spr->SetSize({ s, s });

			// 中心拡縮が気持ちいいので Anchor を中心に寄せる
			// ただし「並べる基準」は左下で管理したいので Position を少し補正する（後でやる）
			hs.spr->SetAnchorPoint({ 0.5f, 0.5f });
		};

	// ---------------------------
	// 6) 位置（武器スロット上）＋シェイク足し込み
	// ---------------------------
	const float baseSize = layout_.heartSize;
	const float stepX = baseSize + layout_.heartGapX; // 配置間隔は「基準サイズ」で固定（見た目が崩れない）
	const float stepY = baseSize + layout_.heartGapY;
	const int perRow = std::max(1, layout_.heartsPerRow);

	// 左下基準で並べたいので、中心Anchorにした分の補正を入れる
	// center = (left + baseSize*0.5, bottom - baseSize*0.5)
	const float startLeft = bar.x + layout_.heartOffsetX + shakeOffsetX_ + alwaysX;
	const float startBottom = bar.y - layout_.heartOffsetY + shakeOffsetY_ + alwaysY;

	for (int i = 0; i < maxHearts; ++i)
	{
		const int row = i / perRow;
		const int col = i % perRow;

		const float left = startLeft + col * stepX;
		const float bottom = startBottom - row * stepY;

		const char* tex = kPathHeartDeath;
		if (filledUnits >= 2) { tex = kPathHeartFull;  filledUnits -= 2; }
		else if (filledUnits == 1) { tex = kPathHeartHalf;  filledUnits -= 1; }
		else { tex = kPathHeartDeath; }

		auto& hs = hearts_[i];
		setTexIfChanged(hs, tex);

		// ★中心Anchorなので中心座標を渡す
		const float cx = left + baseSize * 0.5f;
		const float cy = bottom - baseSize * 0.5f;
		hs.spr->SetPosition({ cx, cy });
		hs.spr->Update();
	}
}

void HUDManager::DrawSprites()
{
	// ハート
	if (layout_.showHearts)
	{
		for (auto& h : hearts_)
		{
			if (h.spr) h.spr->Draw();
		}
	}

	// 右下ガイド
	if (layout_.showGrid)
	{
		reload_icon_->Draw();
		ammo_icon_->Draw();
		reticle_grid_icon_->Draw();

		r_key_icon_->Draw();
		mouse_left_icon_->Draw();
		mouse_right_icon_->Draw();
	}
}
