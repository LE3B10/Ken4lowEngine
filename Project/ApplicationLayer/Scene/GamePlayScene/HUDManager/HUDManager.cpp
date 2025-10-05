#define NOMINMAX
#include "HUDManager.h"

namespace
{
	// レイアウト定義
	struct Layout
	{
		// HP
		const Vector2 hpPosLT{ 30.0f, 22.0f };
		const Vector2 hpSize{ 240.0f, 22.0f };

		// クイックバー
		const Vector2 slotSize{ 92.0f, 76.0f };
		const float   slotGap = 6.0f;
		const float   slotsStartX = 640.0f - (slotSize.x * 6 + slotGap * 5) / 2.0f;
		const float   slotsY = 680.0f - slotSize.y;

		// アクションヒント
		const Vector2 actionPanelPos{ 980.0f, 520.0f };
		const Vector2 actionPanelSize{ 220.0f, 90.0f };
		const Vector2 firePos{ actionPanelPos.x + 36.0f,  actionPanelPos.y + 26.0f };
		const Vector2 adsPos{ actionPanelPos.x + 110.0f, actionPanelPos.y + 26.0f };
		const Vector2 reloadPos{ actionPanelPos.x + 184.0f, actionPanelPos.y + 26.0f };

		// バナー
		const Vector2 bannerPos{ 560.0f, 8.0f };
		const Vector2 bannerSize{ 160.0f, 40.0f };

		// 矢印
		const Vector2 arrowPos{ 640.0f - 16.0f, 58.0f };
		const Vector2 arrowSize{ 32.0f, 24.0f };

		// 残体数（上中央）
		const Vector2 remainPos{ 660.0f, 2.0f }; // もっと中央に出すなら {640,28}
	} L;
} // namespace


/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void HUDManager::Initialize()
{
	// 各種HUD要素の初期化
	InitNumberDrawers();

	// リロードサークル
	InitBars();

	// クイックバー6スロット
	InitQuickSlots();

	// 中央下クイックバー
	InitWaveAndArrow();

	// ガジェットアイコン
	InitGadgets();

	// 武器カテゴリアイコン
	InitCategoryIcons();

	// 右下アクションヒント
	InitActionHints();
}

void HUDManager::InitNumberDrawers()
{
	scoreDrawer_ = std::make_unique<NumberSpriteDrawer>(); scoreDrawer_->Initialize(texturePath_);		   // スコア表示用
	killDrawer_ = std::make_unique<NumberSpriteDrawer>(); killDrawer_->Initialize(texturePath_);		   // キル数表示用
	ammoDrawer_ = std::make_unique<NumberSpriteDrawer>(); ammoDrawer_->Initialize(texturePath_);		   // 弾薬数表示用
	hpDrawer_ = std::make_unique<NumberSpriteDrawer>(); hpDrawer_->Initialize(texturePath_);			   // HP表示用
	waveCurDrawer_ = std::make_unique<NumberSpriteDrawer>(); waveCurDrawer_->Initialize(texturePath_);	   // 現在Wave
	waveTotalDrawer_ = std::make_unique<NumberSpriteDrawer>(); waveTotalDrawer_->Initialize(texturePath_); // 総Wave
	remainDrawer_ = std::make_unique<NumberSpriteDrawer>(); remainDrawer_->Initialize(texturePath_);	   // 残り体数
}

void HUDManager::InitBars()
{
	hpBarBase_ = std::make_unique<Sprite>(); hpBarBase_->Initialize("white.png"); // グレー背景
	hpBarFill_ = std::make_unique<Sprite>(); hpBarFill_->Initialize("white.png"); // 緑バー
	reloadCircle_ = std::make_unique<ReloadCircle>(); reloadCircle_->Initialize("reload-circle.png"); // リロード円
}

void HUDManager::InitQuickSlots()
{
	// クイックバー6スロ
	for (auto& s : quickSlots_)
	{
		s.background = std::make_unique<Sprite>(); s.background->Initialize("white.png");
		s.icon = std::make_unique<Sprite>(); s.icon->Initialize("white.png");
		s.lock = std::make_unique<Sprite>(); s.lock->Initialize("white.png");
	}
}

void HUDManager::InitWaveAndArrow()
{
	waveBanner_ = std::make_unique<Sprite>(); waveBanner_->Initialize("white.png"); // ウェーブ黒バナー
	objectiveArrow_ = std::make_unique<Sprite>(); objectiveArrow_->Initialize("white.png"); // 目標矢印

	remainIcon_ = std::make_unique<Sprite>();
	remainIcon_->Initialize("icon/icon_enemy_person.png");  // ←好みで他のpngに
	remainIcon_->SetAnchorPoint({ 0.5f, 0.5f });
}

