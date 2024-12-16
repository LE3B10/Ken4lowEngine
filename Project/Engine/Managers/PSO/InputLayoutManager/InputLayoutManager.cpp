#include "InputLayoutManager.h"


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



/// -------------------------------------------------------------
///				InputLayoutの設定を行う処理
/// -------------------------------------------------------------
void InputLayoutManager::Initialize()
{
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);
}

