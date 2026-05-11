#define NOMINMAX
#include "GamePlayDebugTools.h"

#include "GamePlayFlow.h"
#include "GamePlayWorld.h"

#include <Input.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include "LightManager.h"

#include "AudioManager.h"
#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"
#include "WeaponMasterDataWriter.h"
#include "TextureManager.h"

#include <filesystem>
#include <system_error>

#ifdef USE_IMGUI
#include <imgui.h>
#include <Editor/EditorWindowManager.h>
#endif
#include <Editor/EditorPlayController.h>


void GamePlayDebugTools::Initialize()
{
	// Debug Freeze状態はGamePlay開始ごとにToolbar表示をOFFへ初期化する。
	K4E::EditorPlayController::GetInstance()->SetDebugFreezeEnabled(false);
}

void GamePlayDebugTools::Finalize()
{
	// GamePlay終了時は完全停止デバッグ表示を残さない。
	K4E::EditorPlayController::GetInstance()->SetDebugFreezeEnabled(false);
}

bool GamePlayDebugTools::HandleFreezeToggle(K4E::Input* input, GamePlayFlow* flow)
{
#ifdef _DEBUG
	if (input && input->TriggerRawKey(DIK_F10))
	{
		if (isImGuiFreeze_)
		{
			ExitImGuiFreeze(input);
		}
		else
		{
			EnterImGuiFreeze(input, flow);
		}
		return true;
	}
#else
	(void)input;
	(void)flow;
#endif
	return false;
}

void GamePlayDebugTools::EnterImGuiFreeze(K4E::Input* input, GamePlayFlow* flow)
{
	if (isImGuiFreeze_) { return; }

	isImGuiFreeze_ = true;
	// F10の完全停止はEditor入力キャプチャとは別のDebug Freezeとして表示する。
	K4E::EditorPlayController::GetInstance()->SetDebugFreezeEnabled(true);

	if (flow && flow->IsPaused())
	{
		flow->CancelPause();
	}

	if (input)
	{
		input->SetLockCursor(false);
		input->SetCursorVisible(true);
	}
}

void GamePlayDebugTools::ExitImGuiFreeze(K4E::Input* input)
{
	if (!isImGuiFreeze_) { return; }

	isImGuiFreeze_ = false;
	// Debug Freeze解除時も入力キャプチャ状態は変更しない。
	K4E::EditorPlayController::GetInstance()->SetDebugFreezeEnabled(false);

	if (input)
	{
		const bool lock = !isDebugCamera_;
		input->SetLockCursor(lock);
		input->SetCursorVisible(!lock);
	}
}

void GamePlayDebugTools::UpdateFreeze()
{
	// 完全停止中は何も更新しない
}

void GamePlayDebugTools::UpdateDebugCamera(K4E::Input* input, GamePlayWorld* world)
{
#ifdef _DEBUG
	if (!input) { return; }

	if (input->TriggerKey(DIK_F12))
	{
		const bool nextDebug = !K4E::CameraManager::GetInstance()->IsUsingDebugCamera();

		K4E::CameraManager::GetInstance()->SetUseDebugCamera(nextDebug);
		K4E::Wireframe::GetInstance()->SetDebugCamera(nextDebug);

		isDebugCamera_ = nextDebug;

		if (world)
		{
			world->SetDebugCameraEnabled(isDebugCamera_);
		}

		input->SetLockCursor(!isDebugCamera_);
		input->SetCursorVisible(isDebugCamera_);
	}
#else
	(void)input;
	(void)world;
#endif
}

