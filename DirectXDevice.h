#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class DirectXDevice
{
public: // メンバ関数

	// デバイスの初期化
	void Initialize();

	// ゲッター
	ID3D12Device* GetDevice() const;
	IDXGIFactory7* GetDXGIFactory() const;

private: // メンバ変数
	Microsoft::WRL::ComPtr <ID3D12Device> device;
	Microsoft::WRL::ComPtr <IDXGIFactory7> dxgiFactory;
	Microsoft::WRL::ComPtr <IDXGIAdapter4> useAdapter;
};

