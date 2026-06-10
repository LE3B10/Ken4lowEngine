#define NOMINMAX
#include "HUDManager.h"
#include "Player.h"
#include "ParameterManager.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr const char* kBossHpBarGroup = "BossHpBar";
}

HUDManager::~HUDManager()
{
	K4E::ParameterManager::GetInstance()->UnregisterParameterApplier(kBossHpBarGroup, this);
}

/// -------------------------------------------------------------
///                    初期化処理
/// -------------------------------------------------------------
void HUDManager::Initialize()
{
	// 個別HUDはHUDManagerが所有し、GamePlayWorldからはHUDManager経由でまとめて更新/描画する。
	reloadCircle_ = std::make_unique<ReloadCircle>();
	reloadCircle_->Initialize("UI/Common/reload-circle.dds");

	// 十字照準の初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	// HP（ハート）の初期化
	hpWidget_ = std::make_unique<HPWidget>();
	hpWidget_->Initialize();

	// HPは固定内部解像度に合わせた左下寄り配置で、ハート数が増えても横方向に伸びる。
	hpWidget_->SetAnchorTopLeft({ 560.0f, 880.0f });
	hpWidget_->SetIconSize({ 22.0f, 22.0f });
	hpWidget_->SetPadding(6.0f);
	hpWidget_->SetHpPerHeart(10.0f); // 1ハート=10HP（必要なら変更）

	// 武器スロットHUDの初期化
	weaponSlot_ = std::make_unique<WeaponSlot>();
	weaponSlot_->Initialize("UI/Common/slot_frame.dds", "UI/Common/slot_frame_selected.dds");
	weaponSlot_->InitializeSlotNumbers("UI/Common/numbers02.dds", 50.0f, 50.0f, { 8.0f, 8.0f }, 2.0f, 32, 32);

	// 武器カテゴリ別アイコン（スロット0..5）をまとめて渡し、WeaponSlot側で選択状態を描き分ける。
	const std::array<std::string, WeaponSlot::kSlotCount> weaponIcons = {
		"UI/Icons/primary_icon.dds",
		"UI/Icons/backup_icon.dds",
		"UI/Icons/melee_icon.dds",
		"UI/Icons/special_icon.dds",
		"UI/Icons/sniper_icon.dds",
		"UI/Icons/heavy_icon.dds"
	};
	weaponSlot_->InitializeIcons(weaponIcons);

	weaponSlot_->InitializeAmmoDelimiter(
		"UI/Icons/slash_icon.dds",
		{ 20.0f, 20.0f },   // 数字が20x20ならこれがちょうど良い
		{ 0.0f, 0.0f }      // 微調整したいならここでオフセット
	);

	// 弾薬表示初期化
	weaponSlot_->InitializeAmmoNumbers("UI/Common/Number.dds",
		50, 50,
		{ 10, 10 },
		-5.0f,   // spacingは小さく
		20.0f, 20.0f); // drawサイズ

	/*waveUI_ = std::make_unique<WaveUI>();
	waveUI_->Initialize();
	waveUI_->SetVisible(true);*/

	damageIndicatorManager_ = std::make_unique<DamageIndicatorManager>();
	damageIndicatorManager_->Initialize();

	noAmmoUI_ = std::make_unique<NoAmmoUI>();
	noAmmoUI_->Initialize();

	controlGuideUI_ = std::make_unique<ControlGuideUI>();
	controlGuideUI_->Initialize(
		"UI/Common/ammo_icon.dds",
		"UI/Common/mouse_leftClick.dds",
		"UI/Common/reticle_icon.dds",
		"UI/Common/mouse_rightClick.dds",
		"UI/Common/R_key_icon.dds",
		"UI/Common/reload_icon.dds"
	);
	controlGuideUI_->SetVisible(true);
	controlGuideUI_->SetAnchorTopLeft({ 1500.0f, 930.0f });

	InitializeBossHpBarSprites();
	RegisterBossHpBarParameters();
	ApplyBossHpBarParameters();
}

