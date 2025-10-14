#pragma once
#include "DX12Include.h"


/// -------------------------------------------------------------
///					デバイスの生成クラス
/// -------------------------------------------------------------
class DX12Device
{
public: /// ---------- メンバ関数 ---------- ///

	// デバイスの初期化
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	// デバイスの取得
	ID3D12Device* GetDevice() const { return device.Get(); }

	// DXGIファクトリの取得
	IDXGIFactory7* GetDXGIFactory() const { return dxgiFactory.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	ComPtr <ID3D12Device> device;
	ComPtr <IDXGIFactory7> dxgiFactory;
	ComPtr <IDXGIAdapter4> useAdapter;
};

