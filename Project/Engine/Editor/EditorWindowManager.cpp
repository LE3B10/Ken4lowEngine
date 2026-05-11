#include "EditorWindowManager.h"

#include "ImGuiManager.h"
#include "PostEffectManager.h"

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
			return;
		}

		// Main ViewportはGameRenderTargetをImGui::Imageで表示する固定名ウィンドウにする
		if (ImGui::Begin("Main Viewport", &windowState_.showMainViewport, ImGuiWindowFlags_NoScrollbar))
		{
			const ImVec2 availableSize = ImGui::GetContentRegionAvail();
			auto* postEffectManager = PostEffectManager::GetInstance();
			const float targetWidth = static_cast<float>(postEffectManager->GetGameRenderTargetWidth());
			const float targetHeight = static_cast<float>(postEffectManager->GetGameRenderTargetHeight());
			const float targetAspect = targetWidth / targetHeight;
			float imageWidth = availableSize.x;
			float imageHeight = imageWidth / targetAspect;
			if (imageHeight > availableSize.y)
			{
				imageHeight = availableSize.y;
				imageWidth = imageHeight * targetAspect;
			}

			// Main Viewportの空き領域に引き伸ばさず、16:9を保った最大サイズで中央寄せする
			const ImVec2 imageSize = ImVec2(std::max(0.0f, imageWidth), std::max(0.0f, imageHeight));
			const ImVec2 cursorStart = ImGui::GetCursorPos();
			const ImVec2 screenStart = ImGui::GetCursorScreenPos();
			const ImVec2 imageOffset = ImVec2(
				std::max(0.0f, (availableSize.x - imageSize.x) * 0.5f),
				std::max(0.0f, (availableSize.y - imageSize.y) * 0.5f));
			ImGui::SetCursorPos(ImVec2(cursorStart.x + imageOffset.x, cursorStart.y + imageOffset.y));
			mainViewportScreenPosition_ = { screenStart.x + imageOffset.x, screenStart.y + imageOffset.y };
			mainViewportSize_ = { imageSize.x, imageSize.y };

			const D3D12_GPU_DESCRIPTOR_HANDLE gameSrv = postEffectManager->GetGameRenderTargetSrvHandleGPU();
			if (gameSrv.ptr != 0 && imageSize.x > 1.0f && imageSize.y > 1.0f)
			{
				// SRVManagerで作成したGameRenderTargetのGPU SRVを16:9維持後の表示サイズで渡す
				ImGui::Image(static_cast<ImTextureID>(gameSrv.ptr), imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
			}
			else
			{
				ImGui::TextUnformatted("GameRenderTarget SRV is not ready.");
			}
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	Vector2 EditorWindowManager::ConvertScreenToMainViewportPosition(const Vector2& screenPosition) const
	{
		// 入力系はこの入口でスクリーン座標からMain Viewportローカル座標へ変換する
		return { screenPosition.x - mainViewportScreenPosition_.x, screenPosition.y - mainViewportScreenPosition_.y };
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
