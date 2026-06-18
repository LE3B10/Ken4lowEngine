#define NOMINMAX
#include "HUDManager.h"
#include "Player.h"
#include "ParameterManager.h"
#include "FontAtlasLoader.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr const char* kBossHpBarGroup = "BossHpBar";

	std::string ToPercentText(int percent)
	{
		return std::to_string(std::clamp(percent, 0, 100)) + "%";
	}

	std::string BuildProgressBlocks(float progress)
	{
		const int filled = std::clamp(static_cast<int>(std::round(progress * 10.0f)), 0, 10);
		std::string out = "［";
		for (int i = 0; i < 10; ++i)
		{
			out += (i < filled) ? "■" : "□";
		}
		out += "］";
		return out;
	}

	float Length2D(float x, float y)
	{
		return std::sqrt(x * x + y * y);
	}
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
	InitializeBossGuideSprites();
	InitializeStage1ObjectiveGuide();
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
	UpdateBossGuideSprites(deltaTime);
	UpdateStage1ObjectiveGuideSprites(deltaTime);
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
	DrawStage1ObjectiveGuide();
	DrawBossHpBar();
	DrawBossGuide();

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

void HUDManager::SetStage1ObjectiveGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive)
{
	stage1ObjectiveGuideEnabled_ = enabled;
	stage1DestroyedCrystals_ = std::clamp(destroyedCrystals, 0, std::max(0, totalCrystals));
	stage1TotalCrystals_ = std::max(0, totalCrystals);
	stage1BossBattleActive_ = bossBattleActive;
	stage1BossDefeated_ = bossDefeated;
	stage1ObjectiveTutorialActive_ = tutorialActive;
}

