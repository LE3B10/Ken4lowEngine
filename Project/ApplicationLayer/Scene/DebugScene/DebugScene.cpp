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

namespace K4E = ::Ken4lowEngine;

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
	K4E::DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	boss_ = std::make_unique<Boss>();
	boss_->Initialize();

	animModel_ = std::make_unique<K4E::AnimationModel>();
	animModel_->Initialize("human.gltf");

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	player_ = std::make_unique<DummyPlayer>();
	player_->Initialize();
	collisionManager_->AddCollider(player_.get());

	for (int i = 0; i < 5; ++i)
	{
		auto enemy = std::make_unique<DummyEnemy>();
		enemy->Initialize();
		enemy->SetCenterPosition({ 0.0f, 0.0f, 5.0f + i * 4.0f });
		collisionManager_->AddCollider(enemy.get());
		enemies_.emplace_back(std::move(enemy));
	}

	fadeManager_ = std::make_unique<FadeManager>();
	fadeManager_->Initialize();

	crackDemoSprite_ = std::make_unique<K4E::Sprite>();
	crackDemoSprite_->Initialize("uvChecker.png");

	// 下のSprite（例：uvCheckerなど）
	blockSprite_ = std::make_unique<K4E::Sprite>();
	blockSprite_->Initialize("uvChecker.png");
	blockSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	blockSprite_->SetPosition({ 0.0f, 0.0f });
	blockSprite_->SetSize({ 512.0f, 512.0f });
	blockSprite_->Update();

	// 上のひび割れ（CrackAtlas）
	crackOverlaySprite_ = std::make_unique<K4E::Sprite>();
	crackOverlaySprite_->Initialize("CrackAtlas.png"); // ←自作スプライトシート
	crackOverlaySprite_->SetAnchorPoint({ 0.0f, 0.0f });
	crackOverlaySprite_->SetPosition(blockSprite_->GetPosition());
	crackOverlaySprite_->SetSize(blockSprite_->GetSize());
	crackOverlaySprite_->SetColor({ 1,1,1,1 }); // 透過PNGならこれでOK
	crackOverlaySprite_->Update();


	K4E::SpriteDebrisEmitter::Params p;
	p.maxParticles = 256;
	p.groundY = 520.0f; // ブロックの下に合わせて調整
	debris_ = std::make_unique<K4E::SpriteDebrisEmitter>();
	debris_->Initialize("DebrisAtlas.png", p);

	auto* mgr = K4E::GpuParticleManager::GetInstance();

	// ------------------------------------------------------------
	//  Spriteデバッグ用：HitSpark（GPU K4E::Sprite / 21タイプの中の1つ）
	// ------------------------------------------------------------
	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "circle2.png";
		info.radius = 0.2f;

		// ループ発生は無効（必要ならここを設定）
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		// ★差別化：Spriteモード
		info.kind = K4E::GpuParticleKind::Sprite;

		// ★Spriteタイプ（21個のうち）
		info.spriteType = K4E::GpuParticleType::HitSpark;

		// ★flags（下位16bit）：通常はCameraでOK
		info.billboardFlags = K4E::BillboardMode::Camera;

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
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "circle2.png";
		info.radius = 0.15f;

		// ループは無効（キーでバーストして確認）
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		// ★差別化：Ribbonモード
		info.kind = K4E::GpuParticleKind::Ribbon;

		// ★Ribbonタイプ（Emitter内の enum）
		info.ribbonType = K4E::GpuRibbonType::BulletTracer;

		// ★flags（下位16bit）：K4E::Camera/YAxisなど（Ribbonフラグは使わない運用）
		info.billboardFlags = K4E::BillboardMode::Camera;

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

	animModel_->Update();

	player_->Update();

	for (auto& enemy : enemies_)
	{
		enemy->Update();
	}


	// --- 弾の生成（SPACEで発射） ---
	if (input_->TriggerKey(DIK_SPACE))
	{
		// 生成位置：プレイヤーの少し前
		K4E::Vector3 start = player_->GetCenterPosition() + K4E::Vector3{ 0.0f, 0.0f, 1.0f };
		K4E::Vector3 vel = { 0.0f, 0.0f, 250.0f }; // 1フレームあたりの移動量

		auto bullet = std::make_unique<DummyBullet>();
		bullet->Initialize(start, vel, 1);
		collisionManager_->AddCollider(bullet.get());
		bullets_.emplace_back(std::move(bullet));
	}

	// --- 弾の更新 ---
	for (auto& b : bullets_)
	{
		b->Update();
	}

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();


	// --- 消滅した弾を削除（Exitが解決されるまで1フレーム猶予） ---
	for (auto it = bullets_.begin(); it != bullets_.end(); )
	{
		if ((*it)->IsRemovable())
		{
			collisionManager_->RemoveCollider(it->get());
			it = bullets_.erase(it);
		}
		else
		{
			++it;
		}
	}


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
			fracture_ = std::make_unique<K4E::SpriteFractureEffect>();
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
	K4E::Vector2 leftTopPx = { crackFrameSizePx_.x * stage, 0.0f };
	K4E::Vector2 sizePx = { crackFrameSizePx_.x, crackFrameSizePx_.y };

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
			K4E::Vector2 center = {
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

	auto* mgr = K4E::GpuParticleManager::GetInstance();

	// 位置追従（ボス中心に合わせる）
	const K4E::Vector3 bossPos = boss_->GetCenterPosition(); // 実関数名に合わせて
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

	animModel_->Draw();

	player_->Draw();

	for (auto& enemy : enemies_)
	{
		enemy->Draw();
	}

	for (auto& b : bullets_)
	{
		b->Draw();
	}

#ifdef _DEBUG
	// ワイヤーフレームの描画
	K4E::Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });

	collisionManager_->Draw();
#endif // _DEBUG
}

void DebugScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();

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
	player_.reset();

	for (auto& enemy : enemies_)
	{
		enemy.reset();
	}

	for (auto& b : bullets_)
	{
		b.reset();
	}
	bullets_.clear();

	collisionManager_.reset();
	animModel_.reset();
	boss_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	boss_->DrawImGui();

	animModel_->DrawImGui();

	fadeManager_->DrawImGui();

	/// ---------- GPUパーティクルデバッグ ---------- ///
	K4E::GpuParticleManager::GetInstance()->DrawImGui();

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
		K4E::Object3DCommon::GetInstance()->SetDebugCamera(!K4E::Object3DCommon::GetInstance()->GetDebugCamera());
		K4E::Wireframe::GetInstance()->SetDebugCamera(!K4E::Wireframe::GetInstance()->GetDebugCamera());
		K4E::GpuParticleManager::GetInstance()->SetDebugCameraEnabled(!isDebugCamera_);
		isDebugCamera_ = !isDebugCamera_;
	}
}