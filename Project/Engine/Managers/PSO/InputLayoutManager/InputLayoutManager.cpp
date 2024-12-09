#include "InputLayoutManager.h"


/// -------------------------------------------------------------
///				InputLayoutの設定を行う処理
/// -------------------------------------------------------------
void InputLayoutManager::Initialize()
{
	/*------------------------------------------------------------------------------------------------------------------------------
	*
	*	1 : SemanticName		 - 頂点シェーダーで使用されるセマンティック名
	*	2 : SemanticIndex		 - 同じセマンティック名を持つ要素が複数ある場合に使用されるインデックス
	*	3 : Format				 - データ形式
	*	4 : InputSlot			 - 入力スロット。データが複数のバッファから供給される場合にスロット番号で指定する
	*	5 : AlignedByteOffset	 - バイト単位でのオフセット
	*	6 : InputSlotClass		 - 頂点データかインスタンスデータかを指定
	*	7 : InstanceDataStepRate - インスタンスデータを使用する場合、ステップごとにインクリメントされるインスタンス数
	*
	-------------------------------------------------------------------------------------------------------------------------------*/

	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);
}

void InputLayoutManager::InitializeParticle()
{
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
}


/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
D3D12_INPUT_LAYOUT_DESC InputLayoutManager::GetInputLayoutDesc()
{
	// TODO: return ステートメントをここに挿入します
	return inputLayoutDesc;
}