void HUDManager::SetStage1ObjectiveTutorialAlpha(float alpha)
{
	stage1ObjectiveTutorialAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void HUDManager::SetStage1ObjectiveTutorialPage(int page)
{
	stage1ObjectiveTutorialPage_ = std::clamp(page, 0, 7);
}

void HUDManager::SetStage1ObjectiveTutorialProgress(float progress)
{
	stage1ObjectiveTutorialProgress_ = std::clamp(progress, 0.0f, 1.0f);
}

void HUDManager::SetStage1TutorialItemMarker(int markerIndex, bool visible, const K4E::Vector2& screenPosition, int itemType)
{
	if (markerIndex < 0 || markerIndex >= static_cast<int>(stage1TutorialItemMarkers_.size()))
	{
		return;
	}
	stage1TutorialItemMarkers_[markerIndex].visible = visible;
	stage1TutorialItemMarkers_[markerIndex].screenPosition = screenPosition;
	stage1TutorialItemMarkers_[markerIndex].itemType = itemType;
}

void HUDManager::NotifyStage1ObjectiveGuideStarted()
{
	// ステージ1開始時に目的を強めに表示し、初心者が最初の目標を見失わないようにする。
	stage1ObjectiveIntroTimer_ = std::max(0.0f, stage1ObjectiveGuideSettings_.introHoldTime);
}

void HUDManager::NotifyStage1BossAppeared()
{
	stage1BossNoticeTimer_ = std::max(0.0f, stage1ObjectiveGuideSettings_.bossNoticeTime);
}

void HUDManager::SetBossGuide(const K4E::Vector3& playerPos,
	const K4E::Vector3& bossPos,
	const K4E::Vector3& cameraForward,
	bool bossBattleActive)
{
	bossGuideBossPosition_ = bossPos;
	bossGuideActive_ = bossGuideSettings_.visible && bossBattleActive && bossGuideTimer_ > 0.0f;
	if (!bossGuideActive_)
	{
		return;
	}

	K4E::Vector3 toBoss = bossPos - playerPos;
	toBoss.y = 0.0f;
	toBoss = K4E::Vector3::NormalizeXZSafe(toBoss, { 0.0f, 0.0f, 1.0f });

	K4E::Vector3 forward = cameraForward;
	forward.y = 0.0f;
	forward = K4E::Vector3::NormalizeXZSafe(forward, { 0.0f, 0.0f, 1.0f });
	const K4E::Vector3 right = K4E::Vector3::PerpRightXZ(forward);

	const float screenX = K4E::Vector3::Dot(toBoss, right);
	const float screenY = -K4E::Vector3::Dot(toBoss, forward);
	const float screenLen = std::max(Length2D(screenX, screenY), 0.0001f);
	const float dirX = screenX / screenLen;
	const float dirY = screenY / screenLen;

	const K4E::Vector2 center = bossGuideSettings_.center;
	bossGuideDotPosition_ = {
		center.x + dirX * bossGuideSettings_.radius,
		center.y + dirY * bossGuideSettings_.radius
	};
	bossGuideLineCenter_ = {
		(center.x + bossGuideDotPosition_.x) * 0.5f,
		(center.y + bossGuideDotPosition_.y) * 0.5f
	};
	bossGuideLineLength_ = bossGuideSettings_.radius;
	bossGuideAngle_ = std::atan2(dirY, dirX);
}

void HUDManager::NotifyBossIntroCompleted(const K4E::Vector3& bossPos)
{
	// ボス登場演出直後だけ方向ガイドを出し、プレイヤーが復帰後にボス位置を追いやすくする。
	bossGuideBossPosition_ = bossPos;
	bossGuideTimer_ = std::max(0.0f, bossGuideSettings_.holdTime);
	bossGuideActive_ = bossGuideSettings_.visible && bossGuideTimer_ > 0.0f;
}

void HUDManager::SetWeaponSlotVisibleSlotCount(int count)
{
	if (weaponSlot_)
	{
		weaponSlot_->SetVisibleSlotCount(count);
	}
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

void HUDManager::InitializeBossGuideSprites()
{
	bossGuideLineSprite_ = std::make_unique<K4E::Sprite>();
	bossGuideLineSprite_->Initialize("Effects/white.dds");
	bossGuideLineSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	bossGuideDotBackSprite_ = std::make_unique<K4E::Sprite>();
	bossGuideDotBackSprite_->Initialize("Effects/white.dds");
	bossGuideDotBackSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	bossGuideDotSprite_ = std::make_unique<K4E::Sprite>();
	bossGuideDotSprite_->Initialize("Effects/white.dds");
	bossGuideDotSprite_->SetAnchorPoint({ 0.5f, 0.5f });
}

void HUDManager::InitializeStage1ObjectiveGuide()
{
	stage1ObjectiveGuideBackSprite_ = std::make_unique<K4E::Sprite>();
	stage1ObjectiveGuideBackSprite_->Initialize("Effects/white.dds");
	stage1ObjectiveGuideBackSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	stage1ObjectiveGuideAccentSprite_ = std::make_unique<K4E::Sprite>();
	stage1ObjectiveGuideAccentSprite_->Initialize("Effects/white.dds");
	stage1ObjectiveGuideAccentSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	stage1ObjectiveTextDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	stage1ObjectiveTextReady_ = false;
	try
	{
		auto fontDefJP = K4E::FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);
		stage1ObjectiveTextDrawer_->Initialize(fontDefJP);
		stage1ObjectiveTextReady_ = true;
	} catch (...)
	{
		stage1ObjectiveTextReady_ = false;
	}
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

void HUDManager::UpdateBossGuideSprites(float deltaTime)
{
	if (bossGuideTimer_ > 0.0f)
	{
		bossGuideTimer_ = std::max(0.0f, bossGuideTimer_ - deltaTime);
	}

	if (!bossGuideActive_)
	{
		return;
	}

	const float fade = std::clamp(bossGuideTimer_ / std::max(0.01f, bossGuideSettings_.holdTime), 0.0f, 1.0f);
	const float alpha = std::clamp(fade * 1.4f, 0.0f, 0.88f);

	if (bossGuideLineSprite_)
	{
		bossGuideLineSprite_->SetPosition(bossGuideLineCenter_);
		bossGuideLineSprite_->SetSize({ bossGuideLineLength_, bossGuideSettings_.lineThickness });
		bossGuideLineSprite_->SetRotation(bossGuideAngle_);
		bossGuideLineSprite_->SetColor({ 1.0f, 0.72f, 0.18f, alpha });
		bossGuideLineSprite_->Update();
	}
	if (bossGuideDotBackSprite_)
	{
		bossGuideDotBackSprite_->SetPosition(bossGuideDotPosition_);
		bossGuideDotBackSprite_->SetSize({ bossGuideSettings_.dotSize + 12.0f, bossGuideSettings_.dotSize + 12.0f });
		bossGuideDotBackSprite_->SetRotation(0.0f);
		bossGuideDotBackSprite_->SetColor({ 0.05f, 0.02f, 0.01f, alpha * 0.65f });
		bossGuideDotBackSprite_->Update();
	}
	if (bossGuideDotSprite_)
	{
		bossGuideDotSprite_->SetPosition(bossGuideDotPosition_);
		bossGuideDotSprite_->SetSize({ bossGuideSettings_.dotSize, bossGuideSettings_.dotSize });
		bossGuideDotSprite_->SetRotation(0.0f);
		bossGuideDotSprite_->SetColor({ 1.0f, 0.18f, 0.08f, alpha });
		bossGuideDotSprite_->Update();
	}
}

void HUDManager::UpdateStage1ObjectiveGuideSprites(float deltaTime)
{
	if (stage1ObjectiveIntroTimer_ > 0.0f)
	{
		stage1ObjectiveIntroTimer_ = std::max(0.0f, stage1ObjectiveIntroTimer_ - deltaTime);
	}
	if (stage1BossNoticeTimer_ > 0.0f)
	{
		stage1BossNoticeTimer_ = std::max(0.0f, stage1BossNoticeTimer_ - deltaTime);
	}

	const bool shouldShow = stage1ObjectiveGuideSettings_.visible && stage1ObjectiveGuideEnabled_ && !stage1BossDefeated_;
	if (stage1ObjectiveTutorialActive_)
	{
		stage1ObjectiveGuideAlpha_ = shouldShow ? 0.92f * stage1ObjectiveTutorialAlpha_ : 0.0f;
	}
	else
	{
		const float targetAlpha = shouldShow ? 0.70f : 0.0f;
		const float approach = std::clamp(deltaTime * 8.0f, 0.0f, 1.0f);
		stage1ObjectiveGuideAlpha_ += (targetAlpha - stage1ObjectiveGuideAlpha_) * approach;
	}

	const K4E::Vector2 center = stage1ObjectiveTutorialActive_
		? stage1ObjectiveGuideSettings_.tutorialCenter
		: stage1ObjectiveGuideSettings_.center;
	const K4E::Vector2 size = stage1ObjectiveTutorialActive_
		? stage1ObjectiveGuideSettings_.tutorialPanelSize
		: stage1ObjectiveGuideSettings_.panelSize;
	if (stage1ObjectiveGuideBackSprite_)
	{
		stage1ObjectiveGuideBackSprite_->SetPosition(center);
		stage1ObjectiveGuideBackSprite_->SetSize(size);
		stage1ObjectiveGuideBackSprite_->SetColor({ 0.03f, 0.035f, 0.045f, stage1ObjectiveGuideAlpha_ * 0.82f });
		stage1ObjectiveGuideBackSprite_->Update();
	}
	if (stage1ObjectiveGuideAccentSprite_)
	{
		stage1ObjectiveGuideAccentSprite_->SetPosition({ center.x, center.y + size.y * 0.5f - 5.0f });
		stage1ObjectiveGuideAccentSprite_->SetSize({ size.x * 0.92f, 5.0f });
		stage1ObjectiveGuideAccentSprite_->SetColor({ 0.35f, 0.86f, 1.0f, stage1ObjectiveGuideAlpha_ });
		stage1ObjectiveGuideAccentSprite_->Update();
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
}

void HUDManager::DrawStage1ObjectiveGuide()
{
	if (stage1ObjectiveGuideAlpha_ <= 0.01f || !stage1ObjectiveGuideEnabled_)
	{
		return;
	}

	if (stage1ObjectiveGuideBackSprite_) stage1ObjectiveGuideBackSprite_->Draw();
	if (stage1ObjectiveGuideAccentSprite_) stage1ObjectiveGuideAccentSprite_->Draw();

	if (!stage1ObjectiveTextReady_ || !stage1ObjectiveTextDrawer_)
	{
		return;
	}

	stage1ObjectiveTextDrawer_->Reset();
	stage1ObjectiveTextDrawer_->SetLetterSpacing(1.0f);
	stage1ObjectiveTextDrawer_->SetLineSpacing(6.0f);

	if (stage1ObjectiveTutorialActive_)
	{
		if (stage1ObjectiveTutorialPage_ == 0)
		{
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("クリスタルを3つ破壊しろ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 34.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.progressScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("光っている青い結晶が破壊対象だ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 12.0f
				});
			stage1ObjectiveTextDrawer_->DrawTextCentered("すべて破壊するとボスが出現する", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 54.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.smallScale);
			stage1ObjectiveTextDrawer_->SetColor({ 1.0f, 0.82f, 0.30f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("左クリックで次へ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 96.0f
				});
		}
		else if (stage1ObjectiveTutorialPage_ == 1)
		{
			const int percent = static_cast<int>(std::round(stage1ObjectiveTutorialProgress_ * 100.0f));
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			// 日本語と英数字を分けず、UTF-8文字列としてまとめてTextSpriteDrawerへ渡す。
			stage1ObjectiveTextDrawer_->DrawTextCentered("WASDで移動しろ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 54.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.progressScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("移動練習：" + ToPercentText(percent), {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 18.0f
				});
			stage1ObjectiveTextDrawer_->DrawTextCentered(BuildProgressBlocks(stage1ObjectiveTutorialProgress_), {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 64.0f
				});
		}
		else if (stage1ObjectiveTutorialPage_ == 2)
		{
			const int percent = static_cast<int>(std::round(stage1ObjectiveTutorialProgress_ * 100.0f));
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("マウスで視点を動かせ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 54.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.progressScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("視点移動：" + ToPercentText(percent), {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 18.0f
				});
			stage1ObjectiveTextDrawer_->DrawTextCentered(BuildProgressBlocks(stage1ObjectiveTutorialProgress_), {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 64.0f
				});
		}
		else if (stage1ObjectiveTutorialPage_ == 3)
		{
			const int shotCount = std::clamp(static_cast<int>(std::round(stage1ObjectiveTutorialProgress_ * 3.0f)), 0, 3);
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			// チュートリアル用テキストを共通フォントへ統一し、キー表記が欠けないようにする。
			stage1ObjectiveTextDrawer_->DrawTextCentered("左クリックで射撃しろ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 54.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.progressScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("射撃練習：" + std::to_string(shotCount) + " / 3", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 18.0f
				});
			stage1ObjectiveTextDrawer_->DrawTextCentered(BuildProgressBlocks(stage1ObjectiveTutorialProgress_), {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 64.0f
				});
		}
		else if (stage1ObjectiveTutorialPage_ == 4)
		{
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("Rキーでリロードしろ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 18.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.smallScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("リロード完了で次へ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 44.0f
				});
		}
		else if (stage1ObjectiveTutorialPage_ == 5)
		{
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("敵を倒してみろ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 20.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.smallScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("リロード後に出た弱い敵を1体倒そう", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 42.0f
				});
		}
		else if (stage1ObjectiveTutorialPage_ == 6)
		{
			const int pickedCount = std::clamp(static_cast<int>(std::round(stage1ObjectiveTutorialProgress_ * 2.0f)), 0, 2);
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("アイテムを2つ拾え", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 48.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.smallScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("アイテムに近づくと自動で拾える", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 8.0f
				});
			stage1ObjectiveTextDrawer_->DrawTextCentered("取得：" + std::to_string(pickedCount) + " / 2", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 58.0f
				});
		}
		else
		{
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.titleScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("チュートリアル完了", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y - 20.0f
				});
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.smallScale);
			stage1ObjectiveTextDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, stage1ObjectiveGuideAlpha_ });
			stage1ObjectiveTextDrawer_->DrawTextCentered("クリスタルを3つ破壊しろ", {
				stage1ObjectiveGuideSettings_.tutorialCenter.x,
				stage1ObjectiveGuideSettings_.tutorialCenter.y + 42.0f
				});
		}
		for (const Stage1TutorialItemMarker& marker : stage1TutorialItemMarkers_)
		{
			if (!marker.visible)
			{
				continue;
			}
			const bool isAmmo = marker.itemType == 1;
			stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.smallScale);
			stage1ObjectiveTextDrawer_->SetColor({ 1.0f, 0.92f, 0.28f, stage1ObjectiveGuideAlpha_ });
			// 初心者が拾う対象を見失わないよう、チュートリアル中はアイテム上に説明マーカーを表示する。
			stage1ObjectiveTextDrawer_->DrawTextCentered("▼", {
				marker.screenPosition.x,
				marker.screenPosition.y - 54.0f
				});
			stage1ObjectiveTextDrawer_->DrawTextCentered(isAmmo ? "弾薬箱" : "回復薬", {
				marker.screenPosition.x,
				marker.screenPosition.y - 18.0f
				});
			stage1ObjectiveTextDrawer_->DrawTextCentered(isAmmo ? "弾を補充" : "HPを回復", {
				marker.screenPosition.x,
				marker.screenPosition.y + 16.0f
				});
		}
		return;
	}

	const bool crystalsDone = stage1TotalCrystals_ > 0 && stage1DestroyedCrystals_ >= stage1TotalCrystals_;
	const std::string objective = crystalsDone
		? "目標：ボスを倒せ"
		: ("目標：クリスタル " + std::to_string(stage1DestroyedCrystals_) + " / " + std::to_string(stage1TotalCrystals_));

	stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.smallScale);
	stage1ObjectiveTextDrawer_->SetColor({ 0.90f, 0.96f, 1.0f, stage1ObjectiveGuideAlpha_ });
	stage1ObjectiveTextDrawer_->DrawTextCentered(objective, {
		stage1ObjectiveGuideSettings_.center.x,
		stage1ObjectiveGuideSettings_.center.y - 2.0f
		});

	if (stage1BossNoticeTimer_ > 0.0f)
	{
		const float noticeAlpha = std::clamp(stage1BossNoticeTimer_ / std::max(0.01f, stage1ObjectiveGuideSettings_.bossNoticeTime), 0.0f, 1.0f);
		stage1ObjectiveTextDrawer_->SetScale(stage1ObjectiveGuideSettings_.noticeScale);
		stage1ObjectiveTextDrawer_->SetColor({ 1.0f, 0.78f, 0.22f, noticeAlpha });
		stage1ObjectiveTextDrawer_->DrawTextCentered("ボスが出現した!", stage1ObjectiveGuideSettings_.noticeCenter);
	}
}

void HUDManager::DrawBossGuide()
{
	if (!bossGuideActive_ || bossGuideTimer_ <= 0.0f)
	{
		return;
	}

	if (bossGuideLineSprite_) bossGuideLineSprite_->Draw();
	if (bossGuideDotBackSprite_) bossGuideDotBackSprite_->Draw();
	if (bossGuideDotSprite_) bossGuideDotSprite_->Draw();
}
