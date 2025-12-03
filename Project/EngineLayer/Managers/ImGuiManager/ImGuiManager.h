#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <cstdint>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#endif // USE_IMGUI

/// ---------- 前方宣言 ---------- ///
class WinApp;
class DirectXCommon;

/// -------------------------------------------------------------
///						ImGui管理クラス
/// -------------------------------------------------------------
class ImGuiManager
{
public:	/// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ImGuiManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>ImGuiManager の唯一のインスタンス。</returns>
	static ImGuiManager* GetInstance();

	/// <summary>
	/// ImGui の初期化処理を行います。<br/>
	/// ・SRVManager から ImGui 用フォントテクスチャ用の SRV インデックスを確保<br/>
	/// ・ImGui コンテキストの作成<br/>
	/// ・Win32 / DX12 バックエンドの初期化<br/>
	/// を行い、ImGui を使える状態にします。
	/// </summary>
	/// <param name="winApp">ウィンドウハンドル取得に使用する WinApp。</param>
	/// <param name="dxCommon">デバイスやスワップチェーンなどを取得する DirectXCommon。</param>
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon);

	/// <summary>
	/// 1 フレーム分の ImGui の開始処理を行います。<br/>
	/// ・ImGui_ImplDX12_NewFrame()<br/>
	/// ・ImGui_ImplWin32_NewFrame()<br/>
	/// ・ImGui::NewFrame()<br/>
	/// を呼び出し、このあと ImGui ウィジェットの記述が行える状態にします。
	/// </summary>
	void BeginFrame();

	/// <summary>
	/// 1 フレーム分の ImGui の終了処理を行います。<br/>
	/// ・ImGui::Render() を呼び出し、内部の描画コマンドリストを生成します。<br/>
	/// 実際の描画は Draw() で行います。
	/// </summary>
	void EndFrame();

	/// <summary>
	/// ImGui の描画処理を行います。<br/>
	/// ・SRVManager::PreDraw() で SRV ヒープをセット<br/>
	/// ・ImGui_ImplDX12_RenderDrawData() で ImGui の描画コマンドをコマンドリストに積む<br/>
	/// という処理を行います。<br/>
	/// DirectX の通常の描画コマンドの後や、ポストエフェクトの後に呼び出す想定です。
	/// </summary>
	void Draw();

	/// <summary>
	/// ImGui の終了処理を行います。<br/>
	/// ・確保していた SRV インデックスの解放<br/>
	/// ・DX12 / Win32 バックエンドのシャットダウン<br/>
	/// ・ImGui コンテキストの破棄<br/>
	/// を行います。
	/// </summary>
	void Finalize();

private: /// ---------- メンバ関数 ---------- ///

	// SRVIndex確保
	uint32_t srvIndex_ = UINT32_MAX;

private: /// ---------- コンストラクタ・デストラクタ ---------- ///

	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
	/// シングルトンとして利用します。
	/// </summary>
	ImGuiManager() = default;

	/// <summary>
	/// デフォルトデストラクタ。
	/// </summary>
	~ImGuiManager() = default;

	/// <summary>
	/// コピーコンストラクタは使用禁止です。
	/// </summary>
	ImGuiManager(const ImGuiManager&) = delete;

	/// <summary>
	/// 代入演算子は使用禁止です。
	/// </summary>
	const ImGuiManager& operator=(const ImGuiManager&) = delete;
};

