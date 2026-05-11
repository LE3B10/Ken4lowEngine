#define NOMINMAX
#include "EditorWindowManager.h"

#include "ImGuiManager.h"
#include "PostEffectManager.h"
#include <Input.h>

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	EditorWindowManager* EditorWindowManager::GetInstance()
	{
		static EditorWindowManager instance;
		return &instance;
	}

	void EditorWindowManager::Draw()
	{
#ifdef USE_IMGUI
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
		// ツールバーは将来のPlay/Build/保存などの操作を集約する固定パネルにする
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("Toolbar", &windowState_.showToolbar, flags))
		{
			ImGui::Button("Save");
			ImGui::SameLine();
			ImGui::Button("Play");
			ImGui::SameLine();
			ImGui::Button("Pause");
			ImGui::SameLine();
			ImGui::Button("Stop");
			ImGui::SameLine();
			ImGui::Button("Build");
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
			mainViewportRect_.hovered = false;
			Input::GetInstance()->SetEditorViewportMousePosition({ 0.0f, 0.0f }, false);
			return;
		}

		// Main ViewportはGameRenderTargetをImGui::Imageで表示する固定名ウィンドウにする
		if (ImGui::Begin("Main Viewport", &windowState_.showMainViewport, ImGuiWindowFlags_NoScrollbar))
		{
			const ImVec2 availableSize = ImGui::GetContentRegionAvail();
			auto* postEffectManager = PostEffectManager::GetInstance();

			// Main Viewportの表示可能領域をそのままゲーム描画サイズの基準として保存する。
			const ImVec2 imageSize = ImVec2(std::max(0.0f, availableSize.x), std::max(0.0f, availableSize.y));
			const ImVec2 screenStart = ImGui::GetCursorScreenPos();
			mainViewportScreenPosition_ = { screenStart.x, screenStart.y };
			mainViewportSize_ = { imageSize.x, imageSize.y };
			// ImGui::Imageで描くゲーム画面のスクリーン矩形を入力変換用に保存する
			mainViewportRect_.screenMin = mainViewportScreenPosition_;
			mainViewportRect_.screenMax = { mainViewportScreenPosition_.x + imageSize.x, mainViewportScreenPosition_.y + imageSize.y };
			mainViewportRect_.imageSize = mainViewportSize_;
			mainViewportRect_.valid = imageSize.x > 1.0f && imageSize.y > 1.0f;

			const D3D12_GPU_DESCRIPTOR_HANDLE gameSrv = postEffectManager->GetGameRenderTargetSrvHandleGPU();
			if (gameSrv.ptr != 0 && imageSize.x > 1.0f && imageSize.y > 1.0f)
			{
				// SRVManagerで作成したGameRenderTargetのGPU SRVをMain Viewportの現在サイズで渡す
				ImGui::Image(static_cast<ImTextureID>(gameSrv.ptr), imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
				mainViewportRect_.hovered = ImGui::IsItemHovered(); // 他ウィンドウに覆われたMain Viewportはゲームクリック扱いにしない
			}
			else
			{
				ImGui::TextUnformatted("GameRenderTarget SRV is not ready.");
				mainViewportRect_.hovered = false;
			}

			Vector2 gameMouse = {};
			const bool gameMouseValid = GetMousePositionInGameViewport(gameMouse);
			Input::GetInstance()->SetEditorViewportMousePosition(gameMouse, gameMouseValid);
		}
		else
		{
			// Main Viewportウィンドウが折りたたまれた場合もゲーム側のマウス入力を無効化する
			mainViewportRect_.valid = false;
			mainViewportRect_.hovered = false;
			Input::GetInstance()->SetEditorViewportMousePosition({ 0.0f, 0.0f }, false);
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
		if (!mainViewportRect_.valid || !mainViewportRect_.hovered)
		{
			// Main Viewport外または他のImGuiウィンドウ操作中はゲーム側へマウス入力を渡さない
			return false;
		}

		const ImVec2 mouseScreen = ImGui::GetMousePos();
		const Vector2 mouse = { mouseScreen.x, mouseScreen.y };
		if (mouse.x < mainViewportRect_.screenMin.x || mouse.y < mainViewportRect_.screenMin.y ||
			mouse.x > mainViewportRect_.screenMax.x || mouse.y > mainViewportRect_.screenMax.y)
		{
			return false;
		}

		const auto* postEffectManager = PostEffectManager::GetInstance();
		const float renderTargetWidth = static_cast<float>(postEffectManager->GetGameRenderTargetWidth());
		const float renderTargetHeight = static_cast<float>(postEffectManager->GetGameRenderTargetHeight());
		if (renderTargetWidth <= 0.0f || renderTargetHeight <= 0.0f ||
			mainViewportRect_.imageSize.x <= 0.0f || mainViewportRect_.imageSize.y <= 0.0f)
		{
			return false;
		}

		// 表示矩形内のローカル座標を現在のGameViewportRenderTargetピクセル座標へスケール変換する。
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
