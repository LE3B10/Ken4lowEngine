#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///					デバッグパネル構造体
/// -------------------------------------------------------------
struct DebugPanel
{
	std::string name;
	bool open = true;        // 一覧に出すか/表示するか
	bool popOut = false;     // 別ウィンドウ化（固定したい時）
	bool pinned = false;     // ポップアウト時に位置固定
	ImVec2 pinnedPos{ 20,20 };
	ImVec2 pinnedSize{ 380,240 };

	std::function<void()> drawContent; // ★ Begin/Endしない「中身だけ描く」関数
};
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

#ifdef USE_IMGUI
	/// <summary>
	/// デバッグパネルを登録します。
	/// </summary>
	/// <param name="panel">登録するデバッグパネル。</param>
	void RegisterPanel(const DebugPanel& panel);
#endif // USE_IMGUI

	/// <summary>
	/// デバッグハブウィンドウの描画処理を行います。
	/// </summary>
	void DrawDebugHub(); // 親ウィンドウ（左メニュー＋右編集）

private: /// ---------- メンバ関数 ---------- ///

	// SRVIndex確保
	uint32_t srvIndex_ = UINT32_MAX;

#ifdef USE_IMGUI
	std::vector<DebugPanel> panels_;
#endif // USE_IMGUI

	int selectedIndex_ = 0;

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


} // namespace Ken4lowEngine
