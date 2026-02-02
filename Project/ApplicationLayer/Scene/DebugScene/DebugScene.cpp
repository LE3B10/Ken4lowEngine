#define NOMINMAX
#include "DebugScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"
#include "AudioManager.h"
#include <SceneManager.h>
#include "LevelLoader.h"

#include <TextureManager.h>

#include <algorithm>
#include <chrono>

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI
#include <GpuParticleManager.h>

#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"
#include "WeaponMasterDataWriter.h"
#include <filesystem>

namespace
{
	static const char* CategoryFolder(EWeaponCategory c)
	{
		switch (c)
		{
		case EWeaponCategory::Primary: return "primary";
		case EWeaponCategory::Backup:  return "backup";
		case EWeaponCategory::Melee:   return "melee";
		case EWeaponCategory::Special: return "special";
		case EWeaponCategory::Sniper:  return "sniper";
		case EWeaponCategory::Heavy:   return "heavy";
		default: return "unknown";
		}
	}

	static std::string RarityToStr(EWeaponRarity r)
	{
		switch (r)
		{
		case EWeaponRarity::Common: return "common";
		case EWeaponRarity::Rare: return "rare";
		case EWeaponRarity::Epic: return "epic";
		case EWeaponRarity::Legendary: return "legendary";
		case EWeaponRarity::Mythical: return "mythical";
		default: return "common";
		}
	}

	static std::string AmmoToStr(EAmmoType a)
	{
		switch (a)
		{
		case EAmmoType::Default: return "default";
		case EAmmoType::Energy: return "energy";
		case EAmmoType::Explosive: return "explosive";
		case EAmmoType::None: return "none";
		default: return "default";
		}
	}

	static std::string CategoryToStr(EWeaponCategory c)
	{
		return CategoryFolder(c); // 同じ文字でOK
	}

	static std::string SanitizeFileStem(std::string s)
	{
		// Windowsで禁止の文字を _ に置換
		const char* bad = "\\/:*?\"<>|";
		for (char& c : s)
		{
			if ((unsigned char)c < 32) c = '_';
			for (const char* p = bad; *p; ++p)
				if (c == *p) { c = '_'; break; }
		}

		// 末尾の . や空白はWindowsで危険なので削る
		while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();

		if (s.empty()) s = "Weapon";
		return s;
	}
}

void DebugScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	boss_ = std::make_unique<Boss>();
	boss_->Initialize();

	fadeManager_ = std::make_unique<FadeManager>();
	fadeManager_->Initialize();

	crackDemoSprite_ = std::make_unique<Sprite>();
	crackDemoSprite_->Initialize("uvChecker.png");

	// 下のSprite（例：uvCheckerなど）
	blockSprite_ = std::make_unique<Sprite>();
	blockSprite_->Initialize("uvChecker.png");
	blockSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	blockSprite_->SetPosition({ 0.0f, 0.0f });
	blockSprite_->SetSize({ 512.0f, 512.0f });
	blockSprite_->Update();

	// 上のひび割れ（CrackAtlas）
	crackOverlaySprite_ = std::make_unique<Sprite>();
	crackOverlaySprite_->Initialize("CrackAtlas.png"); // ←自作スプライトシート
	crackOverlaySprite_->SetAnchorPoint({ 0.0f, 0.0f });
	crackOverlaySprite_->SetPosition(blockSprite_->GetPosition());
	crackOverlaySprite_->SetSize(blockSprite_->GetSize());
	crackOverlaySprite_->SetColor({ 1,1,1,1 }); // 透過PNGならこれでOK
	crackOverlaySprite_->Update();


	SpriteDebrisEmitter::Params p;
	p.maxParticles = 256;
	p.groundY = 520.0f; // ブロックの下に合わせて調整
	debris_ = std::make_unique<SpriteDebrisEmitter>();
	debris_->Initialize("DebrisAtlas.png", p);

	auto* mgr = GpuParticleManager::GetInstance();

	// ------------------------------------------------------------
	//  Spriteデバッグ用：HitSpark（GPU Sprite / 21タイプの中の1つ）
	// ------------------------------------------------------------
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "circle2.png";
		info.radius = 0.2f;

		// ループ発生は無効（必要ならここを設定）
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		// ★差別化：Spriteモード
		info.kind = GpuParticleKind::Sprite;

		// ★Spriteタイプ（21個のうち）
		info.spriteType = GpuParticleType::HitSpark;

		// ★flags（下位16bit）：通常はCameraでOK
		info.billboardFlags = BillboardMode::Camera;

		if (!mgr->GetEmitter("Dbg_HitSpark"))
		{
			if (auto* e = mgr->CreateEmitter("Dbg_HitSpark", info))
			{
				e->SetPosition({ 0.0f, 1.0f, 0.0f });
			}
		}
	}

	// ------------------------------------------------------------
	//  Ribbonデバッグ用：BulletTracer（Ribbonモード）
	// ------------------------------------------------------------
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "circle2.png";
		info.radius = 0.15f;

		// ループは無効（キーでバーストして確認）
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		// ★差別化：Ribbonモード
		info.kind = GpuParticleKind::Ribbon;

		// ★Ribbonタイプ（Emitter内の enum）
		info.ribbonType = GpuRibbonType::BulletTracer;

		// ★flags（下位16bit）：Camera/YAxisなど（Ribbonフラグは使わない運用）
		info.billboardFlags = BillboardMode::Camera;

		if (!mgr->GetEmitter("Dbg_Ribbon"))
		{
			if (auto* e = mgr->CreateEmitter("Dbg_Ribbon", info))
			{
				e->SetPosition({ 0.0f, 1.0f, 0.0f });
			}
		}
	}
}

