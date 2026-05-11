#include "ImGuiManager.h"

#include "WinApp.h"
#include "DirectXCommon.h"
#include "SRVManager.h"

#include <stdexcept>

#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // USE_IMGUI


namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

	using namespace Microsoft::WRL;

	/// -------------------------------------------------------------
	///					シングルトンインスタンス
	/// -------------------------------------------------------------
	ImGuiManager* ImGuiManager::GetInstance()
	{
		static ImGuiManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///							初期化処理
	/// -------------------------------------------------------------
	void ImGuiManager::Initialize(WinApp* winApp, DirectXCommon* dxCommon)
	{
#ifdef USE_IMGUI
		// SRVの番号を取得
		srvIndex_ = SRVManager::GetInstance()->Allocate();

		if (srvIndex_ >= SRVManager::GetInstance()->GetkMaxSRVCount())
		{
			SRVManager::GetInstance()->Free(srvIndex_);
			srvIndex_ = UINT32_MAX;
			throw std::runtime_error("Failed to allocate SRV for ImGuiManager");
		}

#pragma region ImGuiの初期化を行いDirectX12とWindowsAPIを使ってImGuiをセットアップする
		IMGUI_CHECKVERSION();						  // ImGuiのバージョンチェック
		ImGui::CreateContext();						  // ImGuiコンテキストの作成

		ImGuiIO& io = ImGui::GetIO();				  // ImGuiIOへの参照を取得

		// フォントの設定
		io.Fonts->AddFontFromFileTTF("Resources/Fonts/NotoSansJP-VariableFont_wght.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese()); // 日本語フォントの追加

		// ImGuiウィンドウをエンジンのメインウィンドウ内でドッキングできるようにする
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		// 今回は別OSウィンドウ化を避けるため Multi-Viewport は明示的に無効のままにする
		io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();					  // ImGuiスタイルの設定
		ImGui_ImplWin32_Init(winApp->GetHwnd());	  // Win32バックエンドの初期化
		ImGui_ImplDX12_Init(dxCommon->GetDevice(),	  // DirectX 12バックエンドの初期化
			dxCommon->GetSwapChainDesc().BufferCount,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			SRVManager::GetInstance()->GetDescriptorHeap(),
			SRVManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_),
			SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_));
#pragma endregion
#endif // USE_IMGUI
	}



	/// -------------------------------------------------------------
	///						フレーム開始処理
	/// -------------------------------------------------------------
	void ImGuiManager::BeginFrame()
	{
#ifdef USE_IMGUI
		// ImGuiバックエンドとコンテキストのフレーム開始をManagerに集約する
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		DrawDockSpace();
#endif // USE_IMGUI
	}



	/// -------------------------------------------------------------
	///						DockSpace描画処理
	/// -------------------------------------------------------------
	void ImGuiManager::DrawDockSpace()
	{
#ifdef USE_IMGUI
		// 既存描画の見た目を変えないよう中央ノードは透過したDockSpaceを毎フレーム用意する
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
#endif // USE_IMGUI
	}


	/// -------------------------------------------------------------
	///						フレーム終了処理
	/// -------------------------------------------------------------
	void ImGuiManager::EndFrame()
	{
#ifdef USE_IMGUI
		// ImGuiの内部描画コマンド生成をManagerに集約する
		ImGui::Render();
#endif // USE_IMGUI
	}



	/// -------------------------------------------------------------
	///						描画開始処理
	/// -------------------------------------------------------------
	void ImGuiManager::Draw()
	{
#ifdef USE_IMGUI
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();

		ComPtr<ID3D12GraphicsCommandList> commandList = dxCommon->GetCommandManager()->GetCommandList();

		// ImGui描画に必要なSRVヒープ設定をManagerに集約する
		SRVManager::GetInstance()->PreDraw();

		// 実際のcommandListにImGuiの描画コマンドを積む処理をManagerに集約する
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
#endif // USE_IMGUI
	}



	/// -------------------------------------------------------------
	///					Win32メッセージ処理
	/// -------------------------------------------------------------
	bool ImGuiManager::ProcessWin32Message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
