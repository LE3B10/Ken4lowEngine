#include "RasterizerStateManager.h"


/// -------------------------------------------------------------
///							設定処理
/// -------------------------------------------------------------
void RasterizerStateManager::SettingProcess()
{
#pragma region RasterizerStateの設定を行う
	//裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
#pragma endregion

}



/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
D3D12_RASTERIZER_DESC RasterizerStateManager::GetRasterizerDesc() const
{
	return rasterizerDesc;
}
