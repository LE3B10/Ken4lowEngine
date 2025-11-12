#include "ImGuiManager.h"

#include "WinApp.h"
#include "DirectXCommon.h"
#include "SRVManager.h"

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
		throw std::runtime_error("Failed to allocate SRV for ImGuiManager");
	}

#pragma region ImGuiの初期化を行いDirectX12とWindowsAPIを使ってImGuiをセットアップする
	IMGUI_CHECKVERSION();						  // ImGuiのバージョンチェック
	ImGui::CreateContext();						  // ImGuiコンテキストの作成
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
	//ImGuiを使う
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif // USE_IMGUI
}



/// -------------------------------------------------------------
///						フレーム終了処理
/// -------------------------------------------------------------
void ImGuiManager::EndFrame()
{
#ifdef USE_IMGUI
	//ImGuiの内部コマンドを生成する
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

	/*-----ImGuiを描画する-----*/
	SRVManager::GetInstance()->PreDraw();

	/*-----ImGuiを描画する-----*/
	//実際のcommandListのImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
#endif // USE_IMGUI
}



/// -------------------------------------------------------------
///							終了処理
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

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif // USE_IMGUI
}
