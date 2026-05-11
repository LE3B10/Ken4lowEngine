#include "EditorWindowManager.h"

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
			ImGui::MenuItem("Main Viewport", nullptr, &windowState_.showMainViewport);
			ImGui::MenuItem("World Outliner", nullptr, &windowState_.showWorldOutliner);
			ImGui::MenuItem("Details", nullptr, &windowState_.showDetails);
			ImGui::MenuItem("Content Browser", nullptr, &windowState_.showContentBrowser);
			ImGui::MenuItem("Output Log", nullptr, &windowState_.showOutputLog);
			ImGui::Separator();
			ImGui::MenuItem("Toolbar", nullptr, &windowState_.showToolbar);
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
			return;
		}

		// Main Viewportは後でRenderTarget Previewを差し込むための表示領域だけ先に確保する
		if (ImGui::Begin("Main Viewport", &windowState_.showMainViewport))
		{
			const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
			ImGui::TextUnformatted("Main Viewport");
			ImGui::Separator();
			ImGui::Text("RenderTarget preview will be displayed here.");
			ImGui::Text("Available Size: %.0f x %.0f", viewportSize.x, viewportSize.y);
			ImGui::Dummy(ImVec2(viewportSize.x, viewportSize.y > 80.0f ? viewportSize.y - 80.0f : 0.0f));
		}
		ImGui::End();
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
