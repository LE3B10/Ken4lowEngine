#pragma once
#include "DX12Include.h"
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "FPSCounter.h"
#include "RTVManager.h"
#include "DSVManager.h"
#include "DXCCompilerManager.h"
#include "DX12CommandManager.h"
#include "DX12FenceManager.h"

#include <dxcapi.h>
#include <memory>
#include <Vector4.h>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class WinApp;


/// -------------------------------------------------------------
///			DirectXCommon - DirectX12の基盤クラス
/// -------------------------------------------------------------
class DirectXCommon
{
	// クライアント領域サイズ :	幅
	uint32_t kClientWidth = 0;

	// クライアント領域サイズ : 高さ
	uint32_t kClientHeight = 0;

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DirectXCommon のシングルトンインスタンスを取得します。<br/>
	/// 初回呼び出し時に内部で静的インスタンスを生成します。
	/// </summary>
	/// <returns>DirectXCommon の唯一のインスタンス。</returns>
	static DirectXCommon* GetInstance();

	/// <summary>
	/// DirectX12 の初期化処理を行います。<br/>
	/// おおまかな流れ：<br/>
	/// 1. DX12Device / DX12SwapChain / DXCCompilerManager / DX12CommandManager / DX12FenceManager の生成<br/>
	/// 2. デバッグレイヤー有効化(DebugLayer)<br/>
	/// 3. デバイス生成(DX12Device::Initialize)<br/>
	/// 4. エラー・警告時にブレークする InfoQueue 設定(ErrorWarning)<br/>
	/// 5. コマンド関連の初期化(DX12CommandManager::Initialize, SetFenceManager)<br/>
	/// 6. スワップチェイン生成(DX12SwapChain::Initialize)<br/>
	/// 7. フェンス生成(DX12FenceManager::Initialize)<br/>
	/// 8. DXC コンパイラの初期化(DXCCompilerManager::Initialize)<br/>
	/// 9. RTV / DSV の初期化と深度バッファ作成(InitializeRTVAndDSV)<br/>
	/// 10. ビューポート・シザー矩形の設定<br/>
	/// </summary>
	/// <param name="winApp">ウィンドウハンドルを持つ WinApp インスタンス。</param>
	/// <param name="Width">クライアント領域の幅。</param>
	/// <param name="Height">クライアント領域の高さ。</param>
	void Initialize(WinApp* winApp, uint32_t Width, uint32_t Height);

	/// <summary>
	/// 1 フレーム分の描画開始処理を行います。<br/>
	/// ・FPSCounter::StartFrame() を呼んでフレーム開始を記録<br/>
	/// ・ビューポート／シザー矩形のセット<br/>
	/// ・現在のバックバッファインデックス取得<br/>
	/// ・バックバッファを PRESENT → RENDER_TARGET にリソース遷移<br/>
	/// ・深度バッファを DEPTH_WRITE → PIXEL_SHADER_RESOURCE に遷移<br/>
	/// ・カラー／深度ステンシルのクリア(ClearWindow)<br/>
	/// といった処理をまとめて行います。
	/// </summary>
	void BeginDraw();

	/// <summary>
	/// 1 フレーム分の描画終了処理を行います。<br/>
	/// ・バックバッファを RENDER_TARGET → PRESENT にリソース遷移<br/>
	/// ・コマンドリストの実行＆完了待ち(ExecuteAndWait + Fence Signal / Wait)<br/>
	/// ・スワップチェインの Present (VSync 有効 / Present(1,0))<br/>
	/// ・FPSCounter::EndFrame() による FPS 計測・スリープ制御<br/>
	/// を行います。
	/// </summary>
	void EndDraw();

	/// <summary>
	/// DirectXCommon の終了処理を行います。<br/>
	/// ・フェンスで GPU の処理完了を待機<br/>
	/// ・フェンスマネージャの Finalize<br/>
	/// ・デバイス / スワップチェイン等のユニークポインタ解放<br/>
	/// を行います。アプリ終了時に呼び出します。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 指定リソースのステートを変更するヘルパー。<br/>
	/// 内部的には DX12CommandManager::ResourceTransition() を呼び出し、<br/>
	/// D3D12_RESOURCE_BARRIER を設定してコマンドリストに積みます。
	/// </summary>
	/// <param name="resource">ステートを変更したいリソース。</param>
	/// <param name="stateBefore">変更前のステート。</param>
	/// <param name="stateAfter">変更後のステート。</param>
	void ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
	{
		commandManager_->ResourceTransition(resource, stateBefore, stateAfter);
	}

