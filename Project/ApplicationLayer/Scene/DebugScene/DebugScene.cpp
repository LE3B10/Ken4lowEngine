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

	input_->SetLockCursor(true);
	input_->SetCursorVisible(false);

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	player_ = std::make_unique<Player>();
	player_->SetCollisionManager(collisionManager_.get());
	player_->Initialize();

	collisionManager_->AddCollider(player_.get());

	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(collisionManager_.get());

	// ★Playerへ渡す
	player_->SetBulletManager(bulletManager_.get());

	enemy_ = std::make_unique<Enemy>();
	enemy_->Initialize();
	enemy_->SetTarget(player_.get());
	enemy_->SetBulletManager(bulletManager_.get());
	collisionManager_->AddCollider(enemy_.get());
}

void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

	player_->Update(deltaTime);

	bulletManager_->Update(deltaTime);

	enemy_->Update(deltaTime);

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();

	if (enemy_ && enemy_->IsRemovable())
	{
		collisionManager_->RemoveCollider(enemy_.get());
	}
}

void DebugScene::Draw3DObjects()
{
	player_->Draw();
	bulletManager_->Draw();
	enemy_->Draw();
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


#pragma endregion
}

void DebugScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	enemy_.reset();
	bulletManager_.reset();
	player_.reset();
	collisionManager_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	player_->DrawImGui();

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

		player_->SetDebugCamera(isDebugCamera_);
		input_->SetLockCursor(!isDebugCamera_);
		input_->SetCursorVisible(isDebugCamera_);
	}
}