/// -------------------------------------------------------------
///                    更新処理
/// -------------------------------------------------------------
void HUDManager::Update(float deltaTime)
{
	bool isReloadingForHUD = false; // Crosshairへ渡す用

	/// ---------- リロード円：プレイヤーの武器リロード状態に同期 ---------- ///
	if (reloadCircle_)
	{
		bool isReloading = false;
		float reloadTimer = 0.0f;
		float reloadSec = 0.0f;

		const bool hasInfo = (player_ != nullptr) && player_->GetReloadUI(isReloading, reloadTimer, reloadSec);
		if (!hasInfo || reloadSec <= 1e-6f)
		{
			// 情報が取れない/未ロードなら非表示
			reloadCircle_->SetReloading(false, 0.0f);
			isReloadingForHUD = false;
			prevReloading_ = false;
		}
		else
		{
			// 武器側のreloadTimer表現差を吸収し、HUDでは常に0→1の進捗として扱う。
			if (isReloading && !prevReloading_)
			{
				// start直後にtimerがreloadSecに近ければ「残り時間」、0に近ければ「経過時間」とみなす。
				reloadTimerIsRemaining_ = (reloadTimer > reloadSec * 0.5f);
			}

			isReloadingForHUD = isReloading;

			float progress01 = 0.0f;
			if (isReloading)
			{
				const float t = std::clamp(reloadTimer / reloadSec, 0.0f, 1.0f);
				progress01 = reloadTimerIsRemaining_ ? (1.0f - t) : t;
			}

			reloadCircle_->SetReloading(isReloading, progress01);
			prevReloading_ = isReloading;
		}
	}

	// ---------- クロスヘア：武器のレティクルデータを反映 ----------
	if (crosshair_ && player_)
	{
		FWeaponReticleData r{};
		float spread = 0.0f;
		bool isADS = false;

		if (player_->GetReticleUI(r, spread, isADS))
		{
			// 武器マスタ由来のレティクル設定を毎フレーム反映し、Editor調整を即時確認できるようにする。
			crosshair_->SetReticleType(static_cast<int>(r.reticleType));
			crosshair_->SetReticleTexture(r.reticleTexturePath);
			crosshair_->SetBaseSize(r.reticleBaseSize);
			crosshair_->SetMaxSize(r.reticleMaxSize);
			crosshair_->SetExpandPerShot(r.reticleExpandPerShot);
			crosshair_->SetRecoverSpeed(r.reticleRecoverSpeed);
			crosshair_->SetSpreadValue(spread);

			// 移動状態そのものはPlayer側から渡されるため、ここでは倍率だけをレティクルへ反映する。
			crosshair_->SetMoveExpandEnabled(r.bEnableMoveReticleExpand);
			crosshair_->SetMoveExpandMultipliers(
				r.moveExpandMultiplier,
				r.sprintExpandMultiplier,
				r.airExpandMultiplier,
				r.landExpandImpulse);

			// ADS時は通常照準を隠す/専用照準へ差し替えるなど、武器ごとの見え方を同期する。
			crosshair_->SetHideInADS(r.bHideReticleInADS);
			crosshair_->SetADSState(isADS);
			crosshair_->SetHideWhileReload(true);
			crosshair_->SetReloadState(isReloadingForHUD);
			crosshair_->SetUseADSReticleOverride(r.bUseAdsReticleOverride);
			crosshair_->SetADSReticleTexture(r.adsReticleTexturePath);
			crosshair_->SetUseADSCenterDot(r.bUseAdsCenterDot);
			crosshair_->SetADSCenterDotTexture(r.adsCenterDotTexturePath);
			crosshair_->SetADSBlendTime(r.adsReticleBlendTime);

			// ヒット/撃破マーカーは通知時に使う素材と表示時間だけをここで最新化しておく。
			crosshair_->SetShowHitMarker(r.bShowHitMarker);
			crosshair_->SetHitMarkerTexture(r.hitMarkerTexturePath);
			crosshair_->SetUseHeadshotMarker(r.bUseHeadshotMarker);
			crosshair_->SetHeadshotHitMarkerTexture(r.headshotHitMarkerTexturePath);
			crosshair_->SetUseKillConfirmMarker(r.bUseKillConfirmMarker);
			crosshair_->SetKillConfirmMarkerTexture(r.killConfirmMarkerTexturePath);
			crosshair_->SetHitMarkerDuration(r.hitMarkerDuration);
			crosshair_->SetKillConfirmDuration(r.killConfirmDuration);
		}
	}

	if (hpWidget_) hpWidget_->Update();
	if (reloadCircle_) reloadCircle_->Update();
	if (crosshair_) crosshair_->Update();


	// ---------- 武器スロットHUD ----------
	if (weaponSlot_ && player_)
	{
		WeaponSlot::HudSnapshot snap{};
		if (player_->GetWeaponSlotHUD(snap))
		{
			weaponSlot_->Update(snap);
		}
	}

	//if (waveUI_) waveUI_->Update(deltaTime);

	if (damageIndicatorManager_) damageIndicatorManager_->Update(deltaTime);

	if (noAmmoUI_ && player_)
	{
		bool showNoAmmo = false;
		player_->GetNoAmmoUI(showNoAmmo);
		noAmmoUI_->SetVisible(showNoAmmo);
		noAmmoUI_->Update(deltaTime);
	}

	if (controlGuideUI_) controlGuideUI_->Update(deltaTime);

	ApplyBossHpBarParameters();
	UpdateBossHpBarSprites();
}

