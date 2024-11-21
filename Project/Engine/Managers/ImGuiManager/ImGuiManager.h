#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

/// ---------- 前方宣言 ---------- ///
class WinApp;
class DirectXCommon;
class SRVManager;

/// -------------------------------------------------------------
///						ImGui管理クラス
/// -------------------------------------------------------------
class ImGuiManager
{
public:	/// ---------- メンバ関数 ---------- ///

	// シングルトン
	static ImGuiManager* GetInstance();

	// 初期化処理
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon);

	// 開始処理
	void BeginFrame();
	
	// 終了処理
	void EndFrame();
	
	// 描画処理
	void Draw();

	// 終了処理
	void Finalize();

private: /// ---------- メンバ関数 ---------- ///

	SRVManager* srvManager_ = nullptr;

	ImGuiManager() = default;
	~ImGuiManager() = default;

	// コピー禁止
	ImGuiManager(const ImGuiManager&) = delete;
	const ImGuiManager& operator=(const ImGuiManager&) = delete;
};