void HUDManager::InitGadgets()
{
	// ガジェット3スロ
	for (int i = 0; i < 3; ++i)
	{
		gadgetBg_[i] = std::make_unique<Sprite>();    gadgetBg_[i]->Initialize("white.png");
		gadgetIcon_[i] = std::make_unique<Sprite>();  gadgetIcon_[i]->Initialize("white.png");
	}
}

void HUDManager::InitCategoryIcons()
{
	// カテゴリアイコン
	auto makeIcon = [&](WeaponCategory cat, const char* path) {
		TextureManager::GetInstance()->LoadTexture(path);
		auto sp = std::make_unique<Sprite>(); sp->Initialize(path); sp->SetAnchorPoint({ 0.5f,0.5f });
		categoryIcons_[cat] = std::move(sp);
		};
	makeIcon(WeaponCategory::Primary, "icon/icon_primary.png"); // プライマリ
	makeIcon(WeaponCategory::Backup, "icon/icon_backup.png");	// バックアップ
	makeIcon(WeaponCategory::Melee, "icon/icon_melee.png");		// 近接
	makeIcon(WeaponCategory::Special, "icon/icon_special.png"); // スペシャル
	makeIcon(WeaponCategory::Sniper, "icon/icon_sniper.png");	// スナイパー
	makeIcon(WeaponCategory::Heavy, "icon/icon_heavy.png");		// ヘビー
	activeCategory_ = WeaponCategory::Primary; // 主要武器をデフォルト
}