void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

	// ボスの更新
	boss_->Update(deltaTime);

	fadeManager_->Update(deltaTime);

	// 画面左に大きく出す（好きなサイズでOK）
	crackDemoSprite_->Update();

	// --- ひび割れ表示（元スプライト） ---
	if (!fractureActive_)
	{
		crackDemoSprite_->SetCrack(crackEnable_, crackProgress_);
		crackDemoSprite_->SetCrackParams(crackScale_, crackThickness_, crackIntensity_, hitUV_);
		crackDemoSprite_->SetColor({ 1,1,1,1 });
		crackDemoSprite_->Update();

		// crackProgress が 1 になったら分解開始
		if (crackEnable_ && crackProgress_ >= 0.999f)
		{
			// 破片生成（細かいほど砕ける：8x8, 16x16 など）
			fracture_ = std::make_unique<SpriteFractureEffect>();
			fracture_->Initialize(*crackDemoSprite_, 16, 16);
			fracture_->SetHitUV(hitUV_);
			fracture_->SetGravity(2600.0f);
			fracture_->SetImpulse(900.0f);
			fracture_->SetLifetime(1.6f);
			fracture_->SetFadeOut(0.55f);

			fractureProgress_ = 0.0f;
			fractureActive_ = true;
		}
	}
	else
	{
		// 分解中：0→1 で剥がれを進める（速さは調整）
		fractureProgress_ = std::min(1.0f, fractureProgress_ + deltaTime * 3.0f);
		fracture_->SetHitUV(hitUV_);
		fracture_->SetProgress(fractureProgress_);
		fracture_->Update(deltaTime);

		// 元スプライトはフェードアウト（描いてもいいし、描かなくてもOK）
		float a = std::max(0.0f, 1.0f - fractureProgress_ * 2.0f);
		crackDemoSprite_->SetColor({ 1,1,1,a });
		crackDemoSprite_->Update();

		// 全部消えたら終了（必要ならリセット）
		if (fracture_->IsFinished())
		{
			fractureActive_ = false;
			crackProgress_ = 0.0f; // 次のテスト用に戻す
		}
	}

	// リセットキー（任意）
	if (input_->TriggerKey(DIK_R))
	{
		fractureActive_ = false;
		fracture_->Reset();
		crackProgress_ = 0.0f;
	}

	// 自動再生
	if (atlasAuto_)
	{
		atlasTime_ += deltaTime;
		int stage = (int)(atlasTime_ * atlasFps_) % kCrackFrames;
		breakProgress_ = (float)stage / (kCrackFrames - 1); // 表示用（任意）
	}
	else
	{
		// 手動（breakProgress_ による段階）
		// 0..1 → 0..9
		// ※1.0ちょうどで10にならないように0.9999
	}

	int stage = (int)(std::clamp(breakProgress_, 0.0f, 0.9999f) * kCrackFrames);
	stage = std::clamp(stage, 0, kCrackFrames - 1);

	// 横10枚×縦1枚
	Vector2 leftTopPx = { crackFrameSizePx_.x * stage, 0.0f };
	Vector2 sizePx = { crackFrameSizePx_.x, crackFrameSizePx_.y };

	crackOverlaySprite_->SetUVRect(leftTopPx, sizePx);

	// 下Spriteと同じ変形
	crackOverlaySprite_->SetPosition(blockSprite_->GetPosition());
	crackOverlaySprite_->SetSize(blockSprite_->GetSize());
	crackOverlaySprite_->SetRotation(blockSprite_->GetRotation());
	crackOverlaySprite_->SetAnchorPoint(blockSprite_->GetAnchorPoint());

	// テスト中は stage0 でも表示したいので atlasHideAtZero_ で制御
	float alpha = 1.0f;
	if (atlasHideAtZero_ && stage == 0) { alpha = 0.0f; }
	crackOverlaySprite_->SetColor({ 1,1,1,alpha });

	blockSprite_->Update();
	crackOverlaySprite_->Update();


	// stageが進んだ瞬間に欠片をバースト
	if (debrisEnable_)
	{
		if (stage > prevCrackStage_)
		{
			// ブロックの中心付近に出す（座標はあなたのspriteに合わせて調整）
			Vector2 center = {
				blockSprite_->GetPosition().x + blockSprite_->GetSize().x * 0.5f,
				blockSprite_->GetPosition().y + blockSprite_->GetSize().y * 0.5f
			};

			int count = debrisBurstBase_ + stage * 2;
			debris_->Burst(center, count);
		}
	}
	prevCrackStage_ = stage;

	// 欠片更新
	debris_->Update(deltaTime);

	auto* mgr = GpuParticleManager::GetInstance();

	// 位置追従（ボス中心に合わせる）
	const Vector3 bossPos = boss_->GetCenterPosition(); // 実関数名に合わせて
	if (auto* e = mgr->GetEmitter("Dbg_HitSpark"))
	{
		e->SetPosition(bossPos);
	}
	if (auto* e = mgr->GetEmitter("Dbg_Ribbon"))
	{
		e->SetPosition(bossPos);
	}

	// ------------------------------------------------------------
	// デバッグ用：キーで複数射出（同フレーム合算も確認できる）
	// ------------------------------------------------------------
	// 1キー：HitSpark を 50個バースト
	if (input_->TriggerKey(DIK_1))
	{
		mgr->BurstEmitter("Dbg_HitSpark", 50);
	}

	// 2キー：Ribbon を 10個バースト（疑似リボン/モード分離の確認用）
	if (input_->TriggerKey(DIK_2))
	{
		mgr->BurstEmitter("Dbg_Ribbon", 10);
	}

	// 3キー：同フレームに「複数回」射出して合算確認（50 + 50 = 100）
	if (input_->TriggerKey(DIK_3))
	{
		mgr->BurstEmitter("Dbg_HitSpark", 50);
		mgr->BurstEmitter("Dbg_HitSpark", 50);
	}
}

