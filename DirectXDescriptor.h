#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <wrl.h>

class DirectXDescriptor
{
public: // メンバ関数

	// 初期化処理
	void Initialize(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2);

private: // メンバ変数


};

