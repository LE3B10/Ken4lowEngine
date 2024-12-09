#pragma once
#include "DX12Include.h"

/// -------------------------------------------------------------
///					DepthStencilDescクラス
/// -------------------------------------------------------------
class DX12DepthStencil
{
public: /// ---------- メンバ関数 ---------- ///

	// DepthStencilの生成
	void Create(bool depthEnable);
	void CreateParticle(bool depthEnable);
	
public: /// ---------- ゲッター ---------- ///

	D3D12_DEPTH_STENCIL_DESC GetDepthStencilDesc();

private: /// ---------- メンバ変数 ---------- ///

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

};