/// -------------------------------------------------------------
///                    描画処理
/// -------------------------------------------------------------
void HUDManager::Draw()
{
	if (hpWidget_ && hpWidget_->IsVisible()) hpWidget_->Draw();
	if (reloadCircle_ && reloadCircle_->IsVisible()) reloadCircle_->Draw();
	if (crosshair_ && crosshair_->IsVisible()) crosshair_->Draw();
	if (weaponSlot_) weaponSlot_->Draw();
	if (waveUI_ && IsWaveUIDrawEnabled()) waveUI_->Draw();
	DrawBossHpBar();

	if (damageIndicatorManager_) damageIndicatorManager_->Draw();

	if (noAmmoUI_ && noAmmoUI_->IsVisible()) noAmmoUI_->Draw();

	if (controlGuideUI_ && controlGuideUI_->IsVisible()) controlGuideUI_->Draw();
}

void HUDManager::SetHP(float hp, float maxHp)
{
	if (hpWidget_) hpWidget_->SetHP(hp, maxHp);
}

void HUDManager::SetBossHP(float hp, float maxHp, bool bossBattleActive)
{
	bossBattleActive_ = bossBattleActive;
	bossHp_ = std::max(0.0f, hp);
	bossMaxHp_ = std::max(0.0f, maxHp);

	// ボスHP率をUIへ反映する処理。最大HPが0以下なら安全に0扱いにする。
	bossHpRate_ = (bossMaxHp_ > 0.0f) ? std::clamp(bossHp_ / bossMaxHp_, 0.0f, 1.0f) : 0.0f;
	bossHpBarRuntimeVisible_ = bossHpBarSettings_.visible && bossBattleActive_ && bossHp_ > 0.0f;
}

void HUDManager::NotifyPlayerHit(float strength01)
{
	if (hpWidget_) hpWidget_->NotifyHit(strength01);
}

void HUDManager::NotifyEnemyHit(bool isHeadshot)
{
	if (!crosshair_) return;
	crosshair_->NotifyEnemyHit(isHeadshot, false);
}

void HUDManager::NotifyEnemyKill(bool isHeadshot)
{
	if (!crosshair_) return;
	// kill優先（ヘッドショットキルなら killConfirm テクスチャにフォールバック）
	crosshair_->NotifyEnemyHit(isHeadshot, true);
}

void HUDManager::SetCrosshairMovementState(bool isMoving, bool isSprinting, bool isAirborne)
{
	if (!crosshair_) return;
	crosshair_->SetMovementState(isMoving, isSprinting, isAirborne);
}

void HUDManager::NotifyCrosshairLanded()
{
	if (!crosshair_) return;
	crosshair_->NotifyLanded();
}

void HUDManager::SetWaveDisplayState(const WaveUI::DisplayState& state)
{
	if (waveUI_) waveUI_->SetDisplayState(state);
}

void HUDManager::NotifyWaveStarted(int waveNumber, bool isFinalWave)
{
	if (waveUI_) waveUI_->NotifyWaveStarted(waveNumber, isFinalWave);
}

void HUDManager::NotifyAllWavesCleared()
{
	if (waveUI_) waveUI_->NotifyAllWavesCleared();
}

void HUDManager::SetWaveUIVisible(bool v)
{
	if (waveUI_) waveUI_->SetVisible(v);
}

