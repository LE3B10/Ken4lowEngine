#include "DX12DepthStencil.h"


/// -------------------------------------------------------------
///					DepthStencilの生成
/// -------------------------------------------------------------
void DX12DepthStencil::Create(bool depthEnable)
{
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = depthEnable;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

void DX12DepthStencil::CreateParticle(bool depthEnable)
{
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = depthEnable;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//Depthを描くのをやめる
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
}

D3D12_DEPTH_STENCIL_DESC DX12DepthStencil::GetDepthStencilDesc()
{
	return depthStencilDesc;
}
