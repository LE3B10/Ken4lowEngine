#include "ImGuiManager.h"

#include "WinApp.h"
#include "DirectXCommon.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;

ImGuiManager* ImGuiManager::GetInstance()
{
	static ImGuiManager instance;

	return &instance;
}

void ImGuiManager::Initialize(WinApp* winApp, DirectXCommon* dxCommon)
{
#pragma region ImGuiの初期化を行いDirectX12とWindowsAPIを使ってImGuiをセットアップする
	IMGUI_CHECKVERSION();						  // ImGuiのバージョンチェック
	ImGui::CreateContext();						  // ImGuiコンテキストの作成
	ImGui::StyleColorsDark();					  // ImGuiスタイルの設定
	ImGui_ImplWin32_Init(winApp->GetHwnd());	  // Win32バックエンドの初期化
	ImGui_ImplDX12_Init(dxCommon->GetDevice(),	  // DirectX 12バックエンドの初期化
		dxCommon->GetSwapChainDesc().BufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		dxCommon->GetSRVDescriptorHeap(),
		dxCommon->GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		dxCommon->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
#pragma endregion
} 

void ImGuiManager::BeginFrame()
{
#ifdef _DEBUG
	//ImGuiを使う
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif // _DEBUG
}

void ImGuiManager::EndFrame()
{
#ifdef _DEBUG
	//ImGuiの内部コマンドを生成する
	ImGui::Render();
#endif // _DEBUG

}

void ImGuiManager::Draw()
{
#ifdef _DEBUG

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	ComPtr<ID3D12GraphicsCommandList> commandList = dxCommon->GetCommandList();

	/*-----ImGuiを描画する-----*/
	//描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon->GetSRVDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);
	/*-----ImGuiを描画する-----*/

	/*-----ImGuiを描画する-----*/
	//実際のcommandListのImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
#endif // _DEBUG
}

void ImGuiManager::Finalize()
{
#ifdef _DEBUG
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif // _DEBUG

}
