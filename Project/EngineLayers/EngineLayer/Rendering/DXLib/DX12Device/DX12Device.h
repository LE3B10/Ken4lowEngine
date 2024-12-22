#pragma once
#include "DX12Include.h"


/// -------------------------------------------------------------
///					デバイスの生成クラス
/// -------------------------------------------------------------
class DX12Device
{
public:
	/// ---------- メンバ関数 ---------- ///

	// デバイスの初期化
	void Initialize();

	// ゲッター
	ID3D12Device* GetDevice() const;
	IDXGIFactory7* GetDXGIFactory() const;

private:
	/// ---------- メンバ変数 ---------- ///

	ComPtr <ID3D12Device> device;
	ComPtr <IDXGIFactory7> dxgiFactory;
	ComPtr <IDXGIAdapter4> useAdapter;
};