void GamePlayDebugTools::DrawImGui(GamePlayWorld* world)
{
#ifdef USE_IMGUI
	if (!world) { return; }

	auto& characters = world->GetCharacters();
	// WindowメニューのScene Debug/Rendering表示フラグをGamePlay系デバッグUIと共有する
	auto& editorWindowState = K4E::EditorWindowManager::GetInstance()->GetWindowState();

	K4E::LightManager::GetInstance()->DrawImGui(&editorWindowState.showLightEditor);
	if (editorWindowState.showGameDebug)
	{
		if (ImGui::Begin("Game Debug", &editorWindowState.showGameDebug))
		{
			ImGui::Text("Debug Camera: %s", isDebugCamera_ ? "ON" : "OFF");
			ImGui::Text("Debug Freeze (F10): %s", isImGuiFreeze_ ? "ON" : "OFF");
		}
		ImGui::End();

		characters.DrawImGui();
		world->DrawImGui();
	}

	if (editorWindowState.showCullingDebug)
	{
		// WindowメニューのCulling Debug表示フラグでStageChunk/Occlusion系の周辺デバッグUIをまとめる。
		stageChunkDebugController_.DrawImGui(world->GetStage());
		occlusionDebugController_.DrawImGui(world->GetStage());
	}

	/// ---------- 武器マスターデータエディタ ---------- ///
	static WeaponMasterDataDatabase weaponDB;
	static WeaponMasterDataEditor weaponEditor;
	static const std::filesystem::path kRoot = "Resources/JSON/weapons";

	if (!weaponEditorInitialized_)
	{
		weaponEditorInitialized_ = true;

		std::string err;
		weaponDB.LoadFromDirectory(kRoot, &err);
	}

	WeaponEditorHooks hooks{};

	hooks.SaveAll = [world]()
		{
			std::string err;
			WeaponMasterDataWriter::SaveAllByCategory(weaponDB, kRoot, &err);

			if (!world) { return; }
			if (auto* player = world->GetCharacters().GetPlayer())
			{
				player->GetWeaponComponent().ReloadWeaponMasterDataAndReequip();
			}
		};

	hooks.RequestReloadFocus = [](int32_t) {};

	hooks.RebuildLoadout = [world]()
		{
			if (!world) { return; }
			if (auto* player = world->GetCharacters().GetPlayer())
			{
				player->GetWeaponComponent().ReloadWeaponMasterDataAndReequip();
			}
		};

	hooks.ApplyToRuntimeIfCurrent =
		[this, world](int32_t weaponID, const FWeaponMasterData&)
		{
			lastAppliedWeaponID_ = weaponID;

			std::string err;
			WeaponMasterDataWriter::SaveAllByCategory(weaponDB, kRoot, &err);

			if (!world) { return; }
			if (auto* player = world->GetCharacters().GetPlayer())
			{
				auto& wc = player->GetWeaponComponent();

				if (wc.GetCurrentWeaponId() == weaponID)
				{
					wc.ReloadWeaponMasterDataAndReequip();
				}

				player->GetWeaponComponent().ReloadWeaponMasterDataAndReequip();
				player->ForceRefreshWeaponVisual();
			}
		};

	hooks.RequestDelete = [](int32_t weaponID)
		{
			std::string err;
			WeaponMasterDataWriter::DeleteFilesByWeaponID(kRoot, weaponID, &err);
			weaponDB.RemoveByID(weaponID);
		};

	hooks.RequestAdd = [](const std::string&, int32_t) {};

	hooks.PlaySoundPreviewSE = [](const std::string& path)
		{
			if (path.empty()) return;
			K4E::AudioManager::GetInstance()->PlayBGM(path, 1.0f, 1.0f, false);
		};

	hooks.GetImagePreview = [](const std::string& path)
		{
			WeaponEditorImagePreview out{};
			if (path.empty()) return out;

			std::string normalized = path;
			for (char& c : normalized)
			{
				if (c == '\\') c = '/';
			}

			std::error_code ec;
			if (!std::filesystem::exists(normalized, ec))
			{
				OutputDebugStringA(("[GetImagePreview] file not found: " + normalized + "\n").c_str());
				return out;
			}

			auto* texMgr = K4E::TextureManager::GetInstance();
			if (!texMgr)
			{
				OutputDebugStringA("[GetImagePreview] TextureManager is null\n");
				return out;
			}

			auto gpuHandle = texMgr->GetSrvHandleGPU(normalized);
			const auto& meta = texMgr->GetMetaData(normalized);

			out.imguiTextureId = reinterpret_cast<void*>(gpuHandle.ptr);
			out.width = static_cast<int>(meta.width);
			out.height = static_cast<int>(meta.height);

			return out;
		};

	// WindowメニューのWeapon Master Debug表示フラグを武器デバッグUIの×ボタン状態と共有する
	if (editorWindowState.showWeaponMasterDebug)
	{
		weaponEditor.DrawImGui(weaponDB, hooks);

		ImGui::Begin("Weapon Master Debug", &editorWindowState.showWeaponMasterDebug);
		ImGui::Text("Last Applied ID: %d", lastAppliedWeaponID_);

		if (ImGui::Button("Reload Weapon Editor DB"))
		{
			std::string err;
			weaponDB = WeaponMasterDataDatabase{};
			weaponDB.LoadFromDirectory(kRoot, &err);
		}
		ImGui::End();
	}
#else
	(void)world;
#endif
}