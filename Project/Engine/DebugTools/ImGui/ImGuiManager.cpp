#include "ImGuiManager.h"

#include "WinApp.h"
#include "DirectXCommon.h"
#include "SRVManager.h"

#include <cassert>
#include <fstream>
#include <stdexcept>

#ifdef USE_IMGUI
// DockBuilder APIを使って初期Docking配置を組むため内部ヘッダーを参照する
#include <imgui_internal.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	constexpr const char* kEditorLayoutIniFilename = "ken4low_editor_layout.ini";

	bool FileExists(const char* path)
	{
		if (path == nullptr)
		{
			return false;
		}

		std::ifstream file(path, std::ios::binary);
		return file.good();
	}
}
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
		// 初期化順の不整合を起動時に明示的なエラーとして検出する
		if (initialized_)
		{
			throw std::runtime_error("ImGuiManager is already initialized");
		}
		if (winApp == nullptr || dxCommon == nullptr || dxCommon->GetDevice() == nullptr || dxCommon->GetCommandManager() == nullptr || dxCommon->GetCommandManager()->GetCommandQueue() == nullptr)
		{
			throw std::runtime_error("Invalid arguments for ImGuiManager::Initialize");
		}

		auto* srvManager = SRVManager::GetInstance();
		if (srvManager->GetDescriptorHeap() == nullptr)
		{
			throw std::runtime_error("SRV descriptor heap is not initialized for ImGuiManager");
		}

#pragma region ImGuiの初期化を行いDirectX12とWindowsAPIを使ってImGuiをセットアップする
		IMGUI_CHECKVERSION();						  // ImGuiのバージョンチェック
		ImGui::CreateContext();						  // ImGuiコンテキストの作成

		ImGuiIO& io = ImGui::GetIO();				  // ImGuiIOへの参照を取得

		// 手動Dockingレイアウトを次回起動時に復元するため、専用iniへ保存する
		io.IniFilename = kEditorLayoutIniFilename;
		shouldApplyDefaultDockLayout_ = !FileExists(io.IniFilename);
		resetDockLayoutRequested_ = false;
		dockLayoutInitialized_ = false;

		// フォントの設定
		io.Fonts->AddFontFromFileTTF("Resources/Fonts/NotoSansJP-VariableFont_wght.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese()); // 日本語フォントの追加

		// ImGuiウィンドウをエンジンのメインウィンドウ内でドッキングできるようにする
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		// 今回は別OSウィンドウ化を避けるため Multi-Viewport は明示的に無効のままにする
		io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();					  // ImGuiスタイルの設定
		const bool win32Initialized = ImGui_ImplWin32_Init(winApp->GetHwnd()); // Win32バックエンド初期化の失敗を起動時に検出する
		assert(win32Initialized && "ImGui_ImplWin32_Init failed");
		if (!win32Initialized)
		{
			ImGui::DestroyContext();
			throw std::runtime_error("ImGui_ImplWin32_Init failed");
		}

		ImGui_ImplDX12_InitInfo dx12InitInfo{};
		dx12InitInfo.Device = dxCommon->GetDevice();
		dx12InitInfo.CommandQueue = dxCommon->GetCommandManager()->GetCommandQueue();
		dx12InitInfo.NumFramesInFlight = static_cast<int>(dxCommon->GetSwapChainDesc().BufferCount);
		dx12InitInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		dx12InitInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap();
		dx12InitInfo.SrvDescriptorAllocFn = AllocateImGuiSrvDescriptor;
		dx12InitInfo.SrvDescriptorFreeFn = FreeImGuiSrvDescriptor;
		dx12InitInfo.UserData = this;

		const bool dx12Initialized = ImGui_ImplDX12_Init(&dx12InitInfo); // 1.92以降の動的フォントテクスチャ更新に対応したDX12初期化を行う
		assert(dx12Initialized && "ImGui_ImplDX12_Init failed");
		if (!dx12Initialized)
		{
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			throw std::runtime_error("ImGui_ImplDX12_Init failed");
		}

		initialized_ = true;
#pragma endregion
#endif // USE_IMGUI
	}

