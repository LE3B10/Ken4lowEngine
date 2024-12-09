#include "RasterizerStateManager.h"


/// -------------------------------------------------------------
///							設定処理
/// -------------------------------------------------------------
void RasterizerStateManager::Initialize()
{
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴンを塗りつぶす
	//rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // 裏面カリングを有効にする
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // 裏面カリングを無効にする
	rasterizerDesc.FrontCounterClockwise = FALSE;	 // 時計回りの面を表面とする（カリング方向の設定）
}

void RasterizerStateManager::InitializeParticle()
{
	//裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
}



/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
D3D12_RASTERIZER_DESC RasterizerStateManager::GetRasterizerDesc() const
{
	return rasterizerDesc;
}