bool HUDManager::IsWaveUIDrawEnabled() const
{
	// ボス戦中にWave UIを非表示にする処理。WaveUI自体の状態は破棄せず描画だけ止める。
	if (bossHpBarSettings_.hideWaveUI && bossBattleActive_)
	{
		return false;
	}
	return waveUI_ && waveUI_->IsVisible();
}

void HUDManager::AddDamageIndicator(const K4E::Vector3& playerPos, const K4E::Vector3& attackerPos, const K4E::Vector3& cameraForward, const K4E::Vector3& cameraRight)
{
	if (damageIndicatorManager_)
	{
		damageIndicatorManager_->AddIndicator(playerPos, attackerPos, cameraForward, cameraRight);
	}
}

void HUDManager::SetCrosshairTargetingEnemy(bool v)
{
	if (!crosshair_) return;
	// クロスヘア色を切り替える処理。Detectorで遮蔽済みの直接対象だけtrueになる。
	crosshair_->SetTargetingEnemy(v);
}

void HUDManager::SetCrosshairTargetColors(const K4E::Vector4& normalColor, const K4E::Vector4& targetColor)
{
	if (!crosshair_) return;
	crosshair_->SetTargetColors(normalColor, targetColor);
}

void HUDManager::RegisterBossHpBarParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	parameters->CreateGroup(kBossHpBarGroup);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarVisible", bossHpBarSettings_.visible);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarPosition", bossHpBarSettings_.position, K4E::Vector3{ 0.0f, 0.0f, 0.0f }, K4E::Vector3{ 1920.0f, 1080.0f, 0.0f });
	parameters->AddItem(kBossHpBarGroup, "bossHpBarWidth", bossHpBarSettings_.width, 100.0f, 1600.0f);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarHeight", bossHpBarSettings_.height, 4.0f, 80.0f);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarNameOffset", bossHpBarSettings_.nameOffset, K4E::Vector3{ -500.0f, -200.0f, 0.0f }, K4E::Vector3{ 500.0f, 200.0f, 0.0f });
	parameters->AddStringItem(kBossHpBarGroup, "bossHpBarDisplayName", bossHpBarSettings_.displayName, {});
	parameters->AddItem(kBossHpBarGroup, "bossHpBarShowAfterIntro", bossHpBarSettings_.showAfterIntro);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarHideWaveUI", bossHpBarSettings_.hideWaveUI);
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarVisible", "ボスHPバー表示");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarPosition", "表示位置");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarWidth", "バー幅");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarHeight", "バー高さ");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarNameOffset", "名前表示オフセット");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarDisplayName", "ボス表示名");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarShowAfterIntro", "登場後に表示");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarHideWaveUI", "ボス戦中Wave UI非表示");
	parameters->RegisterParameterApplier(kBossHpBarGroup, this, [this]() { ApplyBossHpBarParameters(); });
	parameters->LoadFile(kBossHpBarGroup);
}

void HUDManager::ApplyBossHpBarParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	bossHpBarSettings_.visible = parameters->GetValue<bool>(kBossHpBarGroup, "bossHpBarVisible");
	bossHpBarSettings_.position = parameters->GetValue<K4E::Vector3>(kBossHpBarGroup, "bossHpBarPosition");
	bossHpBarSettings_.width = std::max(1.0f, parameters->GetValue<float>(kBossHpBarGroup, "bossHpBarWidth"));
	bossHpBarSettings_.height = std::max(1.0f, parameters->GetValue<float>(kBossHpBarGroup, "bossHpBarHeight"));
	bossHpBarSettings_.nameOffset = parameters->GetValue<K4E::Vector3>(kBossHpBarGroup, "bossHpBarNameOffset");
	bossHpBarSettings_.displayName = parameters->GetValue<std::string>(kBossHpBarGroup, "bossHpBarDisplayName");
	bossHpBarSettings_.showAfterIntro = parameters->GetValue<bool>(kBossHpBarGroup, "bossHpBarShowAfterIntro");
	bossHpBarSettings_.hideWaveUI = parameters->GetValue<bool>(kBossHpBarGroup, "bossHpBarHideWaveUI");
	bossHpBarRuntimeVisible_ = bossHpBarSettings_.visible && bossBattleActive_ && bossHp_ > 0.0f;
}