void HUDManager::InitActionHints()
{
	actionPanel_ = std::make_unique<Sprite>(); actionPanel_->Initialize("white.png");
	actionPanel_->SetColor({ 0,0,0,0.35f });

	auto make = [&](ActionHint& h, const char* icon, const char* label) {
		TextureManager::GetInstance()->LoadTexture(icon);
		TextureManager::GetInstance()->LoadTexture(label);
		h.icon = std::make_unique<Sprite>(); h.icon->Initialize(icon);   h.icon->SetAnchorPoint({ 0.5f,0.5f });
		h.label = std::make_unique<Sprite>(); h.label->Initialize(label); h.label->SetAnchorPoint({ 0.5f,0.5f });
		};
	make(hintFire_, "icon/ui_mouse_left.png", "icon/label_FIRE.png");
	make(hintAds_, "icon/ui_mouse_right.png", "icon/label_ADS.png");
	make(hintReload_, "icon/ui_key_R.png", "icon/label_RELOAD.png");
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void HUDManager::Update()
{
	// リロード円の更新
	if (reloadCircle_) reloadCircle_->Update();

	// 各要素の更新
	UpdateHPBar();

	// クイックスロットのレイアウト更新
	UpdateQuickSlotsLayout();

	// カテゴリアイコン更新
	UpdateCategoryBadge();

	// 右下アクションヒント更新
	UpdateActionHints();

	// 中央下ウェーブバナー、矢印更新
	UpdateWaveBanner();

	// 目標矢印、ガジェット更新
	UpdateObjectiveArrow();

	// 右側ガジェット更新
	UpdateGadgets();

	// 残り敵数の数字を出している座標に合わせて、左にアイコンを置く
	const Vector2 remainPos = { 640.0f, 28.0f };     // 今の数字の位置に合わせて
	const float   baseSize = 26.0f;

	// ポップ演出（敵を倒した瞬間に少し大きく）
	if (remainIconPopT_ > 0.0f) remainIconPopT_ -= 1.0f / 60.0f;
	float pop = (remainIconPopT_ > 0.0f) ? (1.0f + 0.35f * (remainIconPopT_ / 0.25f)) : 1.0f;

	// 3体以下なら点滅で注意喚起
	static float t = 0.0f; t += 1.0f / 60.0f;
	float alpha = (enemiesRemain_ <= 3) ? (0.65f + 0.35f * 0.5f * (1.0f + std::sin(t * 10.0f)))
		: 1.0f;

	remainIcon_->SetSize({ baseSize * pop, baseSize * pop });
	remainIcon_->SetPosition({ remainPos.x - 28.0f, remainPos.y + 2.0f });
	remainIcon_->SetColor({ 1,1,1,alpha });
	remainIcon_->Update();
}

void HUDManager::UpdateHPBar()
{
	hpBarBase_->SetPosition(L.hpPosLT);
	hpBarBase_->SetSize(L.hpSize);
	hpBarBase_->SetColor({ 0,0,0,0.65f });
	hpBarBase_->Update();

	float hpRatio = std::clamp(hp_ / std::max(1.0f, maxHP_), 0.0f, 1.0f);
	hpBarFill_->SetPosition(L.hpPosLT);
	hpBarFill_->SetSize({ L.hpSize.x * hpRatio, L.hpSize.y });
	hpBarFill_->SetColor({ 0.15f,0.9f,0.2f,1 });
	hpBarFill_->Update();
}

void HUDManager::UpdateQuickSlotsLayout()
{
	for (int i = 0; i < 6; ++i)
	{
		Vector2 pos{ L.slotsStartX + i * (L.slotSize.x + L.slotGap), L.slotsY };
		auto& s = quickSlots_[i];
		s.background->SetPosition(pos);
		s.background->SetSize(L.slotSize);
		bool owned = (i < numWeapons_);
		Vector4 col = owned ? Vector4{ 0,0,0,0.6f } : Vector4{ 0.2f,0.2f,0.2f,0.6f };
		if (i == activeSlot_) col = { 0.0f,0.5f,0.1f,0.8f };
		s.background->SetColor(col); s.background->Update();

		s.icon->SetPosition({ pos.x + 8, pos.y + 8 });
		s.icon->SetSize({ L.slotSize.x - 16, L.slotSize.y - 22 });
		s.icon->SetColor({ 0.25f,0.45f,0.9f, owned ? 1.0f : 0.35f });
		s.icon->Update();

		if (!owned)
		{
			s.lock->SetPosition({ pos.x + L.slotSize.x / 2 - 10, pos.y + L.slotSize.y / 2 - 10 });
			s.lock->SetSize({ 20,20 }); s.lock->SetColor({ 1,1,1,0.85f }); s.lock->Update();
		}
	}
}

void HUDManager::UpdateCategoryBadge()
{
	float ax = L.slotsStartX + activeSlot_ * (L.slotSize.x + L.slotGap);
	float ay = L.slotsY;
	auto it = categoryIcons_.find(activeCategory_);
	if (it != categoryIcons_.end()) {
		auto* ico = it->second.get();
		ico->SetSize({ 36.0f, 36.0f });
		ico->SetPosition({ ax + 18.0f, ay - 14.0f });
		ico->Update();
	}
}

void HUDManager::UpdateActionHints()
{
	actionPanel_->SetPosition(L.actionPanelPos);
	actionPanel_->SetSize(L.actionPanelSize);
	actionPanel_->Update();

	hintFire_.position = L.firePos;
	hintAds_.position = L.adsPos;
	hintReload_.position = L.reloadPos;

	auto set = [&](ActionHint& h, const Vector2& size) {
		h.icon->SetPosition(h.position);
		h.icon->SetSize(size);
		h.label->SetPosition({ h.position.x, h.position.y + 28.0f });
		h.label->SetSize({ 100.0f, 22.0f });
		h.icon->Update(); h.label->Update();
		};
	set(hintFire_, { 34,34 });
	set(hintAds_, { 34,34 });
	set(hintReload_, { 34,34 });

	actionPulseT_ += 1.0f / 60.0f;
	float pulse = 0.5f + 0.5f * std::sin(actionPulseT_ * 10.0f);

	// 色反映
	hintFire_.icon->SetColor(stateCanFire_ ? Vector4{ 1,1,1,1 } : Vector4{ 0.5f,0.5f,0.5f,0.6f });
	hintAds_.icon->SetColor(stateIsADS_ ? Vector4{ 0.6f,1.0f,0.6f,1.0f } : Vector4{ 1,1,1,1 });
	bool pulseReload = stateReloading_ || (ammoInClip_ <= 0 && ammoReserve_ > 0);
	float ra = pulseReload ? (0.6f + 0.4f * pulse) : 0.6f;
	hintReload_.icon->SetColor({ 1,1,1,ra });

	hintFire_.label->SetColor({ 1,1,1,0.85f });
	hintAds_.label->SetColor({ 1,1,1,0.85f });
	hintReload_.label->SetColor({ 1,1,1,0.85f });
}

void HUDManager::UpdateWaveBanner()
{
	waveBanner_->SetPosition(L.bannerPos);
	waveBanner_->SetSize(L.bannerSize);
	waveBanner_->SetColor({ 0,0,0,0.65f });
	waveBanner_->Update();
}

void HUDManager::UpdateObjectiveArrow()
{
	arrowTimer_ += 1.0f / 60.0f;
	float a1 = 0.5f + 0.5f * std::sin(arrowTimer_ * 6.0f);
	objectiveArrow_->SetPosition(L.arrowPos);
	objectiveArrow_->SetSize(L.arrowSize);
	objectiveArrow_->SetColor({ 0.6f,1.0f,0.2f, 0.25f + 0.75f * a1 });
	objectiveArrow_->Update();
}

void HUDManager::UpdateGadgets()
{
	for (int i = 0; i < 3; ++i)
	{
		Vector2 pos{ 1220.0f, 240.0f + i * 76.0f };
		gadgetBg_[i]->SetPosition(pos);
		gadgetBg_[i]->SetSize({ 60,60 });
		gadgetBg_[i]->SetColor({ 0,0,0,0.45f }); gadgetBg_[i]->Update();

		gadgetIcon_[i]->SetPosition({ pos.x + 6, pos.y + 6 });
		gadgetIcon_[i]->SetSize({ 48,48 });
		gadgetIcon_[i]->SetColor({ 0.8f,0.1f,0.1f,0.55f }); gadgetIcon_[i]->Update();
	}
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void HUDManager::Draw()
{
	DrawAmmoNumbers();
	waveBanner_->Draw();
	DrawWaveRemainOnly();
	//objectiveArrow_->Draw();
	DrawHPAndReload();
	DrawActionHints();
	DrawQuickSlots();
	DrawCategoryBadge();
	//DrawGadgets();
	DrawDebugHUD();

	remainIcon_->Draw();           // ← 先にアイコン
	remainDrawer_->Reset();
	remainDrawer_->DrawNumberCentered(enemiesRemain_, L.remainPos);
}

void HUDManager::DrawAmmoNumbers()
{
	ammoDrawer_->Reset();
	ammoDrawer_->DrawNumberRightAligned(ammoInClip_, { 1060.0f, 620.0f });
	ammoDrawer_->DrawNumberRightAligned(ammoReserve_, { 1140.0f, 620.0f });
}

void HUDManager::DrawWaveRemainOnly()
{
	remainDrawer_->Reset();
	remainDrawer_->DrawNumberCentered(enemiesRemain_, L.remainPos);
}

void HUDManager::DrawHPAndReload()
{
	hpBarBase_->Draw();
	hpBarFill_->Draw();
	if (reloadCircle_) reloadCircle_->Draw();
}

void HUDManager::DrawActionHints()
{
	actionPanel_->Draw();
	hintFire_.icon->Draw();   hintFire_.label->Draw();
	hintAds_.icon->Draw();    hintAds_.label->Draw();
	hintReload_.icon->Draw(); hintReload_.label->Draw();
}

void HUDManager::DrawQuickSlots()
{
	for (int i = 0; i < 6; ++i)
	{
		quickSlots_[i].background->Draw();
		quickSlots_[i].icon->Draw();
		if (i >= numWeapons_) quickSlots_[i].lock->Draw();
	}
}

void HUDManager::DrawCategoryBadge()
{
	auto it = categoryIcons_.find(activeCategory_);
	if (it != categoryIcons_.end()) it->second->Draw();
}

void HUDManager::DrawGadgets()
{
	for (int i = 0; i < 3; ++i) { gadgetBg_[i]->Draw(); gadgetIcon_[i]->Draw(); }
}


/// -------------------------------------------------------------
///				　			デバッグ用HUDの描画
/// -------------------------------------------------------------
void HUDManager::DrawDebugHUD()
{
#ifdef _DEBUG
	// リセット
	hpDrawer_->Reset();

	// HPの描画
	int percent = static_cast<int>((static_cast<float>(hp_) / maxHP_) * 100.0f);
	hpDrawer_->DrawNumberLeftAligned(percent, { 85.0f, 620.0f });
#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　			リロード中の円を表示
/// -------------------------------------------------------------
void HUDManager::SetReloading(bool isReloading, float progress)
{
	if (reloadCircle_)
	{
		reloadCircle_->SetVisible(isReloading);
		reloadCircle_->SetProgress(progress);
	}
}

void HUDManager::SetWaveInfo(int currentWave, int totalWaves)
{
	waveCur_ = std::max(1, currentWave);
	waveTotal_ = std::max(1, totalWaves);
}

void HUDManager::SetEnemiesRemaining(int remaining)
{
	if (prevEnemiesRemain_ < 0) prevEnemiesRemain_ = remaining;
	if (remaining < prevEnemiesRemain_) {
		remainIconPopT_ = 0.25f;  // 0.25秒だけポップ
	}
	prevEnemiesRemain_ = remaining;
	enemiesRemain_ = std::max(0, remaining);
}

void HUDManager::SetActiveWeaponIndex(int idx)
{
	int maxOwned = std::max(0, numWeapons_ - 1);
	activeSlot_ = std::clamp(idx, 0, std::min(5, maxOwned));
}

void HUDManager::SyncWeaponSlots(int numWeapons)
{
	numWeapons_ = std::clamp(numWeapons, 1, 6);
	if (activeSlot_ >= numWeapons_) activeSlot_ = numWeapons_ - 1;
}
