#include "InputLayoutManager.h"



/// -------------------------------------------------------------
///				InputLayoutの設定を行う処理
/// -------------------------------------------------------------
void InputLayoutManager::SettingInputLayout()
{
	#pragma region InputLayoutの設定を行う・InputLayoutの拡張
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);
#pragma endregion
}



/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
D3D12_INPUT_LAYOUT_DESC& InputLayoutManager::GetInputLayoutDesc()
{
	// TODO: return ステートメントをここに挿入します
	return inputLayoutDesc;
}
