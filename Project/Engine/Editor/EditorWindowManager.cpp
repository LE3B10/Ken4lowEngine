#define NOMINMAX
#include "EditorWindowManager.h"

#include "EditorPlayController.h"
#include "ImGuiManager.h"
#include "PostEffectManager.h"
#include "GameViewportConstants.h"
#include <Input.h>
#include <LightManager.h>
#include <SceneManager.h>

#include <algorithm>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	namespace
	{
		EditorInputPolicy GetCurrentEditorInputPolicy()
		{
			BaseScene* scene = SceneManager::GetInstance()->GetCurrentScene();
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
				ImGui::MenuItem("Main Viewport", nullptr, &windowState_.showMainViewport);
				ImGui::MenuItem("World Outliner", nullptr, &windowState_.showWorldOutliner);
				ImGui::MenuItem("Details", nullptr, &windowState_.showDetails);
				ImGui::MenuItem("Content Browser", nullptr, &windowState_.showContentBrowser);
				ImGui::MenuItem("Output Log", nullptr, &windowState_.showOutputLog);
				ImGui::MenuItem("Toolbar", nullptr, &windowState_.showToolbar);
				ImGui::EndMenu();
			}

			// Renderingは描画・画面・ライト調整系ウィンドウをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Rendering"))
			{
				ImGui::MenuItem("Light Editor", nullptr, &windowState_.showLightEditor);
				ImGui::MenuItem("Post Effect Settings", nullptr, &windowState_.showPostEffectSettings);
				ImGui::MenuItem("Display", nullptr, &windowState_.showDisplay);
				ImGui::MenuItem("Parameters", nullptr, &windowState_.showParameters);
				ImGui::EndMenu();
			}

			// Scene Debugは現在のシーンに応じて描かれるデバッグウィンドウをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Scene Debug"))
			{
				ImGui::MenuItem("Title Debug", nullptr, &windowState_.showTitleDebug);
				ImGui::MenuItem("Stage Select Debug", nullptr, &windowState_.showStageSelectDebug);
				ImGui::MenuItem("Game Debug", nullptr, &windowState_.showGameDebug);
				ImGui::MenuItem("Player Debug", nullptr, &windowState_.showPlayerDebug);
				ImGui::MenuItem("Weapon Debug", nullptr, &windowState_.showWeaponDebug);
				ImGui::MenuItem("Enemy Debug", nullptr, &windowState_.showEnemyDebug);
				ImGui::MenuItem("Collision Debug", nullptr, &windowState_.showCollisionDebug);
				ImGui::MenuItem("Culling Debug", nullptr, &windowState_.showCullingDebug);
				ImGui::MenuItem("FadeManager", nullptr, &windowState_.showFadeManager);
				ImGui::EndMenu();
			}

			// Layout操作はDock再構築と簡易Focusレイアウト入口を明示的に分ける。
			if (ImGui::BeginMenu("Layout"))
			{
				if (ImGui::MenuItem("Rebuild Default Layout"))
				{
					// Rebuild Default Layoutは誤操作を避けるためPopupでOK確認してから実行する。
					openRebuildDefaultLayoutPopup_ = true;
				}
				if (ImGui::MenuItem("Game Focus Layout"))
				{
					// Game Focus LayoutはDock再配置せず周辺ウィンドウだけ一時的に非表示にする。
					windowState_.showMainViewport = true;
					windowState_.showToolbar = false;
					windowState_.showWorldOutliner = false;
					windowState_.showDetails = false;
					windowState_.showContentBrowser = false;
					windowState_.showOutputLog = false;
					windowState_.showLightEditor = false;
					windowState_.showPostEffectSettings = false;
					windowState_.showDisplay = false;
					windowState_.showParameters = false;
					windowState_.showTitleDebug = false;
					windowState_.showStageSelectDebug = false;
					windowState_.showGameDebug = false;
					windowState_.showPlayerDebug = false;
					windowState_.showWeaponDebug = false;
					windowState_.showEnemyDebug = false;
					windowState_.showCollisionDebug = false;
					windowState_.showCullingDebug = false;
					windowState_.showFadeManager = false;
				}
				ImGui::EndMenu();
			}

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

		if (openRebuildDefaultLayoutPopup_)
		{
			// PopupをMainMenuBar終了後に開き、MenuItem押下だけでDockBuilderを走らせない。
			ImGui::OpenPopup("Rebuild Default Layout?");
			openRebuildDefaultLayoutPopup_ = false;
		}

		if (ImGui::BeginPopupModal("Rebuild Default Layout?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Rebuild the editor docking layout to the default placement?");
			ImGui::TextUnformatted("Current dock positions will be overwritten when you press OK.");
			ImGui::Separator();
			if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
			{
				// OK時だけDockBuilderによる初期配置再構築を次フレームへ要求する。
				ImGuiManager::GetInstance()->RequestResetDockLayout();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				// CancelではDockBuilder要求を発行せず現在の保存済みDocking配置を維持する。
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
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
		std::vector<EditorObjectInfo> objects;
		BaseScene* scene = SceneManager::GetInstance()->GetCurrentScene();
		if (scene)
		{
			// Scene側から軽量情報だけを収集し、Outlinerが実オブジェクト寿命へ依存しないようにする。
			scene->CollectEditorObjects(objects);
		}

		if (selection_.HasSelection())
		{
			const EditorObjectInfo& selected = selection_.GetSelected();
			auto selectedObject = std::find_if(objects.begin(), objects.end(), [&selected](const EditorObjectInfo& object)
				{
					return object.id == selected.id && object.sceneName == selected.sceneName;
				});
			if (selectedObject != objects.end())
			{
				// Detailsが古いSceneオブジェクト入口を掴み続けないよう、現在フレームの情報へ更新する。
				selection_.RefreshSelected(*selectedObject);
			}
			else
			{
				// Scene切り替えやロード状態変化で消えた選択はDetails表示前に破棄する。
				selection_.Clear();
			}
		}

		if (!windowState_.showWorldOutliner)
		{
			// Detailsだけ表示している場合も、現在Sceneの収集結果で古い選択を先に破棄する。
			return;
		}

		if (ImGui::Begin("World Outliner", &windowState_.showWorldOutliner))
		{
			const char* sceneLabel = objects.empty() ? "Current Scene" : objects.front().sceneName.c_str();
			if (ImGui::TreeNodeEx(sceneLabel, ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (objects.empty())
				{
					ImGui::TextUnformatted("No editor objects.");
				}
				for (const EditorObjectInfo& object : objects)
				{
					const bool selected = selection_.HasSelection() &&
						selection_.GetSelected().id == object.id &&
						selection_.GetSelected().sceneName == object.sceneName;
					const std::string label = object.displayName + "##" + object.sceneName + std::to_string(object.id);
					if (ImGui::Selectable(label.c_str(), selected))
					{
						// クリックしたOutliner項目の軽量情報を選択状態として保存する。
						selection_.Select(object);
					}
				}
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

		if (ImGui::Begin("Details", &windowState_.showDetails))
		{
			if (!selection_.HasSelection())
			{
				ImGui::TextUnformatted("No object selected.");
			}
			else
			{
				const EditorObjectInfo& selected = selection_.GetSelected();
				// DetailsはTransform専用ではなく、Outliner選択に対応するInspector種別で必ず表示内容を切り替える。
				ImGui::Text("Selected Name: %s", selected.displayName.c_str());
				ImGui::Text("Type: %s", selected.typeName.c_str());
				ImGui::Text("Scene: %s", selected.sceneName.c_str());
				ImGui::Text("ID: %llu", static_cast<unsigned long long>(selected.id));
				if (!selected.inspectorHint.empty())
				{
					ImGui::Text("Inspector: %s", selected.inspectorHint.c_str());
				}
				ImGui::Separator();

				switch (selected.inspectorType)
				{
				case EditorInspectorType::Transform:
				{
					EditorTransform transform{};
					if (selected.TryReadTransform(transform))
					{
						bool changed = false;
						changed |= ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
						changed |= ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.01f);
						changed |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f);
						if (changed)
						{
							// ImGuiで編集した値はScene側が用意した書き戻し入口から即時反映する。
							selected.WriteTransform(transform);
						}
					}
					else
					{
						ImGui::Text("%s", selected.transformUnavailableReason.c_str());
					}
					break;
				}
				case EditorInspectorType::PunctualLights:
					ImGui::TextUnformatted("Type: Light Manager / Punctual Lights");
					LightManager::GetInstance()->DrawPunctualLightsInspector();
					break;
				case EditorInspectorType::FadeManager:
					if (selected.drawInspector)
					{
						selected.drawInspector();
					}
					else
					{
						ImGui::TextUnformatted("FadeManager Inspector is unavailable.");
					}
					break;
				case EditorInspectorType::StageSelectTextLayout:
					if (selected.drawInspector)
					{
						selected.drawInspector();
					}
					else
					{
						ImGui::TextUnformatted("StageSelect Text Layout Inspector is unavailable.");
					}
					break;
				case EditorInspectorType::ManagerInfo:
					ImGui::TextUnformatted("This manager has no Transform.");
					ImGui::TextUnformatted("Use the dedicated Debug window for detailed editing when available.");
					break;
				case EditorInspectorType::None:
				default:
					ImGui::TextUnformatted("この項目はDetails Inspector未実装です。");
					if (!selected.transformUnavailableReason.empty())
					{
						ImGui::Text("%s", selected.transformUnavailableReason.c_str());
					}
					break;
				}
			}
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
