#pragma once
#include "DirectXDevice.h"
#include "DirectXSwapChain.h"

#include <dxcapi.h>
#include <memory>

// 前方宣言
class WinApp;

// DirectX基盤
class DirectXCommon
{
public:
	// シングルトン
	static DirectXCommon* GetInstance();

	// 初期化処理
	void Initialize(WinApp* winApp);

	// 描画開始・終了処理
	void BeginDraw();
	void EndDraw();

	// 終了処理
	void Finalize();

public: // ゲッター
	ID3D12Device* GetDevice() const;
	ID3D12GraphicsCommandList* GetCommandList() const;

	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

private: // メンバ関数

	// デバッグレイヤーの表示
	void DebugLayer();

	// エラー警告
	void ErrorWarning();

	// コマンド関連の生成
	void CreateCommands();

	// 各種のデスクリプタヒープの生成
	void GenerateDescriptorHeaps();

	// フェンスの生成
	void CreateFenceEvent();

	// ビューポート矩形の初期化
	void InitializeViewport();

	// シザリング矩形の初期化
	void InitializeScissoring();

	// DXCコンパイラの生成
	void CreateDXCCompiler();

	// バリアで書き込み可能に変更
	void ChangeBarrier();

	// 画面全体をクリア
	void ClearWindow();

private: // メンバ変数
	std::unique_ptr<DirectXDevice> device_;
	std::unique_ptr<DirectXSwapChain> swapChain_;

	Microsoft::WRL::ComPtr <ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};


	Microsoft::WRL::ComPtr <ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 fenceValue = 0;
	
	// 描画開始・終了処理に使う
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	UINT backBufferIndex;

private: // メンバ変数

	DirectXCommon() = default;
	~DirectXCommon() = default;

	// コピー禁止
	DirectXCommon(const DirectXCommon&) = delete;
	const DirectXCommon& operator=(const DirectXCommon&) = delete;
};

