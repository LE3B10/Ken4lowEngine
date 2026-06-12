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
#include <GameTimer.h>


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

	if (input->TriggerKey(DIK_F9))
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

	const auto* gameTimer = K4E::GameTimer::GetInstance();
	const float fps = gameTimer->GetFPS();
	const float deltaSeconds = gameTimer->GetDeltaTime();
	// FPS表示はImGuiの内部値ではなく、エンジン側の計測値に統一する。
	performanceMonitor_.Update(deltaSeconds, fps);
	const auto& perfStats = performanceMonitor_.GetStats();
	// WindowメニューのScene Debug/Rendering表示フラグをGamePlay系デバッグUIと共有する
	auto& editorWindowState = K4E::EditorWindowManager::GetInstance()->GetWindowState();

	K4E::LightManager::GetInstance()->DrawImGui(&editorWindowState.showLightEditor);
	if (editorWindowState.showGameDebug)
	{
		ImGui::SetNextWindowSize(ImVec2(420.0f, 260.0f), ImGuiCond_FirstUseEver);
		// Game DebugはScene全体の状態だけに絞り、Main Viewportへ固定配置しないDocking対象にする。
		if (ImGui::Begin("Game Debug", &editorWindowState.showGameDebug))
		{
			ImGui::Text("Debug Camera: %s", isDebugCamera_ ? "ON" : "OFF");
			ImGui::Text("Debug Freeze (F10): %s", isImGuiFreeze_ ? "ON" : "OFF");
			if (performanceDisplayMode_ == PerformanceDisplayMode::FPS)
			{
				ImGui::Text("Instant FPS: %.1f | Average FPS: %.1f", perfStats.instantFps, perfStats.fps);
			}
			else
			{
				ImGui::Text("Frame: %.2f ms", perfStats.frameTimeMs);
			}
			world->DrawGameDebugImGui();
		}
		ImGui::End();
	}
	if (showPerformanceMonitor_)
	{
		if (ImGui::Begin("Performance Monitor", &showPerformanceMonitor_))
		{
			ImGui::Checkbox("Show Graph", &showPerformanceGraph_);
			const char* displayModeLabels[] = { "FPS", "FrameTime(ms)" };
			int displayMode = static_cast<int>(performanceDisplayMode_);
			if (ImGui::Combo("Display Mode", &displayMode, displayModeLabels, IM_ARRAYSIZE(displayModeLabels)))
			{
				performanceDisplayMode_ = static_cast<PerformanceDisplayMode>(displayMode);
			}

			if (performanceDisplayMode_ == PerformanceDisplayMode::FPS)
			{
				ImGui::Text("Instant FPS: %.1f | Average FPS: %.1f", perfStats.instantFps, perfStats.fps);
			}
			else
			{
				ImGui::Text("Frame: %.2f ms", perfStats.frameTimeMs);
			}

			ImGui::Text("Instant FPS: %.1f", perfStats.instantFps);
			ImGui::Text("Average FPS: %.1f", perfStats.fps);
			ImGui::Text("FrameTime: %.2f ms", perfStats.frameTimeMs);
			ImGui::Text("CPU Usage: %.1f%%", perfStats.cpuUsagePercent);
			ImGui::Text("Process CPU Usage: %.1f%%", perfStats.processCpuUsagePercent);
			ImGui::Text("Memory Usage: %.1f MB", perfStats.memoryUsageMB);

			if (showPerformanceGraph_)
			{
				if (performanceDisplayMode_ == PerformanceDisplayMode::FPS)
				{
					const auto& history = performanceMonitor_.GetFpsHistory();
					ImGui::PlotLines("FPS History", history.data(), static_cast<int>(history.size()), 0, nullptr, 0.0f, 240.0f, ImVec2(0.0f, 90.0f));
				}
				else
				{
					const auto& history = performanceMonitor_.GetFrameTimeHistory();
					ImGui::PlotLines("FrameTime History", history.data(), static_cast<int>(history.size()), 0, nullptr, 0.0f, 50.0f, ImVec2(0.0f, 90.0f));
				}
			}
		}
		ImGui::End();
	}


	if (editorWindowState.showPlayerDebug)
	{
		ImGui::SetNextWindowSize(ImVec2(480.0f, 520.0f), ImGuiCond_FirstUseEver);
		// Player Debugは腕・照準・被弾判定などPlayer関連調整だけを表示する。
		if (ImGui::Begin("Player Debug", &editorWindowState.showPlayerDebug))
		{
			characters.DrawPlayerDebugImGui();
		}
		ImGui::End();
	}


	if (editorWindowState.showEnemyDebug)
	{
		ImGui::SetNextWindowSize(ImVec2(480.0f, 520.0f), ImGuiCond_FirstUseEver);
		// Enemy DebugはEnemy Manager相当の一覧とEnemy AI/HPBar確認を分離表示する。
		if (ImGui::Begin("Enemy Debug", &editorWindowState.showEnemyDebug))
		{
			characters.DrawEnemyDebugImGui();
			world->DrawEnemyDebugImGui();
		}
		ImGui::End();
	}

	if (editorWindowState.showCollisionDebug)
	{
		ImGui::SetNextWindowSize(ImVec2(420.0f, 360.0f), ImGuiCond_FirstUseEver);
		// Collision DebugはCollider表示とCollisionManager統計を通常Dockウィンドウへ分離する。
		if (ImGui::Begin("Collision Debug", &editorWindowState.showCollisionDebug))
		{
			world->DrawCollisionDebugImGui();
		}
		ImGui::End();
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

	// Weapon Debugは武器状態・見た目・マスターデータ補助を1つのDocking対象へまとめる。
	if (editorWindowState.showWeaponDebug)
	{
		weaponEditor.DrawImGui(weaponDB, hooks);

		ImGui::SetNextWindowSize(ImVec2(520.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Weapon Debug", &editorWindowState.showWeaponDebug))
		{
			characters.GetPlayer() ? characters.GetPlayer()->DrawWeaponDebugImGui() : ImGui::TextUnformatted("Player is not available.");
			ImGui::SeparatorText("Weapon Master Debug");
			ImGui::Text("Last Applied ID: %d", lastAppliedWeaponID_);

			if (ImGui::Button("Reload Weapon Editor DB"))
			{
				std::string err;
				weaponDB = WeaponMasterDataDatabase{};
				weaponDB.LoadFromDirectory(kRoot, &err);
			}
		}
		ImGui::End();
	}
#else
	(void)world;
#endif
}

void GamePlayDebugTools::DrawCullingDebugContent(GamePlayWorld* world)
{
#ifdef USE_IMGUI
	if (!world) { return; }
	// Culling Debugの親ウィンドウ内へStageChunk/Occlusionの既存調整UIを埋め込む。
	if (ImGui::CollapsingHeader("StageChunk Culling Debug", ImGuiTreeNodeFlags_DefaultOpen))
	{
		stageChunkDebugController_.DrawImGuiContent(world->GetStage());
	}
	if (ImGui::CollapsingHeader("Occlusion Culling Debug", ImGuiTreeNodeFlags_DefaultOpen))
	{
		occlusionDebugController_.DrawImGuiContent(world->GetStage());
	}
#else
	(void)world;
#endif
}