void HUDManager::InitializeBossHpBarSprites()
{
	bossHpFrameSprite_ = std::make_unique<K4E::Sprite>();
	bossHpFrameSprite_->Initialize("Effects/white.dds");
	bossHpFrameSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	bossHpBackSprite_ = std::make_unique<K4E::Sprite>();
	bossHpBackSprite_->Initialize("Effects/white.dds");
	bossHpBackSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	bossHpDelaySprite_ = std::make_unique<K4E::Sprite>();
	bossHpDelaySprite_->Initialize("Effects/white.dds");
	bossHpDelaySprite_->SetAnchorPoint({ 0.0f, 0.5f });

	bossHpFillSprite_ = std::make_unique<K4E::Sprite>();
	bossHpFillSprite_->Initialize("Effects/white.dds");
	bossHpFillSprite_->SetAnchorPoint({ 0.0f, 0.5f });
}

void HUDManager::UpdateBossHpBarSprites()
{
	const float approachSpeed = 0.9f;
	bossDelayedHpRate_ += (bossHpRate_ - bossDelayedHpRate_) * approachSpeed * 0.016f;
	if (std::fabs(bossDelayedHpRate_ - bossHpRate_) < 0.002f)
	{
		bossDelayedHpRate_ = bossHpRate_;
	}

	const K4E::Vector2 center{ bossHpBarSettings_.position.x, bossHpBarSettings_.position.y };
	const float width = bossHpBarSettings_.width;
	const float height = bossHpBarSettings_.height;
	const K4E::Vector2 left{ center.x - width * 0.5f, center.y };

	if (bossHpFrameSprite_)
	{
		bossHpFrameSprite_->SetPosition(center);
		bossHpFrameSprite_->SetSize({ width + 6.0f, height + 6.0f });
		bossHpFrameSprite_->SetColor({ 0.02f, 0.01f, 0.015f, 0.95f });
		bossHpFrameSprite_->Update();
	}
	if (bossHpBackSprite_)
	{
		bossHpBackSprite_->SetPosition(center);
		bossHpBackSprite_->SetSize({ width, height });
		bossHpBackSprite_->SetColor({ 0.12f, 0.05f, 0.08f, 0.86f });
		bossHpBackSprite_->Update();
	}
	if (bossHpDelaySprite_)
	{
		bossHpDelaySprite_->SetPosition(left);
		bossHpDelaySprite_->SetSize({ width * std::clamp(bossDelayedHpRate_, 0.0f, 1.0f), height });
		bossHpDelaySprite_->SetColor({ 1.0f, 0.55f, 0.18f, 0.75f });
		bossHpDelaySprite_->Update();
	}
	if (bossHpFillSprite_)
	{
		bossHpFillSprite_->SetPosition(left);
		bossHpFillSprite_->SetSize({ width * bossHpRate_, height });
		bossHpFillSprite_->SetColor({ 0.82f, 0.06f, 0.18f, 0.96f });
		bossHpFillSprite_->Update();
	}
}

void HUDManager::DrawBossHpBar()
{
	// ボスHPバーを表示する条件判定。登場演出完了後のボス戦中だけ画面固定UIとして描く。
	if (!bossHpBarRuntimeVisible_ || !bossHpBarSettings_.showAfterIntro)
	{
		return;
	}

	if (bossHpFrameSprite_) bossHpFrameSprite_->Draw();
	if (bossHpBackSprite_) bossHpBackSprite_->Draw();
	if (bossHpDelaySprite_) bossHpDelaySprite_->Draw();
	if (bossHpFillSprite_) bossHpFillSprite_->Draw();

#ifdef USE_IMGUI
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	if (drawList)
	{
		const K4E::Vector2 center{ bossHpBarSettings_.position.x, bossHpBarSettings_.position.y };
		const ImVec2 namePos{
			center.x + bossHpBarSettings_.nameOffset.x,
			center.y + bossHpBarSettings_.nameOffset.y
		};
		const ImVec2 textSize = ImGui::CalcTextSize(bossHpBarSettings_.displayName.c_str());
		drawList->AddText({ namePos.x - textSize.x * 0.5f, namePos.y }, IM_COL32(255, 230, 230, 255), bossHpBarSettings_.displayName.c_str());
	}
#endif
}
