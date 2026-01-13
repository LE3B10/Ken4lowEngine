#define NOMINMAX
#include "HUDManager.h"
#include "DirectXCommon.h"

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI

namespace
{
	// テクスチャパス（今まで通り icon/ 配下を想定）
	constexpr const char* kPathReload = "icon/reload_icon.png";
	constexpr const char* kPathAmmo = "icon/ammo_icon.png";
	constexpr const char* kPathReticle = "icon/reticle_icon.png";
	constexpr const char* kPathRKey = "icon/R_key_icon.png";
	constexpr const char* kPathMouseL = "icon/mouse_leftClick.png";
	constexpr const char* kPathMouseR = "icon/mouse_rightClick.png";
}

/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void HUDManager::Initialize()
{
	InitializeSprites();
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

	UpdateSprites(screenW, screenH);
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
	ImGui::DragFloat("Center Reticle Size", &layout_.centerReticleSize, 1.0f, 8.0f, 256.0f, "%.0f");

	if (layout_.layoutMode == 1)
	{
		ImGui::Text("RightBottom 2x3");
		ImGui::Separator();
		ImGui::DragFloat("Margin X", &layout_.marginX, 1.0f, 0.0f, 600.0f, "%.0f");
		ImGui::DragFloat("Margin Y", &layout_.marginY, 1.0f, 0.0f, 600.0f, "%.0f");
		ImGui::DragFloat("Gap X", &layout_.gapX, 1.0f, 0.0f, 400.0f, "%.0f");
		ImGui::DragFloat("Gap Y", &layout_.gapY, 1.0f, 0.0f, 400.0f, "%.0f");
	}
	else
	{
		ImGui::Text("Center (Debug)");
		ImGui::Separator();
		ImGui::DragFloat("Center Offset X", &layout_.centerOffsetX, 1.0f, -1000.0f, 1000.0f, "%.0f");
		ImGui::DragFloat("Center Offset Y", &layout_.centerOffsetY, 1.0f, -1000.0f, 1000.0f, "%.0f");
	}

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

void HUDManager::UpdateSprites(float screenW, float screenH)
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

void HUDManager::DrawSprites()
{
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