	void Resize(uint32_t width, uint32_t height);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// DirectX12 デバイスを取得します。
	/// </summary>
	/// <returns>内部で生成された ID3D12Device。</returns>
	ID3D12Device* GetDevice() const { return device_->GetDevice(); }

	/// <summary>
	/// スワップチェインマネージャを取得します。
	/// </summary>
	/// <returns>DX12SwapChain インスタンス。</returns>
	DX12SwapChain* GetSwapChain() { return swapChain_.get(); }

	/// <summary>
	/// DXC コンパイラマネージャを取得します。<br/>
	/// ShaderCompiler から DXC のユーティリティ／コンパイラ／インクルードハンドラにアクセスする際に使用します。
	/// </summary>
	DXCCompilerManager* GetDXCCompilerManager() { return dxcCompilerManager_.get(); }

	/// <summary>
	/// コマンドマネージャを取得します。<br/>
	/// コマンドリストやコマンドキューへのアクセスに使用します。
	/// </summary>
	DX12CommandManager* GetCommandManager() { return commandManager_.get(); }

	/// <summary>
	/// フェンスマネージャを取得します。
	/// </summary>
	DX12FenceManager* GetFenceManager() { return fenceManager_.get(); }

	/// <summary>
	/// スワップチェインの設定情報(DXGI_SWAP_CHAIN_DESC1)を取得します。
	/// </summary>
	/// <returns>内部で保持している DXGI_SWAP_CHAIN_DESC1 への参照。</returns>
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const { return swapChain_->GetSwapChainDesc(); }

	/// <summary>
	/// FPS 計測用の FPSCounter を取得します。<br/>
	/// 外部で FPS や DeltaTime を参照したい場合に使用します。
	/// </summary>
	/// <returns>内部で保持している FPSCounter への参照。</returns>
	FPSCounter& GetFPSCounter() { return fpsCounter_; }

	/// <summary>
	/// 指定インデックスのバックバッファリソースを取得します。<br/>
	/// 0～(バックバッファ数-1) の範囲で指定します。
	/// </summary>
	/// <param name="index">取得したいバックバッファのインデックス。</param>
	/// <returns>バックバッファの ID3D12Resource を保持した ComPtr。</returns>
	ComPtr<ID3D12Resource> GetBackBuffer(uint32_t index);

	/// <summary>
	/// 指定インデックスのバックバッファ RTV ハンドルを取得します。<br/>
	/// RTVManager 経由で CPU ディスクリプタハンドルを返します。
	/// </summary>
	/// <param name="index">バックバッファインデックス。</param>
	/// <returns>バックバッファに対応する RTV の CPU ディスクリプタハンドル。</returns>
	D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) { return RTVManager::GetInstance()->GetCPUDescriptorHandle(index); }

	/// <summary>
	/// 深度ステンシルバッファリソースを取得します。
	/// </summary>
	/// <returns>深度ステンシル用の ID3D12Resource を保持した ComPtr。</returns>
	ComPtr<ID3D12Resource> GetDepthStencilResource() const { return depthStencilResource.Get(); }

	uint32_t GetClientWidth()  const { return kClientWidth; }

	uint32_t GetClientHeight() const { return kClientHeight; }

private: /// ---------- メンバ関数 ---------- ///

	// デバッグレイヤーの表示
	void DebugLayer();

	// エラー警告
	void ErrorWarning();

	// 画面全体をクリア
	void ClearWindow();

	// RTVとDSVの初期化関数を追加
	void InitializeRTVAndDSV();

private: /// ---------- メンバ変数 ---------- ///

	FPSCounter fpsCounter_;

	std::unique_ptr<DX12Device> device_;
	std::unique_ptr<DX12SwapChain> swapChain_;
	std::unique_ptr<DXCCompilerManager> dxcCompilerManager_;
	std::unique_ptr<DX12CommandManager> commandManager_;
	std::unique_ptr<DX12FenceManager> fenceManager_;

	D3D12_RESOURCE_BARRIER barrier{};

	// 描画開始・終了処理に使う
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	UINT backBufferIndex = 0;
	uint32_t dsvIndex_ = 0; // DSVのインデックス

	ComPtr<ID3D12Resource> depthStencilResource; // 深度バッファ

private: /// ---------- コピー禁止 ---------- ///

	DirectXCommon() = default;
	~DirectXCommon() = default;
	DirectXCommon(const DirectXCommon&) = delete;
	const DirectXCommon& operator=(const DirectXCommon&) = delete;
};

} // namespace Ken4lowEngine