#ifdef USE_IMGUI
		// Win32バックエンド固有のメッセージ処理をManagerに集約する
		return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) != 0;
#else
		(void)hwnd;
		(void)msg;
		(void)wparam;
		(void)lparam;
		return false;
#endif // USE_IMGUI
	}


	/// -------------------------------------------------------------
	///						終了処理
	/// -------------------------------------------------------------
	void ImGuiManager::Finalize()
	{
#ifdef USE_IMGUI
		// SRVが有効であるかを確認
		if (srvIndex_ != UINT32_MAX)
		{
			SRVManager::GetInstance()->Free(srvIndex_);
			srvIndex_ = UINT32_MAX; // 無効な状態にリセット
		}

		// ImGuiバックエンドとコンテキストの終了処理をManagerに集約する
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
#endif // USE_IMGUI
	}

#ifdef USE_IMGUI
	/// -------------------------------------------------------------
	///						デバッグパネル登録
	/// -------------------------------------------------------------
	void ImGuiManager::RegisterPanel(const DebugPanel& panel)
	{
		panels_.push_back(panel);
		if (selectedIndex_ >= (int)panels_.size()) selectedIndex_ = 0;
	}
#endif // USE_IMGUI

	/// -------------------------------------------------------------
	///						デバッグハブ描画
	/// -------------------------------------------------------------
	void ImGuiManager::DrawDebugHub()
	{
#ifdef USE_IMGUI
		if (panels_.empty()) return;

		// 親ウィンドウ
		if (!ImGui::Begin("DebugHub")) { ImGui::End(); return; }

		// 左：一覧 / 右：内容（Docking無しなので Columns）
		ImGui::Columns(2, "hub_cols", true);
		ImGui::SetColumnWidth(0, 200.0f);

		// ---- 左：パネル一覧 ----
		ImGui::BeginChild("##hub_left", ImVec2(0, 0), true);
		for (int i = 0; i < (int)panels_.size(); ++i)
		{
			auto& p = panels_[i];
			if (!p.open) continue;

			bool selected = (selectedIndex_ == i);
			if (ImGui::Selectable(p.name.c_str(), selected))
			{
				selectedIndex_ = i;
			}
		}
		ImGui::EndChild();

		ImGui::NextColumn();

		// ---- 右：選択パネルの中身 ----
		ImGui::BeginChild("##hub_right", ImVec2(0, 0), true);

		auto& cur = panels_[selectedIndex_];
		ImGui::Text("%s", cur.name.c_str());
		ImGui::Separator();

		// ポップアウト/固定オプション（まずはここだけ付けると便利）
		ImGui::Checkbox("Pop out", &cur.popOut);
		ImGui::SameLine();
		ImGui::Checkbox("Pin", &cur.pinned);
		if (cur.pinned)
		{
			ImGui::DragFloat2("Pin Pos", (float*)&cur.pinnedPos, 1.0f);
			ImGui::DragFloat2("Pin Size", (float*)&cur.pinnedSize, 1.0f);
		}
		ImGui::Separator();

		// 中身
		if (!cur.popOut)
		{
			if (cur.drawContent) cur.drawContent();
		}

		ImGui::EndChild();
		ImGui::Columns(1);
		ImGui::End();

		// ---- ポップアウト表示（別ウィンドウ） ----
		for (auto& p : panels_)
		{
			if (!p.open || !p.popOut) continue;

			ImGuiWindowFlags flags = 0;
			if (p.pinned)
			{
				ImGui::SetNextWindowPos(p.pinnedPos, ImGuiCond_Always);
				ImGui::SetNextWindowSize(p.pinnedSize, ImGuiCond_Always);
				flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
			}

			if (ImGui::Begin(p.name.c_str(), &p.open, flags))
			{
				if (p.drawContent) p.drawContent();
			}
			ImGui::End();
		}
#endif // USE_IMGUI

	}

} // namespace Ken4lowEngine