void DebugScene::Draw3DObjects()
{
	boss_->Draw();

#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });
#endif // _DEBUG
}

void DebugScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	fadeManager_->Draw2DSprites();

	//// 元スプライト（ひび割れ）
	//if (crackDemoSprite_) { crackDemoSprite_->Draw(); }

	//// 破片（分解中だけ）
	//if (fractureActive_) 
	//	fracture_->Draw();


	//blockSprite_->Draw();        // 下
	//crackOverlaySprite_->Draw(); // 上（透明PNGの黒線が乗る）

	//debris_->Draw();

#pragma endregion
}

void DebugScene::Finalize()
{
	debris_.reset();
	crackOverlaySprite_.reset();
	blockSprite_.reset();
	fracture_.reset();
	crackDemoSprite_.reset();
	fadeManager_.reset();
	boss_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	boss_->DrawImGui();

	fadeManager_->DrawImGui();

	ImGui::Begin("Debug Crack");

	ImGui::Checkbox("Crack Enable", &crackEnable_);
	ImGui::SliderFloat("Crack Progress", &crackProgress_, 0.0f, 1.0f);
	ImGui::SliderFloat("Crack Scale", &crackScale_, 1.0f, 60.0f);
	ImGui::SliderFloat("Crack Thickness", &crackThickness_, 0.001f, 0.08f);
	ImGui::SliderFloat("Crack Intensity", &crackIntensity_, 0.0f, 2.0f);
	ImGui::SliderFloat2("HitUV", &hitUV_.x, 0.0f, 1.0f);

	// “Updateに渡ってる値” を見える化（効いてるか即わかる）
	ImGui::Separator();
	ImGui::Text("Applied (member) progress = %.3f", crackProgress_);

	ImGui::SeparatorText("Crack Atlas (Minecraft-like)");

	ImGui::Checkbox("Atlas Auto", &atlasAuto_);
	ImGui::SliderFloat("Atlas FPS", &atlasFps_, 1.0f, 30.0f);
	ImGui::Checkbox("Hide Stage0", &atlasHideAtZero_);

	if (!atlasAuto_)
	{
		ImGui::SliderFloat("Break Progress (Atlas)", &breakProgress_, 0.0f, 1.0f);
	}

	ImGui::SliderFloat2("Frame Size(px)", &crackFrameSizePx_.x, 1.0f, 512.0f);

	ImGui::SeparatorText("Debris (Sprite)");
	ImGui::Checkbox("Debris Enable", &debrisEnable_);
	ImGui::SliderInt("Burst Base", &debrisBurstBase_, 0, 60);

	auto& dp = debris_->GetParams();
	ImGui::SliderFloat("Gravity", &dp.gravity, 0.0f, 6000.0f);
	ImGui::SliderFloat("Bounce", &dp.bounce, 0.0f, 0.9f);
	ImGui::SliderFloat("Min Life", &dp.minLife, 0.05f, 2.0f);
	ImGui::SliderFloat("Max Life", &dp.maxLife, 0.05f, 2.0f);
	ImGui::SliderFloat("Min Speed", &dp.minSpeed, 0.0f, 1200.0f);
	ImGui::SliderFloat("Max Speed", &dp.maxSpeed, 0.0f, 1800.0f);
	ImGui::SliderFloat("GroundY", &dp.groundY, 0.0f, 1200.0f);

	ImGui::End();

	/// ---------- GPUパーティクルデバッグ ---------- ///
	GpuParticleManager::GetInstance()->DrawImGui();

	/// ---------- 武器マスターデータエディタ ---------- ///
	static WeaponMasterDataDatabase weaponDB;
	static WeaponMasterDataEditor weaponEditor;
	static WeaponEditorHooks hooks;
	static bool initialized = false;
	static int32_t lastAppliedID = 0;

	if (!initialized)
	{
		initialized = true;

		// まだ保存/再読込はしないので一旦空実装でOK
		hooks.SaveAll = [&]()
			{
				std::string err;
				const std::filesystem::path outRoot = "Resources/JSON/weapons";
				WeaponMasterDataWriter::SaveAllByCategory(weaponDB, outRoot, &err);
			};
		hooks.RequestReloadFocus = [](int32_t) {};
		hooks.RebuildLoadout = []() {};

		// Applyされたら「最後のID」を更新（動作確認）
		hooks.ApplyToRuntimeIfCurrent =
			[&](int32_t weaponID, const FWeaponMasterData&)
			{
				lastAppliedID = weaponID;
			};

		// 削除はDBから消すだけ（ファイル削除は後で）
		hooks.RequestDelete =
			[&](int32_t weaponID)
			{
				std::string err;
				const std::filesystem::path outRoot = "Resources/JSON/weapons";

				// まずはディスク上のjsonファイルを削除
				WeaponMasterDataWriter::DeleteFilesByWeaponID(outRoot, weaponID, &err);

				// DBから削除
				weaponDB.RemoveByID(weaponID);
			};

		// 追加予約は今は使わないなら空でOK
		hooks.RequestAdd = [](const std::string&, int32_t) {};

		// 初期データを2つだけ作る（任意）
		weaponDB.Clear();
	}

	ImGui::Begin("武器マスターデータエディタ");
	if (ImGui::CollapsingHeader("Weapon Master Editor", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Count: %zu", weaponDB.Size());
		ImGui::Text("Last Applied ID: %d", lastAppliedID);
		ImGui::Separator();

		weaponEditor.DrawImGui(weaponDB, hooks);
	}
	ImGui::End();

#endif // USE_IMGUI

}

void DebugScene::UpdateDebug()
{
	if (input_->TriggerKey(DIK_F12))
	{
		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		GpuParticleManager::GetInstance()->SetDebugCameraEnabled(!isDebugCamera_);
		isDebugCamera_ = !isDebugCamera_;
	}
}