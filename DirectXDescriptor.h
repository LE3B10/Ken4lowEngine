#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <wrl.h>

// デスクリプタヒープの生成クラス
class DirectXDescriptor
{
public: // メンバ関数

	// 初期化処理
	void Initialize(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2);

	// DescriptorHeapを生成する
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

	// DepthStencilTextureを生成する
	Microsoft::WRL::ComPtr <ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

	// レンダーターゲットビューの生成
	void GenerateRTV(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2);

	// ゲッター


private: // メンバ関数
	// 指定番号のCPUデスクリプタヒープを取得する
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

	// 指定番号のGPUデスクリプタヒープを取得する
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

private: // メンバ変数
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap;
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> rtvDescriptorHeap;
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> dsvDescriptorHeap;
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> srvDescriptorHeap;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	uint32_t descriptorSizeSRV;
	uint32_t descriptorSizeRTV;
	uint32_t descriptorSizeDSV;

};

