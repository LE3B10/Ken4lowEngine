#pragma once
#include "DirectXInclude.h"

#include <cassert>


class DirectXDevice
{
public:
	// Microsoft::WRL省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	///----- メンバ関数 -----///
	
	// 初期化処理
	void Initialize();

	///----- アクセッサ -----///
	
	// ゲッター
	ID3D12Device* GetDevice() const { return device_.Get(); }
	IDXGIFactory7* GetDxgiFactory() const { return dxgiFactory_.Get(); }

private:
	///----- メンバ変数	-----///

	ComPtr<ID3D12Device> device_;
	ComPtr<IDXGIFactory7> dxgiFactory_;
	ComPtr<IDXGIAdapter4> useAdapter_;

};