#ifdef USE_IMGUI
	/// -------------------------------------------------------------
	///					ImGui用SRVディスクリプタ確保
	/// -------------------------------------------------------------
	void ImGuiManager::AllocateImGuiSrvDescriptor(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
	{
		if (info == nullptr || info->UserData == nullptr || outCpuHandle == nullptr || outGpuHandle == nullptr)
		{
			throw std::runtime_error("Invalid ImGui DX12 SRV allocation request");
		}

		auto* self = static_cast<ImGuiManager*>(info->UserData);
		auto* srvManager = SRVManager::GetInstance();
		const uint32_t srvIndex = srvManager->Allocate();
		*outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
		*outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);

		// フォントを含むImGuiテクスチャ用SRVが有効なヒープ上に確保されたことを記録する
		assert(outCpuHandle->ptr != 0 && outGpuHandle->ptr != 0);
		self->imguiSrvHandleToIndex_[outCpuHandle->ptr] = srvIndex;
	}

	/// -------------------------------------------------------------
	///					ImGui用SRVディスクリプタ解放
	/// -------------------------------------------------------------
	void ImGuiManager::FreeImGuiSrvDescriptor(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
	{
		if (info == nullptr || info->UserData == nullptr || cpuHandle.ptr == 0 || gpuHandle.ptr == 0)
		{
			throw std::runtime_error("Invalid ImGui DX12 SRV free request");
		}

		auto* self = static_cast<ImGuiManager*>(info->UserData);
		auto it = self->imguiSrvHandleToIndex_.find(cpuHandle.ptr);
		if (it == self->imguiSrvHandleToIndex_.end())
		{
			throw std::runtime_error("Unknown ImGui DX12 SRV descriptor handle");
		}

		// ImGuiバックエンドから返却されたフォント/テクスチャ用SRVをSRVManagerへ戻す
		SRVManager::GetInstance()->Free(it->second);
		self->imguiSrvHandleToIndex_.erase(it);
	}
#endif // USE_IMGUI




	/// -------------------------------------------------------------
	///						フレーム開始処理
	/// -------------------------------------------------------------
	void ImGuiManager::BeginFrame()
	{
#ifdef USE_IMGUI
		// DX12/Win32バックエンド初期化前のNewFrame呼び出しを防ぐ
		if (!initialized_)
		{
			throw std::runtime_error("ImGuiManager::BeginFrame called before Initialize");
		}

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
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		// 既存描画の見た目を変えないよう中央ノードは透過したDockSpaceを毎フレーム用意する
		const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

		if (shouldApplyDefaultDockLayout_ || resetDockLayoutRequested_)
		{
			// 保存済みiniが無い初回起動、または明示リセット時だけDockBuilderで初期配置を作る
			SetupDefaultDockLayout(dockspaceId, viewport);
			dockLayoutInitialized_ = true;
			shouldApplyDefaultDockLayout_ = false;
			resetDockLayoutRequested_ = false;
		}
#endif // USE_IMGUI
	}

#ifdef USE_IMGUI
	void ImGuiManager::SetupDefaultDockLayout(ImGuiID dockspaceId, const ImGuiViewport* viewport)
	{
		if (dockspaceId == 0 || viewport == nullptr)
		{
			return;
		}

		ImGuiID rootNode = dockspaceId;
		ImGuiID rightNode = 0;
		ImGui::DockBuilderRemoveNodeChildNodes(rootNode);
		ImGui::DockBuilderSetNodeSize(rootNode, viewport->Size);
		ImGui::DockBuilderSplitNode(rootNode, ImGuiDir_Right, 0.28f, &rightNode, &rootNode);

		// Main Viewportを中央Dockに固定名で配置してゲーム画面と各Dockウィンドウの重なりを避ける
		ImGui::DockBuilderDockWindow("Main Viewport", rootNode);
		ImGui::DockBuilderDockWindow("Details", rightNode);
		ImGui::DockBuilderDockWindow("Post Effect Settings", rightNode);
		ImGui::DockBuilderDockWindow("Light Editor", rightNode);
		ImGui::DockBuilderFinish(dockspaceId);
	}
#endif // USE_IMGUI


	/// -------------------------------------------------------------
	///					Dockingレイアウトリセット要求
	/// -------------------------------------------------------------
	void ImGuiManager::RequestResetDockLayout()
	{
#ifdef USE_IMGUI
		// Windowメニューからの操作で次フレームに初期DockBuilder配置を再適用する
		resetDockLayoutRequested_ = true;
		shouldApplyDefaultDockLayout_ = false;
		dockLayoutInitialized_ = false;
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
		if (!initialized_)
		{
			return;
		}

		// DestroyContext前にiniへ書き出して、終了直前のDockingレイアウトを確実に残す
		if (ImGui::GetIO().IniFilename != nullptr)
		{
			ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
		}

		// ImGuiバックエンドとコンテキストの終了処理をManagerに集約する
		ImGui_ImplDX12_Shutdown();
		// DX12バックエンドから返却されなかったSRVがあればFinalize時に回収する
		for (const auto& srvEntry : imguiSrvHandleToIndex_)
		{
			SRVManager::GetInstance()->Free(srvEntry.second);
		}
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		imguiSrvHandleToIndex_.clear();
		initialized_ = false;
		dockLayoutInitialized_ = false;
		shouldApplyDefaultDockLayout_ = false;
		resetDockLayoutRequested_ = false;
#endif // USE_IMGUI
	}


} // namespace Ken4lowEngine
