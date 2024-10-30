#pragma once
#include "DX12Include.h"

/// -------------------------------------------------------------
///					頂点データを管理するクラス
/// -------------------------------------------------------------
class InputLayoutManager
{
public: /// ---------- メンバ関数 ---------- ///
	InputLayoutManager();
	~InputLayoutManager();

	// レイアウトの設定を行う関数
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	const D3D12_INPUT_LAYOUT_DESC& GetInputLayoutDesc() const;

private: /// ---------- メンバ変数 ---------- ///

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
};

