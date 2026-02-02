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

	// ボスの更新
	boss_->Update(dxCommon_->GetFPSCounter().GetDeltaTime());

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
}

void DebugScene::Finalize()
{
	boss_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	boss_->DrawImGui();

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