#pragma once
#include "DX12Include.h"

/// -------------------------------------------------------------
///					頂点データを管理するクラス
/// -------------------------------------------------------------
class InputLayoutManager
{
public:
	/// ---------- メンバ関数 ---------- ///

	// InputLayoutの設定を行う処理
	void SettingInputLayout();


	/// ---------- ゲッター ---------- ///

	D3D12_INPUT_LAYOUT_DESC& GetInputLayoutDesc();

private:
	/// ---------- メンバ変数 ---------- ///

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
};

