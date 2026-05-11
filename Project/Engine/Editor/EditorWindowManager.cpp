#define NOMINMAX
#include "EditorWindowManager.h"

#include "EditorPlayController.h"
#include "ImGuiManager.h"
#include "PostEffectManager.h"
#include "GameViewportConstants.h"
#include <Input.h>
#include <SceneManager.h>

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	namespace
	{
		EditorInputPolicy GetCurrentEditorInputPolicy()
		{
			const BaseScene* scene = SceneManager::GetInstance()->GetCurrentScene();
			// Scene未設定時はUI Mouse扱いにしてEditor操作を妨げない。
			return scene ? scene->GetEditorInputPolicy() : EditorInputPolicy::UiMouse;
		}

		bool IsFpsCapturePolicy()
		{
			// F8キャプチャはFPS操作Sceneだけで有効にする。
			return GetCurrentEditorInputPolicy() == EditorInputPolicy::FpsCapture;
		}
	}

	EditorWindowManager* EditorWindowManager::GetInstance()
	{
		static EditorWindowManager instance;
		return &instance;
	}

	void EditorWindowManager::Draw()
	{
#ifdef USE_IMGUI
		auto* input = Input::GetInstance();
		if (input->TriggerRawKey(DIK_F8) && IsFpsCapturePolicy())
		{
			const bool forceRelease = input->PushRawKey(DIK_LSHIFT) || input->PushRawKey(DIK_RSHIFT);
			// F8はFPS操作Sceneだけ入力キャプチャ切替、Shift+F8は非常口にする。
			if (forceRelease)
			{
				EditorPlayController::GetInstance()->ForceReleaseToEditor();
			}
			else
			{
				EditorPlayController::GetInstance()->ToggleInputCapture();
			}
		}

		// UE5風エディタUIの描画順をManagerに集約する
		DrawMenuBar();
		if (windowState_.showToolbar)
		{
			DrawToolbar();
		}
		DrawMainViewport();
		DrawWorldOutliner();
		DrawDetails();
		DrawContentBrowser();
		DrawOutputLog();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawMenuBar()
	{
#ifdef USE_IMGUI
		// メインメニューは各エディタパネルの表示状態を切り替える入口にする
		if (!ImGui::BeginMainMenuBar())
		{
			return;
		}

		if (ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("New Level");
			ImGui::MenuItem("Open Level");
			ImGui::MenuItem("Save Level");
			ImGui::Separator();
			ImGui::MenuItem("Exit");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			// Commonは常時使うエディタ基本パネルをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Common"))
			{
				ImGui::MenuItem("Toolbar", nullptr, &windowState_.showToolbar);
				ImGui::MenuItem("Main Viewport", nullptr, &windowState_.showMainViewport);
				ImGui::MenuItem("Content Browser", nullptr, &windowState_.showContentBrowser);
				ImGui::MenuItem("World Outliner", nullptr, &windowState_.showWorldOutliner);
				ImGui::MenuItem("Details", nullptr, &windowState_.showDetails);
				ImGui::MenuItem("Output Log", nullptr, &windowState_.showOutputLog);
				ImGui::EndMenu();
			}

			// Renderingは描画・画面・ライト調整系ウィンドウをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Rendering"))
			{
				ImGui::MenuItem("Parameters", nullptr, &windowState_.showParameters);
				ImGui::MenuItem("Display", nullptr, &windowState_.showDisplay);
				ImGui::MenuItem("Post Effect Settings", nullptr, &windowState_.showPostEffectSettings);
				ImGui::MenuItem("Light Editor", nullptr, &windowState_.showLightEditor);
				ImGui::EndMenu();
			}

			// Scene Debugは現在のシーンに応じて描かれるデバッグウィンドウをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Scene Debug"))
			{
				ImGui::MenuItem("Title Debug", nullptr, &windowState_.showTitleDebug);
				ImGui::MenuItem("Stage Select Debug", nullptr, &windowState_.showStageSelectDebug);
				ImGui::MenuItem("Game Debug", nullptr, &windowState_.showGameDebug);
				ImGui::MenuItem("Weapon Master Debug", nullptr, &windowState_.showWeaponMasterDebug);
				ImGui::MenuItem("FadeManager - Tile Fade", nullptr, &windowState_.showTileFadeDebug);
				ImGui::EndMenu();
			}

			// Reset Layoutは誤操作でDock配置を崩しやすいため一旦メニューから外す
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Tools"))
		{
			ImGui::MenuItem("Stage Editor");
			ImGui::MenuItem("Light Editor");
			ImGui::MenuItem("Particle Editor");
			ImGui::MenuItem("Shader Viewer");
			ImGui::Separator();
			ImGui::MenuItem("Texture Converter");
			ImGui::MenuItem("Mesh Converter");
			ImGui::MenuItem("Font Converter");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Build"))
		{
			ImGui::MenuItem("Build Textures");
			ImGui::MenuItem("Build Meshes");
			ImGui::MenuItem("Build Fonts");
			ImGui::Separator();
			ImGui::MenuItem("Build All Assets");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			ImGui::MenuItem("Show Collider", nullptr, &windowState_.debugShowCollider);
			ImGui::MenuItem("Show Enemy Info", nullptr, &windowState_.debugShowEnemyInfo);
			ImGui::MenuItem("Show Wave Info", nullptr, &windowState_.debugShowWaveInfo);
			ImGui::MenuItem("Show FPS", nullptr, &windowState_.debugShowFps);
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawToolbar()
	{
#ifdef USE_IMGUI
		auto* playController = EditorPlayController::GetInstance();

		const bool fpsCapturePolicy = IsFpsCapturePolicy();
		// ツールバーは現在Sceneの入力ポリシーに合わせて表示と操作可否を分ける。
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("Toolbar", &windowState_.showToolbar, flags))
		{
			ImGui::Button("Save");
			ImGui::SameLine();
			if (ImGui::Button("Play"))
			{
				// PlayボタンはEditorPlayStateをPlayにし、入力をGameCapturedへ遷移させる。
				playController->Play();
			}
			ImGui::SameLine();
			if (ImGui::Button("Pause"))
			{
				// Pauseボタンは入力状態を維持したままEditorPlayStateだけPauseへ遷移させる。
				playController->Pause();
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop"))
			{
				// Stopボタンは編集状態へ戻し、ゲーム入力をGameReleasedへ解放する。
				playController->Stop();
			}
			ImGui::SameLine();
			ImGui::Button("Build");
			ImGui::SameLine();
			ImGui::TextUnformatted("|");
			ImGui::SameLine();
			if (fpsCapturePolicy)
			{
				ImGui::Text("%s  F8: %s", playController->IsGameCaptured() ? "Input: FPS Captured" : "Input: Editor Released", playController->IsGameCaptured() ? "Release" : "Capture");
				ImGui::SameLine();
				if (ImGui::Button(playController->IsGameCaptured() ? "Release Input" : "Capture Input"))
				{
					// ToolbarボタンもFPS操作SceneでだけF8と同じキャプチャ切り替えを実行する。
					playController->ToggleInputCapture();
				}
			}
			else
			{
				// UI Mouse Sceneではキャプチャ操作を出さず常にカーソルUI操作を示す。
				ImGui::TextUnformatted("Input: UI Mouse Mode");
			}
			// Debug Freezeは入力キャプチャとは別状態としてToolbar上で確認できるようにする。
			ImGui::TextUnformatted(playController->GetDebugFreezeStatusText());
			// F8やViewport hoverの状態をToolbar上で即確認できるようにする。
			ImGui::Text("State: %s", playController->GetPlayStateText());
			const char* toolbarInputText = fpsCapturePolicy
				? (playController->IsGameCaptured() ? "FPS Captured" : "Editor Released")
				: "UI Mouse";
			// ToolbarにはPlay状態と入力ポリシーをUE5風に分けて表示する。
			ImGui::Text("Input: %s", toolbarInputText);
			if (!playController->IsPlaying())
			{
				// Edit/Pause中はSceneのゲーム進行がUpdateEditorへ分岐していることを明示する。
				ImGui::TextUnformatted("Game update stopped");
			}
			ImGui::Text("Main Viewport Hovered: %s", inputDebugInfo_.mainViewportHovered ? "true" : "false");
			ImGui::Text("ImGui MouseClicked[0]: %s", inputDebugInfo_.imguiMouseClicked0 ? "true" : "false");
			ImGui::Text("ImGui MouseDown[0]: %s", inputDebugInfo_.imguiMouseDown0 ? "true" : "false");
			ImGui::Text("Input Left Trigger: %s", inputDebugInfo_.inputLeftTrigger ? "true" : "false");
			ImGui::Text("Game Mouse Enabled: %s", inputDebugInfo_.gameMouseEnabled ? "true" : "false");
			ImGui::Text("Game Mouse Position: %.1f, %.1f", inputDebugInfo_.gameMousePosition.x, inputDebugInfo_.gameMousePosition.y);
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawMainViewport()
	{
#ifdef USE_IMGUI
		if (!windowState_.showMainViewport)
		{
			// Main Viewport非表示中はゲーム側へエディタマウス入力を渡さない
			mainViewportRect_.valid = false;
			mainViewportRect_.isHovered = false;
			inputDebugInfo_ = {}; // Main Viewport非表示時もToolbarの入力診断を無効状態へ戻す。
			inputDebugInfo_.gameMousePosition = { -1.0f, -1.0f };
			Input::GetInstance()->SetGameInputEnabled(false);
			Input::GetInstance()->SetEditorViewportMousePosition({ 0.0f, 0.0f }, false);
			Input::GetInstance()->SetLockCursor(false);
			Input::GetInstance()->SetCursorVisible(true);
			return;
		}

		// Main ViewportはGameRenderTargetをDrawListで表示する固定名ウィンドウにする
		if (ImGui::Begin("Main Viewport", &windowState_.showMainViewport, ImGuiWindowFlags_NoScrollbar))
		{
			const ImVec2 availableSize = ImGui::GetContentRegionAvail();
			auto* postEffectManager = PostEffectManager::GetInstance();
			const float renderTargetWidth = static_cast<float>(GameViewportConstants::Width);
			const float renderTargetHeight = static_cast<float>(GameViewportConstants::Height);

			// Main Viewport内では固定解像度GameRenderTargetを16:9維持で最大表示する。
			ImVec2 imageSize = ImVec2(0.0f, 0.0f);
			if (availableSize.x > 1.0f && availableSize.y > 1.0f && renderTargetWidth > 0.0f && renderTargetHeight > 0.0f)
			{
				const float targetAspect = renderTargetWidth / renderTargetHeight;
				imageSize = ImVec2(std::max(0.0f, availableSize.x), std::max(0.0f, availableSize.x / targetAspect));
				if (imageSize.y > availableSize.y)
				{
					imageSize.y = std::max(0.0f, availableSize.y);
					imageSize.x = std::max(0.0f, availableSize.y * targetAspect);
				}
			}

			const ImVec2 contentScreenMin = ImGui::GetCursorScreenPos();
			const ImVec2 imageOffset = ImVec2(
				std::max(0.0f, (availableSize.x - imageSize.x) * 0.5f),
				std::max(0.0f, (availableSize.y - imageSize.y) * 0.5f));
			const ImVec2 imageScreenMin = ImVec2(contentScreenMin.x + imageOffset.x, contentScreenMin.y + imageOffset.y);
			const ImVec2 imageScreenMax = ImVec2(imageScreenMin.x + imageSize.x, imageScreenMin.y + imageSize.y);
			mainViewportScreenPosition_ = { imageScreenMin.x, imageScreenMin.y };
			mainViewportSize_ = { imageSize.x, imageSize.y };
			// 入力はAddImageで描画した中央表示矩形だけを1920x1080へ逆変換する。
			mainViewportRect_.screenMin = mainViewportScreenPosition_;
			mainViewportRect_.screenMax = { imageScreenMax.x, imageScreenMax.y };
			mainViewportRect_.imageSize = mainViewportSize_;
			mainViewportRect_.valid = imageSize.x > 1.0f && imageSize.y > 1.0f;

			const D3D12_GPU_DESCRIPTOR_HANDLE gameSrv = postEffectManager->GetGameRenderTargetSrvHandleGPU();
			if (gameSrv.ptr != 0 && imageSize.x > 1.0f && imageSize.y > 1.0f)
			{
				// SetCursorPosを使わずDrawListへ直接描画してImGuiの境界更新assertを避ける。
				ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(gameSrv.ptr), imageScreenMin, imageScreenMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
			}
			else if (gameSrv.ptr == 0 && availableSize.x > 1.0f && availableSize.y > 1.0f)
			{
				ImGui::GetWindowDrawList()->AddText(contentScreenMin, ImGui::GetColorU32(ImGuiCol_Text), "GameRenderTarget SRV is not ready.");
			}

			if (availableSize.x > 1.0f && availableSize.y > 1.0f)
			{
				// InvisibleButtonはMain ViewportのImGuiアイテム登録だけに使い、ゲーム入力判定は画像矩形で行う。
				ImGui::InvisibleButton("MainViewportImageArea", availableSize);
				const ImVec2 mouseScreen = ImGui::GetMousePos();
				const bool mouseInsideImage = mainViewportRect_.valid &&
					mouseScreen.x >= imageScreenMin.x && mouseScreen.y >= imageScreenMin.y &&
					mouseScreen.x <= imageScreenMax.x && mouseScreen.y <= imageScreenMax.y;
				const bool otherItemActive = ImGui::IsAnyItemActive() && !ImGui::IsItemActive();
				// WantCaptureMouseではなくSceneポリシーとMain Viewport画像上かどうかでゲームクリックを許可する。
				mainViewportRect_.isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && mouseInsideImage && !otherItemActive;
			}
			else
			{
				mainViewportRect_.isHovered = false;
			}

			Vector2 gameMouse = {};
			const bool gameMouseValid = GetMousePositionInGameViewport(gameMouse);
			const EditorInputPolicy inputPolicy = GetCurrentEditorInputPolicy();
			const bool isPlaying = EditorPlayController::GetInstance()->IsPlaying();
			// Edit/Pause中はMain ViewportクリックをゲームSceneへ渡さず、Play中だけ入力ポリシーを適用する。
			const bool gameInputEnabled = isPlaying && (inputPolicy == EditorInputPolicy::UiMouse || EditorPlayController::GetInstance()->IsGameCaptured()) && mainViewportRect_.isHovered;
			Input::GetInstance()->SetGameInputEnabled(gameInputEnabled);
			Input::GetInstance()->SetEditorViewportMousePosition(gameMouse, gameMouseValid);
			// UI Mouseは常にカーソル表示、FPS CaptureはGameCaptured中だけ中央固定にする。
			const bool lockForFpsCapture = inputPolicy == EditorInputPolicy::FpsCapture && gameInputEnabled;
			Input::GetInstance()->SetLockCursor(lockForFpsCapture);
			Input::GetInstance()->SetCursorVisible(!lockForFpsCapture);
			inputDebugInfo_.mainViewportHovered = mainViewportRect_.isHovered; // 入力許可条件のViewport hoverをToolbarに表示する。
			inputDebugInfo_.imguiMouseClicked0 = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
			inputDebugInfo_.imguiMouseDown0 = ImGui::IsMouseDown(ImGuiMouseButton_Left);
			inputDebugInfo_.inputLeftTrigger = Input::GetInstance()->TriggerMouse(0);
			inputDebugInfo_.gameMouseEnabled = gameInputEnabled && gameMouseValid;
			inputDebugInfo_.gameMousePosition = inputDebugInfo_.gameMouseEnabled ? gameMouse : Vector2{ -1.0f, -1.0f };
		}
		else
		{
			// Main Viewportウィンドウが折りたたまれた場合もゲーム側のマウス入力を無効化する
			mainViewportRect_.valid = false;
			mainViewportRect_.isHovered = false;
			inputDebugInfo_ = {}; // Main Viewportが描けない時はゲームクリック診断も無効化する。
			inputDebugInfo_.gameMousePosition = { -1.0f, -1.0f };
			Input::GetInstance()->SetGameInputEnabled(false);
			Input::GetInstance()->SetEditorViewportMousePosition({ 0.0f, 0.0f }, false);
			Input::GetInstance()->SetLockCursor(false);
			Input::GetInstance()->SetCursorVisible(true);
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	Vector2 EditorWindowManager::ConvertScreenToMainViewportPosition(const Vector2& screenPosition) const
	{
		// 入力系はこの入口でスクリーン座標からMain Viewportローカル座標へ変換する
		return { screenPosition.x - mainViewportScreenPosition_.x, screenPosition.y - mainViewportScreenPosition_.y };
	}

	bool EditorWindowManager::GetMousePositionInGameViewport(Vector2& outMouse) const
	{
#ifdef USE_IMGUI
		outMouse = { 0.0f, 0.0f };
		const EditorInputPolicy inputPolicy = GetCurrentEditorInputPolicy();
		if (inputPolicy == EditorInputPolicy::FpsCapture && !(EditorPlayController::GetInstance()->IsPlaying() && EditorPlayController::GetInstance()->IsGameCaptured()))
		{
			// FPS Capture SceneはPlayかつGameCaptured中だけゲーム側へマウス入力を渡す。
			return false;
		}

		if (!mainViewportRect_.valid || !mainViewportRect_.isHovered)
		{
			// Main Viewport外ではImGuiのCapture状態に関係なくゲーム側へマウス入力を渡さない。
			return false;
		}

		const ImVec2 mouseScreen = ImGui::GetMousePos();
		const Vector2 mouse = { mouseScreen.x, mouseScreen.y };
		if (mouse.x < mainViewportRect_.screenMin.x || mouse.y < mainViewportRect_.screenMin.y ||
			mouse.x > mainViewportRect_.screenMax.x || mouse.y > mainViewportRect_.screenMax.y)
		{
			return false;
		}

		const float renderTargetWidth = static_cast<float>(GameViewportConstants::Width);
		const float renderTargetHeight = static_cast<float>(GameViewportConstants::Height);
		if (renderTargetWidth <= 0.0f || renderTargetHeight <= 0.0f ||
			mainViewportRect_.imageSize.x <= 0.0f || mainViewportRect_.imageSize.y <= 0.0f)
		{
			return false;
		}

		// 表示矩形内のローカル座標を固定GameViewportRenderTarget(1920x1080)へスケール変換する。
		const Vector2 localMouse = { mouse.x - mainViewportRect_.screenMin.x, mouse.y - mainViewportRect_.screenMin.y };
		outMouse = {
			(localMouse.x / mainViewportRect_.imageSize.x) * renderTargetWidth,
			(localMouse.y / mainViewportRect_.imageSize.y) * renderTargetHeight
		};
		return true;
#else
		outMouse = { 0.0f, 0.0f };
		return false;
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawWorldOutliner()
	{
#ifdef USE_IMGUI
		if (!windowState_.showWorldOutliner)
		{
			return;
		}

		// World Outlinerは後で実オブジェクト一覧へ差し替えるため仮ノードだけ表示する
		if (ImGui::Begin("World Outliner", &windowState_.showWorldOutliner))
		{
			if (ImGui::TreeNodeEx("Current Scene", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Selectable("Player (placeholder)");
				ImGui::Selectable("Main Camera (placeholder)");
				ImGui::Selectable("Directional Light (placeholder)");
				ImGui::Selectable("Stage Root (placeholder)");
				ImGui::TreePop();
			}
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawDetails()
	{
#ifdef USE_IMGUI
		if (!windowState_.showDetails)
		{
			return;
		}

		// Detailsは後で選択中オブジェクトの編集UIへ接続するため仮プロパティを表示する
		if (ImGui::Begin("Details", &windowState_.showDetails))
		{
			ImGui::TextUnformatted("Selection: None");
			ImGui::Separator();
			ImGui::TextUnformatted("Transform");
			float zero3[3] = { 0.0f, 0.0f, 0.0f };
			ImGui::InputFloat3("Location", zero3, "%.2f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation", zero3, "%.2f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Scale", zero3, "%.2f", ImGuiInputTextFlags_ReadOnly);
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawContentBrowser()
	{
#ifdef USE_IMGUI
		if (!windowState_.showContentBrowser)
		{
			return;
		}

		// Content Browserは後でResources配下のアセット列挙へ接続するため仮フォルダを表示する
		if (ImGui::Begin("Content Browser", &windowState_.showContentBrowser))
		{
			ImGui::TextUnformatted("/Resources");
			ImGui::Separator();
			ImGui::Button("Textures");
			ImGui::SameLine();
			ImGui::Button("Models");
			ImGui::SameLine();
			ImGui::Button("Shaders");
			ImGui::SameLine();
			ImGui::Button("Fonts");
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawOutputLog()
	{
#ifdef USE_IMGUI
		if (!windowState_.showOutputLog)
		{
			return;
		}

		// Output Logは後でエンジンログへ接続するため仮ログ行だけ表示する
		if (ImGui::Begin("Output Log", &windowState_.showOutputLog))
		{
			ImGui::TextUnformatted("[Editor] UE5-style editor shell initialized.");
			ImGui::TextUnformatted("[Editor] Dock panels from the Window menu.");
			if (windowState_.debugShowFps)
			{
				ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			}
		}
		ImGui::End();
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
