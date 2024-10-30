#include "InputLayoutManager.h"



/// -------------------------------------------------------------
///				InputLayoutの設定を行う処理
/// -------------------------------------------------------------
void InputLayoutManager::Initialize()
{
	/*----------------------------------------------------------------------------------------
	* 
	*		SemanticName - SemanticIndex - Format - AlignedByteOffset - InputSlotClass
	*
	-----------------------------------------------------------------------------------------*/
	inputElementDescs[0] = { "POSITION" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT , 0 };
	inputElementDescs[1] = { "TEXCOORD" , 0, DXGI_FORMAT_R32G32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT , 0 };
	inputElementDescs[2] = { "NORMAL" , 0, DXGI_FORMAT_R32G32B32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT , 0 };

	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);
#pragma endregion
}



/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
const D3D12_INPUT_LAYOUT_DESC& InputLayoutManager::GetInputLayoutDesc() const
{
	// TODO: return ステートメントをここに挿入します
	return inputLayoutDesc;
}
